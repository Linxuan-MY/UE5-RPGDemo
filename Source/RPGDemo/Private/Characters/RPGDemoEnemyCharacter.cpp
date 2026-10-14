// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/RPGDemoEnemyCharacter.h"
#include "AIController.h"
#include "Controllers/RPGDemoAIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Components/Combat/EnemyCombatComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "DataAssets/StartUpData/DataAsset_EnemyStartUpData.h"
#include "Components/UI/EnemyUIComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextBlock.h"
#include "Widgets/RPGDemoWidgetBase.h"
#include "Components/BoxComponent.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "Items/PickUps/RPGDemoStoneBase.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

#include "RPGDemoDebugHelper.h"
#include "Combat/RPGDemoCombatLog.h"

ARPGDemoEnemyCharacter::ARPGDemoEnemyCharacter()
{
	RPGDemoAbilitySystemComponent = CreateDefaultSubobject<URPGDemoAbilitySystemComponent>(TEXT("RPGDemoAbilitySystemComponent"));
	RPGDemoAbilitySystemComponent->SetIsReplicated(true);
	RPGDemoAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	RPGDemoAttributeSet = CreateDefaultSubobject<URPGDemoAttributeSet>(TEXT("RPGDemoAttributeSet"));
	bReplicates = true;
	SetReplicateMovement(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;

	EnemyCombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("EnemyCombatComponent"));

	EnemyUIComponent = CreateDefaultSubobject<UEnemyUIComponent>(TEXT("EnemyUIComponent"));

	EnemyHealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHealthWidgetComponent"));
	EnemyHealthWidgetComponent->SetupAttachment(GetMesh());

	LeftHandCollisionBox = CreateDefaultSubobject<UBoxComponent>("LeftHandCollisionBox");
	LeftHandCollisionBox->SetupAttachment(GetMesh());
	LeftHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

	RightHandCollisionBox = CreateDefaultSubobject<UBoxComponent>("RightHandCollisionBox");
	RightHandCollisionBox->SetupAttachment(GetMesh());
	RightHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);
}

UPawnCombatComponent* ARPGDemoEnemyCharacter::GetPawnCombatComponent() const
{
	return EnemyCombatComponent;
}

UPawnUIComponent* ARPGDemoEnemyCharacter::GetPawnUIComponent() const
{
	return EnemyUIComponent;
}

UEnemyUIComponent* ARPGDemoEnemyCharacter::GetEnemyUIComponent() const
{
	return EnemyUIComponent;
}

void ARPGDemoEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
    // The editor property hook does not run in a cooked server executable.
    if (!LeftHandCollisionBoxAttachBoneName.IsNone())
        LeftHandCollisionBox->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, LeftHandCollisionBoxAttachBoneName);
    if (!RightHandCollisionBoxAttachBoneName.IsNone())
        RightHandCollisionBox->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, RightHandCollisionBoxAttachBoneName);

    if (GetNetMode() != NM_DedicatedServer && BossHealthWidgetClass && !IsDeathPresentationActive())
    {
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (APlayerController* PC = It->Get(); PC && PC->IsLocalController())
                if (URPGDemoWidgetBase* BossWidget = CreateWidget<URPGDemoWidgetBase>(PC, BossHealthWidgetClass))
                {
                    if (UTextBlock* NameText = Cast<UTextBlock>(BossWidget->GetWidgetFromName(TEXT("TextBlock_BossName")))) NameText->SetText(BossDisplayName);
                    BossWidget->AddToPlayerScreen();
                    BossWidget->InitEnemyCreatedWidget(this);
                    EnemyUIComponent->RegisterEnemyDrawnWidget(BossWidget);
                }
    }
	if (GetNetMode() != NM_DedicatedServer)
    if (URPGDemoWidgetBase* HealthWidget = Cast<URPGDemoWidgetBase>(EnemyHealthWidgetComponent->GetUserWidgetObject()))
	{
		HealthWidget->InitEnemyCreatedWidget(this);
	}

	if (!HasAuthority())
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
		return;
	}

	// Spawned AI pawns may enter BeginPlay before AutoPossessAI calls PossessedBy.
	// Startup data must be requested from either path, but only once.
	if (!RPGDemoAbilitySystemComponent->GetAvatarActor())
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	InitEnemyStartUpData();

}

void ARPGDemoEnemyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority())
	{
		InitEnemyStartUpData();
	}
}

#if WITH_EDITOR
void ARPGDemoEnemyCharacter::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, LeftHandCollisionBoxAttachBoneName))
	{
		LeftHandCollisionBox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, LeftHandCollisionBoxAttachBoneName);
	}
	else if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, RightHandCollisionBoxAttachBoneName))
	{
		RightHandCollisionBox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RightHandCollisionBoxAttachBoneName);
	}
}
#endif

void ARPGDemoEnemyCharacter::OnBodyCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                            AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                                            const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (URPGDemoFunctionLibrary::TargetPawnHostile(this, HitPawn))
		{
            if (RPGDemoCombat::IsDebugEnabled())
                UE_LOG(LogRPGDemoCombat, Log, TEXT("BodyOverlap pawn=%s target=%s box=%s position=%s"), *GetName(), *HitPawn->GetName(), *OverlappedComponent->GetName(), *OverlappedComponent->GetComponentLocation().ToString());
			EnemyCombatComponent->OnHitTargetActor(HitPawn);
		}
	}
}

void ARPGDemoEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoEnemyCharacter, ReplicatedDeathPresentation);
}

void ARPGDemoEnemyCharacter::BeginReplicatedDeathPresentation()
{
    if (!HasAuthority() || bDeathPresentationRequested) return;
    bDeathPresentationRequested = true;
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
        if (ARPGDemoAIController* RPGDemoAI = Cast<ARPGDemoAIController>(AI)) RPGDemoAI->StopTargetSelection();
        if (AI->BrainComponent) AI->BrainComponent->StopLogic(TEXT("Enemy death"));
    }
    UAnimMontage* UnusedMontage = nullptr;
    UNiagaraSystem* Dissolve = DeathDissolveSystem;
    if (!Dissolve) ResolveDeathPresentationAssets(UnusedMontage, Dissolve);
    ReplicatedDeathPresentation.DissolveSystem = Dissolve;
    ReplicatedDeathPresentation.bStarted = true;
    ReplicatedDeathPresentation.DissolveDuration = FMath::Max(DeathDissolveDuration, 0.01f);
    EnemyHealthWidgetComponent->SetVisibility(false, true);
    BeginCharacterDeath();
    ReplicatedDeathPresentation.DeathMontage = GetDeathSnapshot().Montage;
    ForceNetUpdate();
}

void ARPGDemoEnemyCharacter::FinalizeReplicatedDeathPresentation() { BeginReplicatedDeathPresentation(); }

void ARPGDemoEnemyCharacter::OnRep_DeathPresentation()
{
    ApplyReplicatedDeathPresentation();
}

void ARPGDemoEnemyCharacter::ApplyReplicatedDeathPresentation()
{
    if (!ReplicatedDeathPresentation.bStarted) return;
    bLocalDeathPresentationApplied = true;
    EnemyHealthWidgetComponent->SetVisibility(false, true);
    EnemyUIComponent->RemoveEnemyDrawnWidgetsIfAny();
    if (IsDeathPoseHeld()) BeginReplicatedDissolvePresentation();
}

void ARPGDemoEnemyCharacter::FinishEnemyDeathPresentation()
{
    // Ability cancellation must not shorten the character's replicated death animation.
    if (IsDeathPoseHeld()) BeginReplicatedDissolvePresentation();
}

void ARPGDemoEnemyCharacter::OnDeathPoseHeld()
{
    BeginReplicatedDissolvePresentation();
}

