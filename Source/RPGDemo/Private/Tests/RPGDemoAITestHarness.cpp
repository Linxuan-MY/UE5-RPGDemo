#include "Tests/RPGDemoAITestHarness.h"
#include "Tests/RPGDemoCombatTestHarness.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Controllers/RPGDemoAIController.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Combat/RPGDemoCombatLog.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "RPGDemoGameplayTags.h"

namespace
{
    const TCHAR* EnemyPaths[] = {
        TEXT("/Game/EnemyCharacter/Gruntling/Guardian/BP_Gruntling_Guardian.BP_Gruntling_Guardian_C"),
        TEXT("/Game/EnemyCharacter/Gruntling/Glacer/BP_Gruntling_Glacer.BP_Gruntling_Glacer_C"),
        TEXT("/Game/EnemyCharacter/FrostGiant/BP_FrostGiant.BP_FrostGiant_C")};
    const TCHAR* KindNames[] = {TEXT("Guardian"), TEXT("Glacer"), TEXT("Giant")};
}

ARPGDemoAITestHarness::ARPGDemoAITestHarness()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ARPGDemoAITestHarness::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARPGDemoAITestHarness, Fixture);
    DOREPLIFETIME(ARPGDemoAITestHarness, Phase);
    DOREPLIFETIME(ARPGDemoAITestHarness, EnemyKind);
    DOREPLIFETIME(ARPGDemoAITestHarness, Hero1Location);
    DOREPLIFETIME(ARPGDemoAITestHarness, Hero2Location);
    DOREPLIFETIME(ARPGDemoAITestHarness, bPlacementReady);
}

ARPGDemoHeroCharacter* ARPGDemoAITestHarness::FindHero(int32 PlayerRole) const
{
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get(); PC && PC->PlayerState &&
            PC->PlayerState->GetPlayerName() == FString::Printf(TEXT("LocalClient%d"), PlayerRole))
            return Cast<ARPGDemoHeroCharacter>(PC->GetPawn());
    return nullptr;
}

void ARPGDemoAITestHarness::PlaceHero(int32 PlayerRole, const FVector& Offset)
{
    (PlayerRole == 1 ? Hero1Location : Hero2Location) = Origin + Offset;
    bPlacementReady = true;
    if (auto* Hero = FindHero(PlayerRole))
    {
        Hero->GetCharacterMovement()->DisableMovement();
        Hero->TeleportTo(Origin + Offset, FRotator::ZeroRotator);
        Hero->GetRPGDemoAbilitySystemComponent()->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 10000.f);
        Hero->GetRPGDemoAbilitySystemComponent()->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 10000.f);
    }
}

void ARPGDemoAITestHarness::SpawnEnemy(int32 Kind, bool bRunBrain)
{
    if (Fixture) Fixture->Destroy();
    EnemyKind = Kind;
    UClass* Class = LoadClass<ARPGDemoEnemyCharacter>(nullptr, EnemyPaths[Kind]);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Fixture = Class ? GetWorld()->SpawnActor<ARPGDemoEnemyCharacter>(Class, Origin, FRotator::ZeroRotator, Params) : nullptr;
    if (!Fixture) { Check(false, TEXT("spawn")); return; }
    Fixture->bAlwaysRelevant = true;
    if (!bRunBrain)
    {
        // Only decisions are isolated; the native sight sense and its real occlusion
        // traces remain active. No synthetic stimuli or direct blackboard writes.
        if (auto* AI = Cast<AAIController>(Fixture->GetController()); AI && AI->BrainComponent)
            AI->BrainComponent->StopLogic(TEXT("Sight target regression"));
        Fixture->GetCharacterMovement()->DisableMovement();
    }
    ForceNetUpdate();
}

void ARPGDemoAITestHarness::DamageToDeath(AActor* Actor)
{
    auto* ASC = Actor->FindComponentByClass<URPGDemoAbilitySystemComponent>();
    if (auto* Hero = Cast<ARPGDemoHeroCharacter>(Actor)) ASC = Hero->GetRPGDemoAbilitySystemComponent();
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(this);
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = URPGDemoAttributeSet::GetDamageTakenAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(100000.f);
    if (ASC) ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext());
}

