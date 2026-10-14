#include "Tests/RPGDemoLifecycleTestHarness.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "AbilitySystem/Abilities/HeroGameplayAbility_TargetLock.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BTNode.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Controllers/RPGDemoAIController.h"
#include "Controllers/RPGDemoHeroController.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/UI/HeroUIComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Materials/MaterialInstance.h"
#include "Components/TextBlock.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/UnrealType.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/RPGDemoCombatLog.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameModes/RPGDemoLobbyGameMode.h"
#include "GameModes/RPGDemoSurvivalGameMode.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "Items/PickUps/RPGDemoStoneBase.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "NavigationSystem.h"
#include "RPGDemoGameplayTags.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
    int32 LifecycleMatchesStarted = 0;
    int32 LifecycleMatchesCompleted = 0;
    int32 LifecycleServerFailures = 0;
    int32 ExpectedClients()
    {
        int32 Count = 2;
        FParse::Value(FCommandLine::Get(), TEXT("RPGDemoLifecycleClients="), Count);
        return FMath::Clamp(Count, 2, 4);
    }
    UUserWidget* FindOverlay(APlayerController* PC)
    {
        UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Widgets/HeroWidgets/WBP_HeroOverlay.WBP_HeroOverlay_C"));
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Widgets, Class, true);
        for (UUserWidget* Widget : Widgets) if (Widget->GetOwningPlayer() == PC) return Widget;
        return nullptr;
    }
    float StatusPercent(UUserWidget* Overlay, const FName Name)
    {
        UUserWidget* Status = Overlay ? Cast<UUserWidget>(Overlay->GetWidgetFromName(Name)) : nullptr;
        float Result = -1.f;
        if (Status && Status->WidgetTree) Status->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
        { if (UProgressBar* Bar = Cast<UProgressBar>(Widget)) Result = Bar->GetPercent(); });
        return Result;
    }
    bool HasVisibleBrush(UUserWidget* Root, UObject* Resource)
    {
        if (!Root || !Root->WidgetTree || !Resource) return false;
        bool bFound = false;
        Root->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if (UImage* Image = Cast<UImage>(Widget); Image && Image->IsVisible() && Image->GetRenderOpacity() > 0.1f)
            {
                UObject* Brush = Image->GetBrush().GetResourceObject();
                bFound |= Brush == Resource;
                for (UMaterialInstance* Material = Cast<UMaterialInstance>(Brush); Material; Material = Cast<UMaterialInstance>(Material->Parent))
                    bFound |= Material == Resource;
            }
            if (UUserWidget* Child = Cast<UUserWidget>(Widget); Child && Child != Root)
                bFound |= HasVisibleBrush(Child, Resource);
        });
        return bFound;
    }
}

ARPGDemoLifecycleTestHarness::ARPGDemoLifecycleTestHarness()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ARPGDemoLifecycleTestHarness::InitializeTestWorld(UWorld& World)
{
#if !UE_BUILD_SHIPPING
    if (World.GetNetMode() == NM_Client) return;
    if (World.GetMapName().Contains(TEXT("MultiplayerLobbyMap")))
    {
        FTimerHandle Timer;
        World.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(&World, [&World]()
        {
            if (LifecycleMatchesCompleted >= 2 || World.bIsTearingDown) return;
            ARPGDemoLobbyGameMode* Lobby = World.GetAuthGameMode<ARPGDemoLobbyGameMode>();
            if (!Lobby || Lobby->GetNumPlayers() < ExpectedClients()) return;
            for (auto It = World.GetPlayerControllerIterator(); It; ++It)
                if (APlayerController* PC = It->Get())
                    if (ARPGDemoPlayerState* State = PC->GetPlayerState<ARPGDemoPlayerState>(); State && State->bIsLobbyHost)
                    {
                        Lobby->SetDifficultyForHost(PC, LifecycleMatchesCompleted == 0 ? ERPGDemoGameDifficulty::Normal : ERPGDemoGameDifficulty::Hard);
                        if (LifecycleMatchesStarted == LifecycleMatchesCompleted) ++LifecycleMatchesStarted;
                        Lobby->StartGameForHost(PC);
                        break;
                    }
        }), 2.f, true);
    }
    else if (World.GetMapName().Contains(TEXT("SurvivalGameModeMap"))) World.SpawnActor<ARPGDemoLifecycleTestHarness>();
#endif
}

void ARPGDemoLifecycleTestHarness::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, Phase);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, Cycle);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, Match);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, PhaseStartServerTime);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, TestOrigin);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, Fixture);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, Corpse);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, HealthStone);
    DOREPLIFETIME(ARPGDemoLifecycleTestHarness, RageStone);
}

ARPGDemoHeroCharacter* ARPGDemoLifecycleTestHarness::FindHero(int32 PlayerRole) const
{
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get(); PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == FString::Printf(TEXT("LocalClient%d"), PlayerRole))
            return Cast<ARPGDemoHeroCharacter>(PC->GetPawn());
    return nullptr;
}

void ARPGDemoLifecycleTestHarness::Check(bool bPassed, const TCHAR* Case, int32 PlayerRole)
{
    Failures += !bPassed;
    if (HasAuthority() && !bPassed) ++LifecycleServerFailures;
    UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE %s case=%s match=%d cycle=%d phase=%d role=%d"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), Case, Match, Cycle, Phase, PlayerRole);
}

void ARPGDemoLifecycleTestHarness::SetPhase(int32 NewPhase)
{
    Phase = NewPhase;
    PhaseStartServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
    ForceNetUpdate();
}