void ARPGDemoEnemyCharacter::BeginReplicatedDissolvePresentation()
{
    if (!IsDeathPoseHeld() || bLocalDissolvePresentationApplied) return;
    bLocalDissolvePresentationApplied = true;
    TInlineComponentArray<UTimelineComponent*> Timelines(this);
    for (UTimelineComponent* Timeline : Timelines)
        if (Timeline->GetName().Contains(TEXT("DissolveTimeline")))
        {
            TSet<UCurveBase*> Curves;
            Timeline->GetAllCurves(Curves);
            for (UCurveBase* Curve : Curves) if (UCurveFloat* FloatCurve = Cast<UCurveFloat>(Curve)) { DissolveCurve = FloatCurve; break; }
        }
    bLocalDeathPresentationApplied = true;
    if (HasAuthority())
    {
        ReplicatedDeathPresentation.bDissolving = true;
        ReplicatedDeathPresentation.DissolveStartServerTime = GetDeathSnapshot().StartServerTime + GetDeathSnapshot().Duration;
        ForceNetUpdate();
        const float Remaining = FMath::Max(0.01f, static_cast<float>(
            ReplicatedDeathPresentation.DissolveStartServerTime + ReplicatedDeathPresentation.DissolveDuration + 0.5 - GetPresentationServerTime()));
        GetWorldTimerManager().SetTimer(DeathCleanupTimer, this, &ThisClass::CompleteDeathCleanup, Remaining, false);
    }
    EnemyHealthWidgetComponent->SetVisibility(false, true);
    if (GetNetMode() != NM_DedicatedServer)
    {
        EnemyUIComponent->RemoveEnemyDrawnWidgetsIfAny();
        for (int32 Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index) GetMesh()->CreateDynamicMaterialInstance(Index);
        if (UNiagaraSystem* System = ReplicatedDeathPresentation.DissolveSystem)
        {
            if (UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetMesh(), NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true))
            {
                if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(GetMesh()->GetMaterial(0)))
                    FX->SetVariableLinearColor(TEXT("DissolveParticleColor"), Material->K2_GetVectorParameterValue(TEXT("DissolveEdgeColor")));
            }
        }
    }
    UpdateDissolvePresentation();
    GetWorldTimerManager().SetTimer(DissolveUpdateTimer, this, &ThisClass::UpdateDissolvePresentation, 0.016f, true);
}

void ARPGDemoEnemyCharacter::UpdateDissolvePresentation()
{
    if (GetNetMode() == NM_DedicatedServer) { GetWorldTimerManager().ClearTimer(DissolveUpdateTimer); return; }
    const double Start = ReplicatedDeathPresentation.DissolveStartServerTime > 0.0
        ? ReplicatedDeathPresentation.DissolveStartServerTime : GetDeathSnapshot().StartServerTime + GetDeathSnapshot().Duration;
    const float Progress = FMath::Clamp(static_cast<float>((GetPresentationServerTime() - Start) /
        FMath::Max(ReplicatedDeathPresentation.DissolveDuration, 0.01f)), 0.f, 1.f);
    const float Amount = DissolveCurve.IsValid() ? DissolveCurve->GetFloatValue(Progress) : Progress;
    GetMesh()->SetScalarParameterValueOnMaterials(TEXT("DissolveAmount"), Amount);
    if (ARPGDemoWeaponBase* Weapon = EnemyCombatComponent->GetCharacterCurrentlyEquippedWeapon())
    {
        TInlineComponentArray<UMeshComponent*> Meshes(Weapon);
        for (UMeshComponent* WeaponMesh : Meshes) WeaponMesh->SetScalarParameterValueOnMaterials(TEXT("DissolveAmount"), Amount);
    }
    if (Progress >= 1.f) GetWorldTimerManager().ClearTimer(DissolveUpdateTimer);
}

void ARPGDemoEnemyCharacter::CompleteDeathCleanup()
{
    if (!HasAuthority() || IsActorBeingDestroyed()) return;
    SpawnDeathDrop();
    if (ARPGDemoWeaponBase* Weapon = EnemyCombatComponent->GetCharacterCurrentlyEquippedWeapon()) Weapon->Destroy();
    Destroy();
}