void ARPGDemoAITestHarness::Check(bool bPassed, const TCHAR* Case)
{
    Failures += !bPassed;
    UE_LOG(LogRPGDemoCombat, Log, TEXT("AI %s case=%s phase=%d"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Case, Phase);
}

void ARPGDemoAITestHarness::TickServer()
{
    const double Now = GetWorld()->GetTimeSeconds();
    // Autonomous pawns do not consume simulated-proxy transform replication.
    // Keep both ends of this stationary sight fixture at the agreed coordinates.
    if (bPlacementReady)
        for (int32 PlayerRole = 1; PlayerRole <= 2; ++PlayerRole)
            if (auto* Hero = FindHero(PlayerRole))
            {
                Hero->GetCharacterMovement()->DisableMovement();
                const FVector Desired = PlayerRole == 1 ? Hero1Location : Hero2Location;
                if (!Hero->GetActorLocation().Equals(Desired, 1.f)) Hero->TeleportTo(Desired, FRotator::ZeroRotator);
            }
    if (!Started) Started = Now;
    if (Now - Started > 90.0)
    {
        Check(false, TEXT("timeout"));
        UE_LOG(LogRPGDemoCombat, Log, TEXT("AI COMPLETE failures=%d"), Failures);
        SetActorTickEnabled(false);
        return;
    }
    if (Phase < 0)
    {
        if (!FindHero(1) || !FindHero(2) || FindHero(1)->GetRPGDemoAttributeSet()->GetMaxHealth() <= 0.f) return;
        Origin = FindHero(1)->GetActorLocation() + FVector(0, 0, 1500);
        for (TActorIterator<ARPGDemoEnemyCharacter> It(GetWorld()); It; ++It) It->Destroy();
        PlaceHero(1, FVector(1000, 0, 0));
        PlaceHero(2, FVector(0, 6500, 0));
        SpawnEnemy(0, false);
        Phase = 0;
        PhaseStarted = Now;
    }
    auto* AI = Fixture ? Cast<ARPGDemoAIController>(Fixture->GetController()) : nullptr;
    auto* BB = AI ? AI->GetBlackboardComponent() : nullptr;
    AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
    if (Phase == 0 && Target == FindHero(1))
    {
        PlaceHero(2, FVector(0, 300, 0));
        AActor* Wall = GetWorld()->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
        Wall->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(20, 200, 400));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->RegisterComponent();
        Wall->SetActorLocation(Origin + FVector(500, 0, 0));
        Occluder = Wall;
        PhaseStarted = Now;
        Phase = 1;
        UE_LOG(LogRPGDemoCombat, Log, TEXT("AI lockChallenge started=%.3f locked=P1 challenger=P2 nearer=1 occluded=1"), Now);
    }
    else if (Phase == 1)
    {
        TArray<AActor*> Seen;
        AI->GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);
        bSawSightLoss |= !Seen.Contains(FindHero(1)) && Seen.Contains(FindHero(2));
        if (Target != FindHero(1))
        {
            const double Held = Now - PhaseStarted;
            Check(Held >= 2.75 && Held <= 4.5 && bSawSightLoss && Target == FindHero(2), TEXT("lockSurvivesSightLossAndExpires"));
            UE_LOG(LogRPGDemoCombat, Log, TEXT("AI lockMeasured seconds=%.3f sawSightLoss=%d"), Held, bSawSightLoss);
            if (Occluder.IsValid()) Occluder->Destroy();
            DamageToDeath(FindHero(2));
            Phase = 2;
            PhaseStarted = Now;
        }
    }
    else if (Phase == 2 && Target == FindHero(1))
    {
        Check(Now - PhaseStarted < 0.6, TEXT("deadTargetBreaksLock"));
        Phase = 3;
        PhaseStarted = Now;
    }
    else if (Phase == 3 && FindHero(2) && !FindHero(2)->GetRPGDemoAbilitySystemComponent()->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead))
    {
        PlaceHero(1, FVector(1000, 0, 0));
        PlaceHero(2, FVector(0, 1300, 0));
        SpawnEnemy(0, true);
        Phase = 4;
        PhaseStarted = Now;
    }
    else if (Phase == 4 && Fixture && Now - PhaseStarted >= 2.0 && Fixture->GetRPGDemoAttributeSet()->GetMaxHealth() > 0.f)
    {
        DeathStartLocation = Fixture->GetActorLocation();
        if (EnemyKind == 2)
        {
            // Scripted boss death: no DamageTaken callback requests presentation.
            auto* ASC = Fixture->GetRPGDemoAbilitySystemComponent();
            ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 0.f);
            ASC->AddLooseGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
            UE_LOG(LogRPGDemoCombat, Log, TEXT("AI scriptedDeathTag kind=Giant"));
        }
        else DamageToDeath(Fixture);
        Check(!AI || !AI->BrainComponent || !AI->BrainComponent->IsRunning(), TEXT("deathStopsBrainImmediately"));
        UE_LOG(LogRPGDemoCombat, Log, TEXT("AI deathStarted kind=%s"), KindNames[EnemyKind]);
        Phase = 5;
        PhaseStarted = Now;
    }
    else if (Phase == 5)
    {
        ObserveCorpse(0);
        if (!IsValid(Fixture))
        {
            Check(bCheckedFrozen, TEXT("corpseDissolvesAndIsDestroyed"));
            Fixture = nullptr;
            if (EnemyKind < 2)
            {
                SpawnEnemy(EnemyKind + 1, true);
                Phase = 4;
                PhaseStarted = Now;
            }
            else
            {
                PlaceHero(1, FVector(6500, 0, 0));
                PlaceHero(2, FVector(0, 1000, 0));
                SpawnEnemy(0, false);
                Phase = 6;
            }
        }
    }
    else if (Phase == 6 && Target == FindHero(2))
    {
        PlaceHero(1, FVector(300, 0, 0));
        LostHero = FindHero(2);
        Phase = 7;
        PhaseStarted = Now;
        ForceNetUpdate();
    }
    else if (Phase == 7 && !FindHero(2) && Target == FindHero(1))
    {
        Check((!LostHero.IsValid() || !LostHero->GetController()) && Now - PhaseStarted < 2.75, TEXT("disconnectBreaksLock"));
        UE_LOG(LogRPGDemoCombat, Log, TEXT("AI COMPLETE failures=%d"), Failures);
        if (Fixture) Fixture->Destroy();
        Phase = 8;
        ForceNetUpdate();
        SetActorTickEnabled(false);
    }
}