void ARPGDemoLifecycleTestHarness::PrepareCycle()
{
    // Keep a natural ground-level fight. Use the live capsule and a physical
    // floor after entry presentation, rather than relying only on navigation
    // projection for the fixture's actor-center height.
    const auto GroundPosition = [this](ACharacter* Character, FVector& Position, float& FloorZ)
    {
        if (!Character) return false;
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(LifecycleGround), false, Character);
        for (TActorIterator<ARPGDemoWeaponBase> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
        if (!GetWorld()->LineTraceSingleByObjectType(Hit, Position + FVector(0, 0, 500),
            Position - FVector(0, 0, 1000), FCollisionObjectQueryParams(ECC_WorldStatic), Query)) return false;
        FloorZ = Hit.ImpactPoint.Z;
        Position.Z = FloorZ + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f;
        return true;
    };
    ARPGDemoHeroCharacter* MainHero = FindHero(1);
    float HeroFloorZ = 0.f, EnemyFloorZ = 0.f;
    const bool bHeroGrounded = GroundPosition(MainHero, TestOrigin, HeroFloorZ);
    bool bEnemyGrounded = GroundPosition(Fixture, FixtureStart, EnemyFloorZ);
    if (Fixture && bHeroGrounded && (!bEnemyGrounded || FMath::Abs(HeroFloorZ - EnemyFloorZ) >= 10.f))
    {
        UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        for (int32 Direction = 0; Nav && Direction < 8; ++Direction)
        {
            const float Angle = Direction * PI / 4.f;
            FVector Candidate = TestOrigin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 900.f;
            FNavLocation Projected;
            if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(150, 150, 400))) continue;
            Candidate = Projected.Location;
            float CandidateFloorZ = 0.f;
            if (GroundPosition(Fixture, Candidate, CandidateFloorZ) && FMath::Abs(HeroFloorZ - CandidateFloorZ) < 10.f)
            {
                FixtureStart = Candidate;
                EnemyFloorZ = CandidateFloorZ;
                bEnemyGrounded = true;
                break;
            }
        }
    }
    Check(bHeroGrounded && bEnemyGrounded && FMath::Abs(HeroFloorZ - EnemyFloorZ) < 10.f, TEXT("TestCombatGroundAligned"));
    if (MainHero && Fixture)
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE ground setup heroHalf=%.1f enemyHalf=%.1f enemyDefaultHalf=%.1f heroFloorZ=%.1f enemyFloorZ=%.1f"),
            MainHero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Fixture->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
            Fixture->GetClass()->GetDefaultObject<ARPGDemoEnemyCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), HeroFloorZ, EnemyFloorZ);
    for (int32 PlayerRole = 1; PlayerRole <= ExpectedClients(); ++PlayerRole)
        if (ARPGDemoHeroCharacter* Hero = FindHero(PlayerRole))
        {
            Hero->GetCharacterMovement()->StopMovementImmediately();
            if (PlayerRole != 1) Hero->GetCharacterMovement()->DisableMovement();
            Hero->SetActorLocation(TestOrigin + (PlayerRole == 1 ? FVector::ZeroVector : FVector(0, 12000.f + PlayerRole * 200.f, 1500)), false, nullptr, ETeleportType::TeleportPhysics);
            auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
            ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 1000.f);
            ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 1000.f);
            ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentRageAttribute(), Hero->GetRPGDemoAttributeSet()->GetMaxRage());
            Hero->GetRPGDemoAttributeSet()->SynchronizeRageStatus();
        }
    if (Fixture)
    {
        Fixture->TeleportTo(FixtureStart, FRotator(0, 180, 0));
        Fixture->GetRPGDemoAbilitySystemComponent()->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 10000.f);
        Fixture->GetRPGDemoAbilitySystemComponent()->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 10000.f);
    }
    SetPhase(1);
    NextAIDiagnostic = 14.0;
    LowestHeroHealth = 1000.f;
    LowestEnemyHealth = 10000.f;
    MaxEnemyTilt = 0.f;
    GroundedEnemySamples = 0;
}

void ARPGDemoLifecycleTestHarness::DamageToDeath(ARPGDemoHeroCharacter* Hero)
{
    if (!Hero) return;
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(this);
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = URPGDemoAttributeSet::GetDamageTakenAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(100000.f);
    auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
    auto* SourceASC = FParse::Param(FCommandLine::Get(), TEXT("RPGDemoRespawnRegression")) && IsValid(Fixture)
        ? Fixture->GetRPGDemoAbilitySystemComponent() : ASC;
    ASC->ApplyGameplayEffectToSelf(Effect, 1.f, SourceASC->MakeEffectContext());
}