void ARPGDemoEnemyCharacter::SpawnDeathDrop()
{
    if (!HasAuthority() || bDeathDropRequested || !GetWorld()) return;
    bDeathDropRequested = true;
    // Preserve the existing ability's scalable probability and granted difficulty level.
    const UGameplayAbility* DropDefaults = DeathDropAbility ? DeathDropAbility->GetDefaultObject<UGameplayAbility>() : nullptr;
    int32 Level = 1;
    if (RPGDemoAbilitySystemComponent)
        for (const FGameplayAbilitySpec& Spec : RPGDemoAbilitySystemComponent->GetActivatableAbilities())
            if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(RPGDemoGameplayTags::Enemy_Ability_SpawnStone))
            { DropDefaults = Spec.Ability; Level = Spec.Level; break; }
    const FStructProperty* ChanceProperty = DropDefaults
        ? FindFProperty<FStructProperty>(DropDefaults->GetClass(), TEXT("StoneSpawnChanceScalableFloat")) : nullptr;
    if (!ChanceProperty || ChanceProperty->Struct != FScalableFloat::StaticStruct()) return;
    const auto* Chance = ChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(DropDefaults);
    const bool bHealing = FMath::FRand() < 0.5f;
    if (FMath::FRand() >= FMath::Clamp(Chance->GetValueAtLevel(Level), 0.f, 1.f)) return;

    UWorld* World = GetWorld();
    FVector Location = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FHitResult Ground;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DeathDrop), false, this);
    if (World->LineTraceSingleByObjectType(Ground, GetActorLocation(), GetActorLocation() - FVector(0, 0, 200),
        FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic), Query)) Location = Ground.ImpactPoint;
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        FNavLocation Random;
        if (Nav->GetRandomPointInNavigableRadius(Location, 200.f, Random)) Location = Random.Location;
    }
    const FTransform SpawnTransform(FRotator(0, FMath::FRandRange(0.f, 360.f), 0), Location);
    const FSoftClassPath StonePath(bHealing ? TEXT("/Game/Items/Stones/BP_HealingStone.BP_HealingStone_C")
        : TEXT("/Game/Items/Stones/BP_RageStone.BP_RageStone_C"));
    // The world owns completion, so destroying the corpse/ASC cannot cancel a cold asset load.
    UAssetManager::GetStreamableManager().RequestAsyncLoad(StonePath,
        FStreamableDelegate::CreateWeakLambda(World, [World, StonePath, SpawnTransform]()
        {
            if (World->bIsTearingDown || World->GetNetMode() == NM_Client) return;
            UClass* StoneClass = StonePath.ResolveClass();
            if (!StoneClass || !StoneClass->IsChildOf(ARPGDemoStoneBase::StaticClass()))
            { UE_LOG(LogRPGDemoCombat, Error, TEXT("Death drop class failed to load: %s"), *StonePath.ToString()); return; }
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            if (!World->SpawnActor<ARPGDemoStoneBase>(StoneClass, SpawnTransform, Params))
                UE_LOG(LogRPGDemoCombat, Error, TEXT("Death drop failed to spawn: %s"), *StonePath.ToString());
        }));
}