void ARPGDemoAITestHarness::ObserveCorpse(int32 PlayerRole)
{
    if (LastKind != EnemyKind)
    {
        LastKind = EnemyKind;
        bObservedFrozen = bCheckedFrozen = false;
        DeathFirstPose.Reset();
        bSawDeathMontage = false;
        DeathPoseChange = 0.0;
        MaxPoseError = MaxPositionError = MaxAngleError = 0.0;
    }
    if (!IsValid(Fixture)) return;
    const auto* Mesh = Fixture->GetMesh();
    if (UAnimInstance* Anim = Mesh->GetAnimInstance())
        if (UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
            bSawDeathMontage |= Montage->GetName().Contains(TEXT("Death"));
    const TArray<FTransform>& Pose = Mesh->GetComponentSpaceTransforms();
    if (DeathFirstPose.IsEmpty()) DeathFirstPose = Pose;
    if (!Mesh->bPauseAnims)
    {
        for (int32 i = 0; i < FMath::Min(Pose.Num(), DeathFirstPose.Num()); ++i)
            DeathPoseChange = FMath::Max(DeathPoseChange, double(FVector::Dist(Pose[i].GetLocation(), DeathFirstPose[i].GetLocation())));
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bObservedFrozen)
    {
        bObservedFrozen = true;
        FrozenStarted = Now;
        FrozenPose = Mesh->GetComponentSpaceTransforms();
        FrozenLocation = Fixture->GetActorLocation();
        FrozenRotation = Fixture->GetActorQuat();
    }
    MaxPositionError = FMath::Max(MaxPositionError, FVector::Dist(FrozenLocation, Fixture->GetActorLocation()));
    MaxAngleError = FMath::Max(MaxAngleError, double(FrozenRotation.AngularDistance(Fixture->GetActorQuat())));
    if (Pose.Num() != FrozenPose.Num()) MaxPoseError = 10000.0;
    for (int32 i = 0; i < FMath::Min(Pose.Num(), FrozenPose.Num()); ++i)
        MaxPoseError = FMath::Max(MaxPoseError, double(FVector::Dist(Pose[i].GetLocation(), FrozenPose[i].GetLocation())) + Pose[i].GetRotation().AngularDistance(FrozenPose[i].GetRotation()));
    if (!bCheckedFrozen && Now - FrozenStarted >= 0.7)
    {
        auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
        // The original six-second dissolve curve eases in. Sample after it has
        // visibly advanced, while retaining at least 0.7 s of pose observation.
        if (!Material || Material->K2_GetScalarParameterValue(TEXT("DissolveAmount")) <= 0.05f) return;
        bCheckedFrozen = true;
        const bool bPassed = bSawDeathMontage && DeathPoseChange > 0.5 && MaxPoseError < 0.1 && MaxPositionError < 2.0 && MaxAngleError < 0.01 &&
            Fixture->GetVelocity().IsNearlyZero() && Fixture->GetRPGDemoAttributeSet()->GetCurrentHealth() <= 0.f &&
            Material && Material->K2_GetScalarParameterValue(TEXT("DissolveAmount")) > 0.05f;
        Failures += !bPassed;
        UE_LOG(LogRPGDemoCombat, Log, TEXT("AI %s corpseFrozen kind=%s role=%d duration=%.3f poseError=%.5f positionError=%.3f angleError=%.5f dissolve=%.3f deathMontage=%d deathPoseChange=%.3f"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"), KindNames[EnemyKind], PlayerRole, Now - FrozenStarted, MaxPoseError, MaxPositionError, MaxAngleError,
            Material ? Material->K2_GetScalarParameterValue(TEXT("DissolveAmount")) : -1.f, bSawDeathMontage, DeathPoseChange);
        if (PlayerRole > 0)
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("AITest-%s-Role%d.png"), KindNames[EnemyKind], PlayerRole), false, false);
    }
}