void ARPGDemoLifecycleTestHarness::TickServer()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoRespawnRegression"))) { TickRespawnRegressionServer(); return; }
    if (!Started) Started = GetWorld()->GetRealTimeSeconds();
    if (GetWorld()->GetRealTimeSeconds() - Started > 180.f)
    { Check(false, TEXT("Timeout")); UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE COMPLETE failures=%d"), Failures); SetActorTickEnabled(false); return; }
    ARPGDemoSurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ARPGDemoSurvivalGameMode>();
    ARPGDemoGameState* State = GetWorld()->GetGameState<ARPGDemoGameState>();
    if (!Mode || !State) return;
    ARPGDemoHeroCharacter* Hero = FindHero(1);
    const double Elapsed = State->GetServerWorldTimeSeconds() - PhaseStartServerTime;
    if (Phase == 1 && Hero && Fixture)
    {
        LowestHeroHealth = FMath::Min(LowestHeroHealth, Hero->GetRPGDemoAttributeSet()->GetCurrentHealth());
        LowestEnemyHealth = FMath::Min(LowestEnemyHealth, Fixture->GetRPGDemoAttributeSet()->GetCurrentHealth());
        if (Fixture->GetCharacterMovement()->IsMovingOnGround())
        {
            ++GroundedEnemySamples;
            MaxEnemyTilt = FMath::Max(MaxEnemyTilt, FMath::Max(FMath::Abs(Fixture->GetActorRotation().Pitch), FMath::Abs(Fixture->GetActorRotation().Roll)));
        }
        if (Elapsed >= NextAIDiagnostic)
        {
            NextAIDiagnostic = Elapsed + 2.0;
            const auto* AI = Cast<ARPGDemoAIController>(Fixture->GetController());
            const auto* Tree = AI ? Cast<UBehaviorTreeComponent>(AI->GetBrainComponent()) : nullptr;
            const FVector Delta = Hero->GetActorLocation() - Fixture->GetActorLocation();
            const FVector Forward = Fixture->GetActorForwardVector();
            UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE AI diagnostic cycle=%d elapsed=%.1f task=%s rotation=%s angle3D=%.1f angleXY=%.1f heroCenterZ=%.1f enemyCenterZ=%.1f heroFeetZ=%.1f enemyFeetZ=%.1f distanceXY=%.1f"),
                Cycle, Elapsed, Tree && Tree->GetActiveNode() ? *Tree->GetActiveNode()->GetNodeName() : TEXT("None"),
                *Fixture->GetActorRotation().ToString(),
                FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, Delta.GetSafeNormal()), -1.f, 1.f))),
                FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward.GetSafeNormal2D(), Delta.GetSafeNormal2D()), -1.f, 1.f))),
                Hero->GetActorLocation().Z, Fixture->GetActorLocation().Z,
                Hero->GetActorLocation().Z - Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
                Fixture->GetActorLocation().Z - Fixture->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Delta.Size2D());
        }
    }
    if (Phase == 0)
    {
        if (!Hero || !Hero->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe)) return;
        for (int32 PlayerRole = 2; PlayerRole <= ExpectedClients(); ++PlayerRole) if (!FindHero(PlayerRole)) return;
        Match = LifecycleMatchesStarted;
        if (Match >= 2)
        {
            auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
            const auto* PS = Hero->GetPlayerState<ARPGDemoPlayerState>();
            Check(PS && PS->LifeState == ERPGDemoPlayerLifeState::Alive && !ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead), TEXT("NewMatchAlive"));
            Check(Mode->GetCurrentGameDifficulty() == ERPGDemoGameDifficulty::Hard, TEXT("NewMatchDifficulty"));
            const FGameplayAbilitySpec* Equip = ASC->FindAbilitySpecFromClass(LoadClass<UGameplayAbility>(nullptr,
                TEXT("/Game/PlayerCharacter/GameplayAbility/Equipping/GA_Hero_EquipAxe.GA_Hero_EquipAxe_C")));
            Check(Equip && Equip->Level == 2, TEXT("NewMatchStartupLevel"));
            ++LifecycleMatchesCompleted;
            SetPhase(9);
            Mode->SetActorTickEnabled(false);
            return;
        }
        if (State->SurvivalSnapshot.State != ERPGDemoSurvivalGameModeState::InProgress) return;
        int32 Enemies = 0;
        for (TActorIterator<ARPGDemoEnemyCharacter> It(GetWorld()); It; ++It) ++Enemies;
        if (Enemies == 0) return;
        Check(true, TEXT("WavePreloadSpawn"));
        Mode->SetActorTickEnabled(false);
        TArray<ARPGDemoEnemyCharacter*> OldEnemies;
        for (TActorIterator<ARPGDemoEnemyCharacter> It(GetWorld()); It; ++It) OldEnemies.Add(*It);
        for (ARPGDemoEnemyCharacter* Enemy : OldEnemies) { Enemy->OnDestroyed.RemoveAll(Mode); Enemy->Destroy(); }
        TestOrigin = Hero->GetActorLocation();
        UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        FNavLocation Ground;
        if (Nav && Nav->ProjectPointToNavigation(TestOrigin, Ground, FVector(250, 250, 400)))
            TestOrigin = Ground.Location + FVector(0, 0, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        UClass* Class = LoadClass<ARPGDemoEnemyCharacter>(nullptr, TEXT("/Game/EnemyCharacter/Gruntling/Guardian/BP_Gruntling_Guardian.BP_Gruntling_Guardian_C"));
        FixtureStart = TestOrigin + FVector(900, 0, 0);
        if (Nav && Nav->ProjectPointToNavigation(FixtureStart, Ground, FVector(250, 250, 400)))
            FixtureStart = Ground.Location + FVector(0, 0, Class->GetDefaultObject<ARPGDemoEnemyCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        Fixture = GetWorld()->SpawnActor<ARPGDemoEnemyCharacter>(Class, FixtureStart, FRotator(0, 180, 0));
        if (!Fixture) { Check(false, TEXT("FixtureSpawn")); SetPhase(9); return; }
        Fixture->bAlwaysRelevant = true;
        Mode->RegisterSpawnedEnemies({Fixture});
        SetPhase(10);
    }
    else if (Phase == 10 && Fixture && Elapsed >= 2.0 && Fixture->GetRPGDemoAttributeSet()->GetMaxHealth() > 1.f)
    {
        PrepareCycle();
    }
    else if (Phase == 1 && Elapsed >= 20.f && Hero && Fixture)
    {
        // Initial asset loading and reaction montages can postpone the AI's first attack.
        // Keep an explicit deadline while allowing a full unopposed attack window.
        if ((LowestHeroHealth >= 1000.f || LowestEnemyHealth >= 10000.f) && Elapsed < 26.f) return;
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE positions hero=%s enemy=%s"), *Hero->GetActorLocation().ToString(), *Fixture->GetActorLocation().ToString());
        auto* AI = Cast<ARPGDemoAIController>(Fixture->GetController());
        Check(AI && AI->GetBlackboardComponent() && AI->GetBlackboardComponent()->GetValueAsObject(TEXT("TargetActor")) == Hero, TEXT("AITracksCurrentPawn"));
        Check(LowestHeroHealth < 1000.f, TEXT("AIDamagesCurrentPawn"));
        Check(LowestEnemyHealth < 10000.f, TEXT("OwnerWeaponDamagesEnemy"));
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE ground motion samples=%d maxTilt=%.4f movementMode=%d"),
            GroundedEnemySamples, MaxEnemyTilt, static_cast<int32>(Fixture->GetCharacterMovement()->MovementMode));
        Check(GroundedEnemySamples > 0 && MaxEnemyTilt < 0.1f, TEXT("GroundEnemyStaysUpright"));
        if (Cycle == 3)
        {
            HealthStone = GetWorld()->SpawnActor<ARPGDemoStoneBase>(LoadClass<ARPGDemoStoneBase>(nullptr, TEXT("/Game/Items/Stones/BP_HealingStone.BP_HealingStone_C")), TestOrigin + FVector(50, 0, 20), FRotator::ZeroRotator);
            RageStone = GetWorld()->SpawnActor<ARPGDemoStoneBase>(LoadClass<ARPGDemoStoneBase>(nullptr, TEXT("/Game/Items/Stones/BP_RageStone.BP_RageStone_C")), TestOrigin + FVector(-50, 0, 20), FRotator::ZeroRotator);
            Check(HealthStone && RageStone, TEXT("RealStonesSpawn"));
            if (HealthStone) HealthStone->bAlwaysRelevant = true;
            if (RageStone) RageStone->bAlwaysRelevant = true;
            for (int32 PlayerRole = 1; PlayerRole <= 2; ++PlayerRole) if (auto* PartyHero = FindHero(PlayerRole)) PartyHero->TeleportTo(TestOrigin, FRotator::ZeroRotator);
            SetPhase(3);
        }
        else
        {
            Corpse = Hero;
            Corpse->bAlwaysRelevant = true;
            DamageToDeath(Hero);
            State->SetSurvivalSnapshot(ERPGDemoSurvivalGameModeState::WaitSpawnNewWave, 1, State->SurvivalSnapshot.TotalWaves, 10.f);
            SetPhase(2);
        }
    }
    else if (Phase == 2 && Hero && Hero != Corpse && Hero->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe))
    {
        const auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
        Check(!Hero->GetHeroCombatComponent()->CurrentlyEquippedWeaponTag.IsValid(), TEXT("RespawnInitiallyCarried"));
        Check(!ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead) && ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Player_Status_Rage_None), TEXT("RespawnTags"));
        bool bNoOldSpecs = true;
        for (const auto& Spec : ASC->GetActivatableAbilities()) bNoOldSpecs &= Spec.SourceObject.Get() != Corpse.Get();
        Check(bNoOldSpecs && ASC->GetAvatarActor() == Hero, TEXT("RespawnAvatarAndSpecs"));
        Corpse = nullptr;
        ++Cycle;
        PrepareCycle();
    }
    else if (Phase == 3)
    {
        bServerSawConsumption |= HealthStone && RageStone && HealthStone->IsConsumed() && RageStone->IsConsumed();
        if (Elapsed < 5.f) return;
        Check(bServerSawConsumption && !IsValid(HealthStone) && !IsValid(RageStone), TEXT("StonesConsumedAndDestroyed"));
        TArray<ARPGDemoHeroCharacter*> Party;
        for (int32 PlayerRole = 1; PlayerRole <= ExpectedClients(); ++PlayerRole) if (auto* PartyHero = FindHero(PlayerRole)) Party.Add(PartyHero);
        for (auto* PartyHero : Party) DamageToDeath(PartyHero);
        Check(State->SurvivalSnapshot.State == ERPGDemoSurvivalGameModeState::TeamDefeated, TEXT("TeamWipeFailsMatch"));
        const int32 Revision = State->SurvivalSnapshot.Revision;
        if (Fixture) Fixture->Destroy();
        Check(State->SurvivalSnapshot.Revision == Revision && State->SurvivalSnapshot.State == ERPGDemoSurvivalGameModeState::TeamDefeated, TEXT("TerminalEnemyCallbackIgnored"));
        ++LifecycleMatchesCompleted;
        SetPhase(4);
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE firstMatchComplete failures=%d"), Failures);
    }
    else if (Phase == 9 && Elapsed > 2.0 && !bDisconnectVictimDied)
    {
        bDisconnectVictimDied = true;
        DamageToDeath(FindHero(ExpectedClients()));
        Check(!FindHero(ExpectedClients()), TEXT("DisconnectDuringRespawn"));
    }
    else if (Phase == 9 && Elapsed > 8.0)
    {
        Check(Mode->GetNumPlayers() == ExpectedClients() - 1 && !FindHero(ExpectedClients()), TEXT("DisconnectControllerRemoved"));
        int32 Weapons = 0;
        bool bOrphan = false;
        for (TActorIterator<ARPGDemoHeroWeapon> It(GetWorld()); It; ++It)
        {
            ++Weapons;
            const auto* WeaponOwner = Cast<ARPGDemoHeroCharacter>(It->GetOwner());
            bOrphan |= !IsValid(WeaponOwner) || !WeaponOwner->GetController();
        }
        Check(!bOrphan && Weapons == ExpectedClients() - 1, TEXT("DisconnectWeaponCleanup"));
        SetPhase(11);
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LIFECYCLE COMPLETE failures=%d clients=%d"), LifecycleServerFailures, ExpectedClients());
    }
}