void ARPGDemoEnemyCharacter::ResolveDeathPresentationAssets(
	UAnimMontage*& OutMontage,
	UNiagaraSystem*& OutDissolveSystem) const
{
	OutMontage = RPGDemoAbilitySystemComponent ? RPGDemoAbilitySystemComponent->GetCurrentMontage() : nullptr;
	OutDissolveSystem = nullptr;

	if (!RPGDemoAbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : RPGDemoAbilitySystemComponent->GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = AbilitySpec.Ability;
		if (!Ability || !Ability->GetAssetTags().HasTagExact(RPGDemoGameplayTags::Shared_Ability_Death))
		{
			continue;
		}

		TArray<UAnimMontage*> CandidateMontages;
		for (TFieldIterator<FArrayProperty> PropertyIt(Ability->GetClass()); PropertyIt; ++PropertyIt)
		{
			FArrayProperty* ArrayProperty = *PropertyIt;
			FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner);
			if (!InnerObjectProperty || !InnerObjectProperty->PropertyClass->IsChildOf(UAnimMontage::StaticClass()))
			{
				continue;
			}

			void* ArrayAddress = ArrayProperty->ContainerPtrToValuePtr<void>(const_cast<UGameplayAbility*>(Ability));
			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddress);
			for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			{
				if (UAnimMontage* Montage = Cast<UAnimMontage>(
					InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index))))
				{
					CandidateMontages.Add(Montage);
				}
			}
		}

		// CurrentMontage may still be an attack when startup/death activation was interrupted.
		if (!CandidateMontages.Contains(OutMontage) && !CandidateMontages.IsEmpty())
		{
			OutMontage = CandidateMontages[FMath::RandHelper(CandidateMontages.Num())];
		}

		for (TFieldIterator<FObjectPropertyBase> PropertyIt(Ability->GetClass()); PropertyIt; ++PropertyIt)
		{
			FObjectPropertyBase* ObjectProperty = *PropertyIt;
			if (ObjectProperty->PropertyClass->IsChildOf(UNiagaraSystem::StaticClass()) &&
				ObjectProperty->GetName().Contains(TEXT("Dissolve"), ESearchCase::IgnoreCase))
			{
				if (FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(ObjectProperty))
				{
					// Resolve on authority before replicating the hard reference; a cold soft asset's Get() is null.
					OutDissolveSystem = Cast<UNiagaraSystem>(SoftProperty->GetPropertyValue_InContainer(Ability).LoadSynchronous());
				}
				else
				{
					OutDissolveSystem = Cast<UNiagaraSystem>(ObjectProperty->GetObjectPropertyValue_InContainer(Ability));
				}
				break;
			}
		}

		break;
	}
}
void ARPGDemoEnemyCharacter::InitEnemyStartUpData()
{
	if (!HasAuthority() || !GetWorld() || IsActorBeingDestroyed() || bDeathPresentationRequested || bEnemyStartUpDataInitialized || bEnemyStartUpDataLoading || CharacterStartUpData.IsNull())
	{
		return;
	}

	bEnemyStartUpDataLoading = true;

	int32 AbilityApplyLevel = 1;

	if (ARPGDemoBaseGameMode* BaseGameMode = GetWorld()->GetAuthGameMode<ARPGDemoBaseGameMode>())
	{
		switch (BaseGameMode->GetCurrentGameDifficulty())
		{
		case ERPGDemoGameDifficulty::Easy:
			AbilityApplyLevel = 1;
			break;

		case ERPGDemoGameDifficulty::Normal:
			AbilityApplyLevel = 2;
			break;

		case ERPGDemoGameDifficulty::Hard:
			AbilityApplyLevel = 3;
			break;

		case ERPGDemoGameDifficulty::ExtremelyHard:
			AbilityApplyLevel = 4;
			break;

		default:
			break;
		}
	}

	EnemyStartUpDataHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		CharacterStartUpData.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this,
			[this, AbilityApplyLevel]()
			{
				bEnemyStartUpDataLoading = false;
				if (!HasAuthority() || IsActorBeingDestroyed() || bDeathPresentationRequested || !GetWorld() ||
					GetWorld()->bIsTearingDown || !IsValid(RPGDemoAbilitySystemComponent) ||
					RPGDemoAbilitySystemComponent->GetAvatarActor() != this)
				{
					return;
				}

				if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.Get())
				{
					LoadedData->GiveToAbilitySystemComponent(RPGDemoAbilitySystemComponent, AbilityApplyLevel);
					bEnemyStartUpDataInitialized = true;
				}
			}
		)
	);
}

void ARPGDemoEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EnemyUIComponent->RemoveEnemyDrawnWidgetsIfAny();
	if (EnemyStartUpDataHandle.IsValid())
	{
		EnemyStartUpDataHandle->CancelHandle();
		EnemyStartUpDataHandle.Reset();
	}
	bEnemyStartUpDataLoading = false;
	GetWorldTimerManager().ClearTimer(DeathPresentationTimerHandle);
    GetWorldTimerManager().ClearTimer(DeathCleanupTimer);
    GetWorldTimerManager().ClearTimer(DissolveUpdateTimer);
	Super::EndPlay(EndPlayReason);
}
