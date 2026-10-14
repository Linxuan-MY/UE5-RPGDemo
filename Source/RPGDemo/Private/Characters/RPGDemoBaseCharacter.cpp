// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/RPGDemoBaseCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "MotionWarpingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/RPGDemoCombatLog.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "RPGDemoGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ARPGDemoBaseCharacter::ARPGDemoBaseCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    GetMesh()->bReceivesDecals = false;
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

UAbilitySystemComponent* ARPGDemoBaseCharacter::GetAbilitySystemComponent() const
{
    return RPGDemoAbilitySystemComponent;
}

UPawnCombatComponent* ARPGDemoBaseCharacter::GetPawnCombatComponent() const
{
    return nullptr;
}

UPawnUIComponent* ARPGDemoBaseCharacter::GetPawnUIComponent() const
{
    return nullptr;
}

void ARPGDemoBaseCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        // A dedicated server never renders the mesh. Socket-attached damage
        // boxes still need the evaluated bones, not just an advancing montage.
        GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (RPGDemoCombat::IsDebugEnabled())
        {
            UE_LOG(LogRPGDemoCombat, Log, TEXT("AuthorityPose actor=%s netMode=%d refreshBones=1"), *GetName(), GetNetMode());
        }
    }
    if (RPGDemoAbilitySystemComponent && RPGDemoAbilitySystemComponent->GetAvatarActor() != this)
    {
        RPGDemoAbilitySystemComponent->InitAbilityActorInfo(RPGDemoAbilitySystemComponent->GetOwner(), this);
    }
    if (DeathSnapshot.bStarted) OnRep_DeathSnapshot();
}

void ARPGDemoBaseCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (RPGDemoAbilitySystemComponent)
    {
        RPGDemoAbilitySystemComponent->InitAbilityActorInfo(RPGDemoAbilitySystemComponent->GetOwner(), this);
        ensureMsgf(!CharacterStartUpData.IsNull(), TEXT("CharacterStartUpData is null for %s"), *GetName());
    }
}

void ARPGDemoBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARPGDemoBaseCharacter, DeathSnapshot);
}

double ARPGDemoBaseCharacter::GetPresentationServerTime() const
{
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

UAnimMontage* ARPGDemoBaseCharacter::SelectDeathMontage() const
{
    TArray<UAnimMontage*> Candidates;
    for (UAnimMontage* Montage : DeathMontages) if (Montage) Candidates.Add(Montage);
    // Compatibility for existing ability data, including projects upgraded before their character defaults are saved.
    if (Candidates.IsEmpty() && RPGDemoAbilitySystemComponent)
        for (const FGameplayAbilitySpec& Spec : RPGDemoAbilitySystemComponent->GetActivatableAbilities())
        {
            if (!Spec.Ability || !Spec.Ability->GetAssetTags().HasTagExact(RPGDemoGameplayTags::Shared_Ability_Death)) continue;
            for (TFieldIterator<FArrayProperty> It(Spec.Ability->GetClass()); It; ++It)
            {
                FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(It->Inner);
                if (!Inner || !Inner->PropertyClass->IsChildOf(UAnimMontage::StaticClass())) continue;
                FScriptArrayHelper Array(*It, It->ContainerPtrToValuePtr<void>(Spec.Ability));
                for (int32 Index = 0; Index < Array.Num(); ++Index)
                    if (UAnimMontage* Montage = Cast<UAnimMontage>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index)))) Candidates.Add(Montage);
            }
            break;
        }
    return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandHelper(Candidates.Num())];
}

void ARPGDemoBaseCharacter::BeginCharacterDeath()
{
    if (!HasAuthority() || DeathSnapshot.bStarted) return;
    DeathSnapshot.Montage = SelectDeathMontage();
    DeathSnapshot.StartServerTime = GetPresentationServerTime();
    DeathSnapshot.Duration = DeathSnapshot.Montage
        ? DeathSnapshot.Montage->GetPlayLength() / FMath::Max(FMath::Abs(DeathSnapshot.Montage->RateScale), 0.01f) : 0.f;
    DeathSnapshot.bStarted = true;
    if (RPGDemoAbilitySystemComponent && RPGDemoAbilitySystemComponent->GetAvatarActor() == this)
    {
        FGameplayTagContainer ExceptDeath(RPGDemoGameplayTags::Shared_Ability_Death);
        RPGDemoAbilitySystemComponent->CancelAbilities(nullptr, &ExceptDeath);
    }
    OnRep_DeathSnapshot();
    ForceNetUpdate();
}

void ARPGDemoBaseCharacter::OnRep_DeathSnapshot()
{
    if (!DeathSnapshot.bStarted) return;
    TInlineComponentArray<UTimelineComponent*> Timelines(this);
    for (UTimelineComponent* Timeline : Timelines) Timeline->Stop();
    if (UPawnCombatComponent* Combat = GetPawnCombatComponent()) Combat->ResetDamageCollision();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->StopActiveMovement();
    // Allow the existing montage root motion until the final pose is captured.
    if (!bDeathPoseHeld) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    UpdateDeathPresentation();
    if (!bDeathPoseHeld) GetWorldTimerManager().SetTimer(DeathPoseTimer, this, &ThisClass::UpdateDeathPresentation, 0.016f, true);
}

void ARPGDemoBaseCharacter::UpdateDeathPresentation()
{
    if (!DeathSnapshot.bStarted || bDeathPoseHeld) return;
    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    const double Elapsed = FMath::Max(0.0, GetPresentationServerTime() - DeathSnapshot.StartServerTime);
    if (DeathSnapshot.Montage && !Anim) return; // Mesh/AnimBP may not be ready when replication arrives.
    if (!bLocalDeathStarted || (Anim && DeathSnapshot.Montage && !Anim->GetActiveInstanceForMontage(DeathSnapshot.Montage)))
    {
        bLocalDeathStarted = true;
        GetMesh()->bPauseAnims = false;
        if (Anim && DeathSnapshot.Montage)
        {
            Anim->Montage_Stop(0.f);
            Anim->Montage_Play(DeathSnapshot.Montage, 1.f, EMontagePlayReturnType::MontageLength,
                FMath::Min(static_cast<float>(Elapsed) * FMath::Abs(DeathSnapshot.Montage->RateScale), DeathSnapshot.Montage->GetPlayLength() - 0.001f));
            if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(DeathSnapshot.Montage)) Instance->bEnableAutoBlendOut = false;
        }
    }
    if (Elapsed < DeathSnapshot.Duration) return;
    if (Anim && DeathSnapshot.Montage)
    {
        Anim->Montage_SetPosition(DeathSnapshot.Montage, FMath::Max(0.f, DeathSnapshot.Montage->GetPlayLength() - 0.001f));
        Anim->Montage_Pause(DeathSnapshot.Montage);
        GetMesh()->TickAnimation(0.f, false);
        GetMesh()->RefreshBoneTransforms();
    }
    GetMesh()->bPauseAnims = true;
    GetCharacterMovement()->DisableMovement();
    bDeathPoseHeld = true;
    GetWorldTimerManager().ClearTimer(DeathPoseTimer);
    OnDeathPoseHeld();
}

void ARPGDemoBaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(DeathPoseTimer);
    Super::EndPlay(EndPlayReason);
}

void ARPGDemoBaseCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();
    if (RPGDemoAbilitySystemComponent && GetController())
    {
        AActor* AbilityOwner = RPGDemoAbilitySystemComponent->GetOwnerActor();
        if (!AbilityOwner || AbilityOwner == this)
        {
            RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
        }
    }
}