void ARPGDemoLifecycleTestHarness::TickClient()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoRespawnRegression"))) { TickRespawnRegressionClient(); return; }
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->IsLocalController()) return;
    int32 PlayerRole = 1;
    FParse::Value(FCommandLine::Get(), TEXT("RPGDemoCombatRole="), PlayerRole);
    auto* Hero = Cast<ARPGDemoHeroCharacter>(PC->GetPawn());
    const auto* State = GetWorld()->GetGameState();
    const double Elapsed = State ? State->GetServerWorldTimeSeconds() - PhaseStartServerTime : 0.0;
    if (LastClientPhase != Phase || LastClientCycle != Cycle)
    {
        LastClientPhase = Phase; LastClientCycle = Cycle;
        bClientEquipped = bClientLight = bClientHeavy = bClientSpecial = bClientHUDChecked = bClientPoseChecked = false;
        bClientPromptChecked = bClientPickupRequested = bClientStoneCleanupChecked = bClientSawDeathMontage = false;
        HeldPose.Empty(); PoseSampleTime = 0.0;
        ClientSpecialIndex = 0;
        bClientSpecialPresentationChecked = false;
        NextEquipAttempt = NextLightAttempt = NextHeavyAttempt = NextSpecialAttempt = 0.0;
        NextCombatRetry = 12.0;
        if (Hero && Phase == 1)
        {
            Hero->GetCharacterMovement()->StopMovementImmediately();
            if (PlayerRole != 1) Hero->GetCharacterMovement()->DisableMovement();
            Hero->SetActorLocation(TestOrigin + (PlayerRole == 1 ? FVector::ZeroVector : FVector(0, 12000.f + PlayerRole * 200.f, 1500)), false, nullptr, ETeleportType::TeleportPhysics);
        }
        if (Hero && Phase == 3 && PlayerRole <= 2) Hero->TeleportTo(TestOrigin, FRotator::ZeroRotator);
    }
    if (Hero && (Phase == 1 || Phase == 9) && !bClientHUDChecked && Elapsed > 1.5)
    {
        UUserWidget* HUD = FindOverlay(PC);
        const auto* Attr = Hero->GetRPGDemoAttributeSet();
        Check(HUD && StatusPercent(HUD, TEXT("WBP_HeroHealthBar")) >= 0.f && Attr &&
            FMath::IsNearlyEqual(StatusPercent(HUD, TEXT("WBP_HeroHealthBar")), Attr->GetCurrentHealth() / Attr->GetMaxHealth(), 0.04f), TEXT("VisibleHealthHUD"), PlayerRole);
        Check(HUD && StatusPercent(HUD, TEXT("WBP_HeroRageBar")) >= 0.f, TEXT("VisibleRageHUD"), PlayerRole);
        Check(!UHeroGameplayAbility_TargetLock::IsValidLockTarget(Hero), TEXT("TargetLockRejectsPlayer"), PlayerRole);
        bClientHUDChecked = true;
        if (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoLifecycleScreenshots")))
            FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Lifecycle-HUD-Match%d-Cycle%d-PlayerRole%d.png"), Match, Cycle, PlayerRole), true, false);
    }
    if (Phase == 1 && PlayerRole == 1 && Hero && Fixture)
    {
        auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
        if (!ASC) return;
        const auto TryInput = [&](FGameplayTag Tag, const TCHAR* Case, bool& bDone, double Earliest, double Deadline, double& NextAttempt)
        {
            if (bDone || Elapsed < Earliest || Elapsed < NextAttempt) return;
            NextAttempt = Elapsed + 0.25;
            const bool bActivated = ASC->TryActivateAbilityByTag(Tag);
            if (bActivated || Elapsed >= Deadline)
            { Check(bActivated, Case, PlayerRole); bDone = true; }
        };
        TryInput(RPGDemoGameplayTags::Player_Ability_Equip_Axe, TEXT("OwnerEquip"), bClientEquipped, 0.5, 4.0, NextEquipAttempt);
        const bool bWeaponDamageObserved = Fixture->GetRPGDemoAttributeSet()->GetCurrentHealth() < 10000.f;
        if (Elapsed >= 13.f && bWeaponDamageObserved) Hero->GetCharacterMovement()->StopMovementImmediately();
        else if (Hero->GetDistanceTo(Fixture) > 150.f) Hero->AddMovementInput((Fixture->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D());
        TryInput(RPGDemoGameplayTags::Player_Ability_LightAttack_Axe, TEXT("OwnerLight"), bClientLight, 2.5, 8.0, NextLightAttempt);
        TryInput(RPGDemoGameplayTags::Player_Ability_HeavyAttack_Axe, TEXT("OwnerHeavy"), bClientHeavy, 4.5, 10.0, NextHeavyAttempt);
        if (!bClientSpecial && Elapsed > 7.0 + ClientSpecialIndex * 3.0 && Elapsed >= NextSpecialAttempt)
        {
            NextSpecialAttempt = Elapsed + 0.25;
            auto* Weapon = Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon();
            auto* Input = PC->GetLocalPlayer() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
            if (!bClientSpecialPresentationChecked)
            {
                bClientSpecialPresentationChecked = true;
                Check(Weapon && Input && Input->HasMappingContext(Weapon->HeroWeaponData.WeaponInputMappingContext), TEXT("WeaponInputMapping"), PlayerRole);
                if (ClientSpecialIndex == 0 && Weapon)
                {
                    UUserWidget* HUD = FindOverlay(PC);
                    bool bIconsVisible = true;
                    for (const auto& Ability : Weapon->HeroWeaponData.SpecialWeaponAbilities)
                        bIconsVisible &= HasVisibleBrush(HUD, Ability.SoftAbilityIconMaterial.Get());
                    Check(bIconsVisible, TEXT("VisibleWeaponAbilityIcons"), PlayerRole);
                    Check(HasVisibleBrush(HUD, Weapon->HeroWeaponData.SoftWeaponIconTexture.Get()), TEXT("VisibleEquippedWeaponIcon"), PlayerRole);
                }
            }
            if (Weapon && Weapon->HeroWeaponData.SpecialWeaponAbilities.IsValidIndex(ClientSpecialIndex))
            {
                const auto& Ability = Weapon->HeroWeaponData.SpecialWeaponAbilities[ClientSpecialIndex];
                const bool bOnCooldown = ASC->HasMatchingGameplayTag(Ability.AbilityCooldownTag);
                const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Ability.AbilityToGrant);
                const bool bPassed = Spec && (bOnCooldown || ASC->TryActivateAbility(Spec->Handle));
                if (bPassed || Elapsed >= 18.0)
                {
                    Check(bPassed, bOnCooldown ? TEXT("WeaponCooldownPreserved") : TEXT("OwnerSpecial"), PlayerRole);
                    ++ClientSpecialIndex;
                    bClientSpecialPresentationChecked = false;
                    bClientSpecial = ClientSpecialIndex >= Weapon->HeroWeaponData.SpecialWeaponAbilities.Num();
                }
            }
            else bClientSpecial = true;
        }
        // A successful activation can legitimately miss a strafing opponent. Continue fighting
        // after the one-shot grant checks instead of treating a missed swing as a broken skill.
        if (!bWeaponDamageObserved && Elapsed >= NextCombatRetry && Hero->GetDistanceTo(Fixture) < 240.f)
        {
            NextCombatRetry = Elapsed + 2.0;
            ASC->TryActivateAbilityByTag(RPGDemoGameplayTags::Player_Ability_LightAttack_Axe);
        }
    }
    if (Phase == 2 && Corpse)
    {
        if (auto* Anim = Corpse->GetMesh()->GetAnimInstance())
            if (auto* Montage = Anim->GetCurrentActiveMontage()) bClientSawDeathMontage |= Montage->GetName().Contains(TEXT("Death"));
        if (Corpse->IsDeathPoseHeld() && !bClientPoseChecked)
        {
            const auto& Pose = Corpse->GetMesh()->GetComponentSpaceTransforms();
            if (HeldPose.IsEmpty()) { HeldPose = Pose; PoseSampleTime = GetWorld()->GetTimeSeconds(); }
            else if (GetWorld()->GetTimeSeconds() - PoseSampleTime > 0.5)
            {
                bool bStable = HeldPose.Num() == Pose.Num();
                for (int32 Index = 0; bStable && Index < Pose.Num(); ++Index) bStable &= Pose[Index].Equals(HeldPose[Index], 0.05f);
                Check(bClientSawDeathMontage && bStable, TEXT("PlayerDeathPoseHeld"), PlayerRole);
                bClientPoseChecked = true;
            }
        }
        if (!bClientHUDChecked && PlayerRole == 1 && Elapsed > 1.0)
        {
            UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Widgets/GameModeWidgets/WBP_WaveTextWithCountDown.WBP_WaveTextWithCountDown_C"));
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Widgets, Class, true);
            TArray<double> Positions;
            for (auto* Widget : Widgets) if (Widget->GetOwningPlayer() == PC)
                if (auto* Text = Widget->GetWidgetFromName(TEXT("TextBlock_WaveText"))) Positions.Add(Text->GetCachedGeometry().GetAbsolutePosition().Y);
            Check(Positions.Num() == 2 && FMath::Abs(Positions[0] - Positions[1]) > 100.0, TEXT("WaveRespawnLayout"), PlayerRole);
            bClientHUDChecked = true;
            if (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoLifecycleScreenshots")))
                FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Lifecycle-Countdown-Cycle%d-PlayerRole%d.png"), Cycle, PlayerRole), true, false);
        }
    }
    if (Phase == 3 && Hero && PlayerRole <= 2)
    {
        if (!bClientPromptChecked && Elapsed > 1.0)
        {
            UUserWidget* HUD = FindOverlay(PC);
            UWidget* Key = HUD ? HUD->GetWidgetFromName(TEXT("WBP_ConsumeStoneKey")) : nullptr;
            Check(Key && Key->IsVisible(), TEXT("VisibleStonePrompt"), PlayerRole);
            bClientPromptChecked = true;
        }
        if (!bClientPickupRequested && Elapsed > 1.5)
        { bClientPickupRequested = true; Hero->ServerTryConsumeNearbyStones(); }
        if (!bClientStoneCleanupChecked && Elapsed > 3.0)
        {
            Check(!IsValid(HealthStone) && !IsValid(RageStone), TEXT("ClientStonesRemoved"), PlayerRole);
            bClientStoneCleanupChecked = true;
        }
    }
    if (Phase == 9 && PlayerRole == ExpectedClients() && Elapsed > 3.0 && !bClientRequestedDisconnect)
    {
        bClientRequestedDisconnect = true;
        Check(true, TEXT("DisconnectRequested"), PlayerRole);
        GEngine->Exec(GetWorld(), TEXT("quit"));
    }
}