void ARPGDemoAITestHarness::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority()) TickServer();
    else
    {
        int32 PlayerRole = 0;
        FParse::Value(FCommandLine::Get(), TEXT("RPGDemoCombatRole="), PlayerRole);
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (bPlacementReady && PC)
            if (auto* Hero = Cast<ARPGDemoHeroCharacter>(PC->GetPawn()))
            {
                Hero->GetCharacterMovement()->DisableMovement();
                const FVector Desired = PlayerRole == 1 ? Hero1Location : Hero2Location;
                if (!Hero->GetActorLocation().Equals(Desired, 1.f)) Hero->TeleportTo(Desired, FRotator::ZeroRotator);
                if (IsValid(Fixture) && PC->PlayerCameraManager)
                    PC->SetControlRotation((Fixture->GetActorLocation() + FVector(0, 0, 40) - PC->PlayerCameraManager->GetCameraLocation()).Rotation());
            }
        if (Phase == 5) ObserveCorpse(PlayerRole);
        if (Phase == 7 && PlayerRole == 2 && !bRequestedDisconnect)
        {
            bRequestedDisconnect = true;
            GEngine->Exec(GetWorld(), TEXT("quit"));
        }
        if (Phase == 8) GEngine->Exec(GetWorld(), TEXT("quit"));
    }
}