ARPGDemoPlayerState* ARPGDemoLifecycleTestHarness::FindRegressionPlayerState(int32 PlayerRole) const
{
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (const APlayerController* PC = It->Get())
            if (auto* State = PC->GetPlayerState<ARPGDemoPlayerState>(); State &&
                State->GetPlayerName() == FString::Printf(TEXT("LocalClient%d"), PlayerRole)) return State;
    return nullptr;
}

void ARPGDemoLifecycleTestHarness::TickRespawnRegressionServer()
{
    auto* Mode = GetWorld()->GetAuthGameMode<ARPGDemoSurvivalGameMode>();
    auto* State = GetWorld()->GetGameState<ARPGDemoGameState>();
    if (!Mode || !State || Phase == 109) return;
    if (!Started) Started = GetWorld()->GetRealTimeSeconds();
    if (GetWorld()->GetRealTimeSeconds() - Started > 110.f)
    {
        Check(false, TEXT("RespawnRegressionTimeout"));
        UE_LOG(LogRPGDemoCombat, Log, TEXT("RESPAWN_REGRESSION COMPLETE failures=%d"), Failures);
        SetPhase(109);
        return;
    }
    auto* Hero1 = FindHero(1);
    auto* Hero2 = FindHero(2);
    const double Elapsed = State->GetServerWorldTimeSeconds() - PhaseStartServerTime;
    const auto StartDeath = [this, Mode](int32 PlayerRole, int32 NextPhase, int32 Wave)
    {
        Cycle = PlayerRole;
        Mode->CurrentWaveCount = Wave;
        Mode->SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::InProgress);
        Corpse = FindHero(PlayerRole);
        DamageToDeath(Corpse);
        const auto* PS = FindRegressionPlayerState(PlayerRole);
        Check(PS && PS->LifeState == ERPGDemoPlayerLifeState::Dead && PS->RespawnEndServerTime > GetWorld()->GetTimeSeconds(), TEXT("DeathSchedulesRespawn"), PlayerRole);
        Check(!Mode->IsMatchTerminal(), TEXT("LivingTeammateKeepsMatchRunning"), PlayerRole);
        SetPhase(NextPhase);
    };
    if (Phase == 0)
    {
        if (!Hero1 || !Hero2 || !Hero1->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe) ||
            !Hero2->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe)) return;
        Mode->SetActorTickEnabled(false);
        TArray<ARPGDemoEnemyCharacter*> Enemies;
        for (TActorIterator<ARPGDemoEnemyCharacter> It(GetWorld()); It; ++It) Enemies.Add(*It);
        for (auto* Enemy : Enemies) { Enemy->OnDestroyed.RemoveAll(Mode); Enemy->Destroy(); }
        RegressionPawnClass = Mode->DefaultPawnClass;
        Check(FMath::IsNearlyEqual(Mode->RespawnDelaySeconds, 10.f), TEXT("NormalRespawnDelayUnchanged"));
        StartDeath(1, 100, 1);
    }
    else if ((Phase == 100 || Phase == 102 || Phase == 104 || Phase == 106) && Elapsed > 11.f)
    {
        const int32 PlayerRole = Phase == 100 || Phase == 104 ? 1 : 2;
        ARPGDemoHeroCharacter* Hero = PlayerRole == 1 ? Hero1 : Hero2;
        if (Phase == 106)
        {
            auto* PS = FindRegressionPlayerState(PlayerRole);
            Check(!Hero && PS && PS->LifeState == ERPGDemoPlayerLifeState::Dead && PS->RespawnEndServerTime > State->GetServerWorldTimeSeconds(), TEXT("FailedSpawnRemainsQueued"), PlayerRole);
            Check(PS && Mode->RespawnTimers.Contains(PS) && !Mode->IsMatchTerminal() && Hero1, TEXT("FailedSpawnDoesNotDefeatTeam"), PlayerRole);
            Check(PS && PS->GetRPGDemoAbilitySystemComponent()->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead), TEXT("FailedSpawnRestoresDeadTag"), PlayerRole);
            Mode->DefaultPawnClass = RegressionPawnClass;
            SetPhase(107);
            return;
        }
        if (!Hero && Elapsed < 16.f) return;
        const auto* PS = Hero ? Hero->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
        Check(Hero && Hero != Corpse && PS && PS->LifeState == ERPGDemoPlayerLifeState::Alive &&
            !PS->GetRPGDemoAbilitySystemComponent()->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead), TEXT("RespawnRestoresAlivePawn"), PlayerRole);
        Check(Hero && Hero->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe), TEXT("RespawnRestoresCarriedWeapon"), PlayerRole);
        Check(!Mode->IsMatchTerminal(), TEXT("RespawnDoesNotDefeatTeam"), PlayerRole);
        if (Phase == 104)
        {
            bool bClear = Hero != nullptr;
            for (const auto& Blocker : SpawnBlockers)
                if (Blocker.IsValid() && Hero) bClear &= !Blocker->GetComponentsBoundingBox().IsInside(Hero->GetActorLocation());
            Check(bClear, TEXT("OccupiedStartsFindClearSpawn"));
            for (const auto& Blocker : SpawnBlockers) if (Blocker.IsValid()) Blocker->Destroy();
            SpawnBlockers.Empty();
        }
        SetPhase(Phase + 1);
    }
    else if (Phase == 101 && Elapsed > 2.f) StartDeath(2, 102, 2);
    else if (Phase == 103 && Elapsed > 2.f)
    {
        UClass* BossClass = LoadClass<ARPGDemoEnemyCharacter>(nullptr, TEXT("/Game/EnemyCharacter/FrostGiant/BP_FrostGiant.BP_FrostGiant_C"));
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Fixture = GetWorld()->SpawnActor<ARPGDemoEnemyCharacter>(BossClass, Hero1->GetActorLocation() + FVector(1500.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
        if (Fixture)
        {
            if (auto* AI = Cast<ARPGDemoAIController>(Fixture->GetController()); AI && AI->GetBrainComponent())
                AI->GetBrainComponent()->StopLogic(TEXT("Respawn regression fixture"));
            Fixture->GetCharacterMovement()->DisableMovement();
        }
        Check(Fixture != nullptr, TEXT("Wave3BossFixture"));
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            AActor* Blocker = GetWorld()->SpawnActor<AActor>();
            UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
            Blocker->AddInstanceComponent(Box);
            Blocker->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(240.f, 240.f, 400.f));
            Box->SetCanEverAffectNavigation(false);
            Box->SetCollisionProfileName(TEXT("BlockAll"));
            Box->RegisterComponent();
            Blocker->SetActorLocation(It->GetActorLocation());
            SpawnBlockers.Add(Blocker);
        }
        Check(!SpawnBlockers.IsEmpty(), TEXT("AllPlayerStartsOccupied"));
        StartDeath(1, 104, 3);
    }
    else if (Phase == 105 && Elapsed > 2.f)
    {
        // Force a transient spawn failure separately from the collision fallback.
        // Only this disposable test changes the pawn class, then restores it.
        Mode->DefaultPawnClass = nullptr;
        StartDeath(2, 106, 3);
    }
    else if (Phase == 107 && Elapsed > 3.f)
    {
        Check(Hero2 && Hero2->GetPlayerState<ARPGDemoPlayerState>()->LifeState == ERPGDemoPlayerLifeState::Alive,
            TEXT("QueuedRetryRecoversAfterSpawnAvailable"), 2);
        Check(!Mode->IsMatchTerminal() && Mode->RespawnTimers.IsEmpty(), TEXT("RetryQueueCleaned"));
        SetPhase(108);
    }
    else if (Phase == 108 && Elapsed > 2.f)
    {
        DamageToDeath(Hero1);
        DamageToDeath(Hero2);
        Check(State->SurvivalSnapshot.State == ERPGDemoSurvivalGameModeState::TeamDefeated && Mode->RespawnTimers.IsEmpty(), TEXT("ActualTeamWipeStillDefeats"));
        LifecycleMatchesCompleted = 2;
        SetPhase(109);
        UE_LOG(LogRPGDemoCombat, Log, TEXT("RESPAWN_REGRESSION COMPLETE failures=%d"), Failures);
    }
}

void ARPGDemoLifecycleTestHarness::TickRespawnRegressionClient()
{
    auto* PC = Cast<ARPGDemoHeroController>(GetWorld()->GetFirstPlayerController());
    const auto* GS = GetWorld()->GetGameState<ARPGDemoGameState>();
    if (!PC || !PC->IsLocalController() || !GS || Phase < 100) return;
    int32 PlayerRole = 1;
    FParse::Value(FCommandLine::Get(), TEXT("RPGDemoCombatRole="), PlayerRole);
    if (LastClientPhase != Phase) { LastClientPhase = Phase; bClientHUDChecked = false; }
    const double Elapsed = GS->GetServerWorldTimeSeconds() - PhaseStartServerTime;
    if (bClientHUDChecked || Elapsed < 1.f) return;
    if (Phase == 100 || Phase == 102 || Phase == 104 || Phase == 106)
    {
        const auto* PS = PC->GetPlayerState<ARPGDemoPlayerState>();
        Check(GS->SurvivalSnapshot.State != ERPGDemoSurvivalGameModeState::TeamDefeated, TEXT("ClientNoFalseDefeat"), PlayerRole);
        if (PlayerRole == Cycle)
        {
            const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(PC->GetClass(), TEXT("RespawnMessage"));
            const auto* Message = Property ? Cast<UUserWidget>(Property->GetObjectPropertyValue_InContainer(PC)) : nullptr;
            Check(PS && PS->LifeState == ERPGDemoPlayerLifeState::Dead && PC->GetLocalRespawnSecondsRemaining() > 0.f &&
                Message && Message->IsInViewport() && Message->IsVisible(), TEXT("ClientWaitingRespawnUI"), PlayerRole);
        }
        else Check(PS && PS->LifeState == ERPGDemoPlayerLifeState::Alive && PC->GetPawn(), TEXT("ClientTeammateStillAlive"), PlayerRole);
        bClientHUDChecked = true;
    }
    else if (Phase == 101 || Phase == 103 || Phase == 105 || Phase == 108)
    {
        Check(PC->GetPawn() && PC->GetPlayerState<ARPGDemoPlayerState>()->LifeState == ERPGDemoPlayerLifeState::Alive, TEXT("ClientRespawnRecovered"), PlayerRole);
        bClientHUDChecked = true;
    }
    else if (Phase == 109)
    {
        Check(GS->SurvivalSnapshot.State == ERPGDemoSurvivalGameModeState::TeamDefeated, TEXT("ClientActualTeamWipe"), PlayerRole);
        bClientHUDChecked = true;
        UE_LOG(LogRPGDemoCombat, Log, TEXT("RESPAWN_REGRESSION CLIENT COMPLETE failures=%d role=%d"), Failures, PlayerRole);
    }
}

void ARPGDemoLifecycleTestHarness::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority()) TickServer();
    if (GetNetMode() != NM_DedicatedServer) TickClient();
}
