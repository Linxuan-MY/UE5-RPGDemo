// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/RPGDemoSurvivalGameMode.h"

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "RPGDemoGameplayTags.h"
#include "RPGDemoFunctionLibrary.h"
#include "TimerManager.h"

namespace RPGDemoMatch
{
    const FString LobbyTravel(TEXT("/Game/Maps/MultiplayerLobbyMap?game=/Script/RPGDemo.RPGDemoLobbyGameMode"));
    constexpr float RespawnRetrySeconds = 1.f;
}

void ARPGDemoSurvivalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    bMultiplayerMatch = GetNetMode() != NM_Standalone || UGameplayStatics::HasOption(Options, TEXT("RPGDemoMultiplayer"));
    if (bMultiplayerMatch)
    {
        const FString Value = UGameplayStatics::ParseOption(Options, TEXT("RPGDemoDifficulty"));
        if (Value == TEXT("Easy")) CurrentGameDifficulty = ERPGDemoGameDifficulty::Easy;
        else if (Value == TEXT("Hard")) CurrentGameDifficulty = ERPGDemoGameDifficulty::Hard;
        else if (Value == TEXT("ExtremelyHard")) CurrentGameDifficulty = ERPGDemoGameDifficulty::ExtremelyHard;
        else CurrentGameDifficulty = ERPGDemoGameDifficulty::Normal;
    }
    else
    {
        ERPGDemoGameDifficulty SavedDifficulty;
        if (URPGDemoFunctionLibrary::TryLoadSavedGameDifficulty(SavedDifficulty)) CurrentGameDifficulty = SavedDifficulty;
    }
}

void ARPGDemoSurvivalGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (ErrorMessage.IsEmpty() && bMultiplayerMatch)
    {
        ErrorMessage = TEXT("RPGDEMO_MATCH_IN_PROGRESS");
    }
}

void ARPGDemoSurvivalGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!EnemyWaveSpawnerDataTable || EnemyWaveSpawnerDataTable->GetRowNames().IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Survival match has no valid enemy wave table"));
        EnterTerminalState(ERPGDemoSurvivalGameModeState::TeamDefeated);
        return;
    }
    TotalWavesToSpawn = EnemyWaveSpawnerDataTable->GetRowNames().Num();
    SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::WaitSpawnNewWave);
    PreLoadNextWaveEnemies();
}

void ARPGDemoSurvivalGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::AllWavesDone ||
        CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::TeamDefeated) return;

    TimePassedSinceStart += DeltaTime;
    if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::WaitSpawnNewWave &&
        TimePassedSinceStart >= SpawnNewWaveWaitTime)
    {
        TimePassedSinceStart = 0.f;
        SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::SpawningNewWave);
    }
    else if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::SpawningNewWave &&
        TimePassedSinceStart >= SpawnEnemiesDelayTime)
    {
        if (PendingWaveLoads > 0 && TimePassedSinceStart < 20.f) return;
        TimePassedSinceStart = 0.f;
        if (!IsWavePreloadReady())
        {
            UE_LOG(LogTemp, Error, TEXT("Wave %d preload failed or timed out (pending=%d)"), CurrentWaveCount, PendingWaveLoads);
            if (++WaveSpawnFailures >= 3) EnterTerminalState(ERPGDemoSurvivalGameModeState::TeamDefeated);
            else PreLoadNextWaveEnemies();
            return;
        }
        CurrentSpawnedEnemiesCounter += TrySpawnWaveEnemies();
        if (CurrentSpawnedEnemiesCounter > 0)
        {
            WaveSpawnFailures = 0;
            SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::InProgress);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Wave %d generated no enemies, attempt %d"), CurrentWaveCount, WaveSpawnFailures + 1);
            if (++WaveSpawnFailures >= 3) EnterTerminalState(ERPGDemoSurvivalGameModeState::TeamDefeated);
        }
    }
    else if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::WaveCompleted &&
        TimePassedSinceStart >= WaveCompletedWaitTime)
    {
        TimePassedSinceStart = 0.f;
        ++CurrentWaveCount;
        if (HasFinishedAllWaves()) EnterTerminalState(ERPGDemoSurvivalGameModeState::AllWavesDone);
        else
        {
            SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::WaitSpawnNewWave);
            PreLoadNextWaveEnemies();
        }
    }
}

void ARPGDemoSurvivalGameMode::SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState NewState)
{
    if (IsMatchTerminal()) return;
    TimePassedSinceStart = 0.f;
    CurrentSurvivalGameModeState = NewState;
    OnSurvivalGameModeStateChanged.Broadcast(NewState);
    float Duration = 0.f;
    if (NewState == ERPGDemoSurvivalGameModeState::WaitSpawnNewWave) Duration = SpawnNewWaveWaitTime;
    else if (NewState == ERPGDemoSurvivalGameModeState::SpawningNewWave) Duration = SpawnEnemiesDelayTime;
    else if (NewState == ERPGDemoSurvivalGameModeState::WaveCompleted) Duration = WaveCompletedWaitTime;
    if (ARPGDemoGameState* State = GetGameState<ARPGDemoGameState>())
    {
        State->SetSurvivalSnapshot(NewState, CurrentWaveCount, TotalWavesToSpawn, Duration);
    }
}

void ARPGDemoSurvivalGameMode::EnterTerminalState(ERPGDemoSurvivalGameModeState TerminalState)
{
    if (IsMatchTerminal()) return;
    CancelAllRespawns();
    CancelWavePreload();
    CurrentSurvivalGameModeState = TerminalState;
    TimePassedSinceStart = 0.f;
    OnSurvivalGameModeStateChanged.Broadcast(TerminalState);
    if (ARPGDemoGameState* State = GetGameState<ARPGDemoGameState>())
    {
        State->SetSurvivalSnapshot(TerminalState, CurrentWaveCount, TotalWavesToSpawn,
            bMultiplayerMatch ? ResultDisplaySeconds : 0.f);
    }
    if (bMultiplayerMatch)
    {
        GetWorldTimerManager().ClearTimer(ReturnToLobbyTimer);
        GetWorldTimerManager().SetTimer(ReturnToLobbyTimer, this, &ThisClass::ReturnAllPlayersToLobby, ResultDisplaySeconds, false);
    }
}

bool ARPGDemoSurvivalGameMode::HasFinishedAllWaves() const { return CurrentWaveCount > TotalWavesToSpawn; }

bool ARPGDemoSurvivalGameMode::IsMatchTerminal() const
{
    return CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::AllWavesDone ||
        CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::TeamDefeated;
}

void ARPGDemoSurvivalGameMode::CancelWavePreload()
{
    for (const auto& Handle : WavePreloadHandles) if (Handle.IsValid()) Handle->CancelHandle();
    WavePreloadHandles.Empty();
    PendingWaveLoads = 0;
}

bool ARPGDemoSurvivalGameMode::IsWavePreloadReady() const
{
    const auto* Row = GetCurrentWaveSpawnerTableRow();
    if (!Row || Row->TotalEnemyToSpawnThisWave <= 0 || Row->EnemyWaveSpawnerDefinitions.IsEmpty()) return false;
    for (const auto& Info : Row->EnemyWaveSpawnerDefinitions)
        if (Info.SoftEnemyClassToSpawn.IsNull() || !PreloadedEnemyClassMap.Contains(Info.SoftEnemyClassToSpawn)) return false;
    return PendingWaveLoads == 0;
}

void ARPGDemoSurvivalGameMode::PreLoadNextWaveEnemies()
{
    if (IsMatchTerminal() || HasFinishedAllWaves()) return;
    CancelWavePreload();
    PreloadedEnemyClassMap.Empty();
    const auto* Row = GetCurrentWaveSpawnerTableRow();
    if (!Row) return;
    const int32 LoadingWave = CurrentWaveCount;
    for (const FRPGDemoEnemyWaveSpawnerInfo& Info : Row->EnemyWaveSpawnerDefinitions)
    {
        if (Info.SoftEnemyClassToSpawn.IsNull()) continue;
        ++PendingWaveLoads;
        const auto Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Info.SoftEnemyClassToSpawn.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this, Info, LoadingWave]()
            {
                if (IsMatchTerminal() || CurrentWaveCount != LoadingWave) return;
                PendingWaveLoads = FMath::Max(0, PendingWaveLoads - 1);
                if (UClass* LoadedClass = Info.SoftEnemyClassToSpawn.Get()) PreloadedEnemyClassMap.Emplace(Info.SoftEnemyClassToSpawn, LoadedClass);
                else UE_LOG(LogTemp, Error, TEXT("Could not load wave %d enemy %s"), LoadingWave, *Info.SoftEnemyClassToSpawn.ToString());
            }));
        if (Handle.IsValid()) WavePreloadHandles.Add(Handle);
        else --PendingWaveLoads;
    }
}

FRPGDemoEnemyWaveSpawnerTableRow* ARPGDemoSurvivalGameMode::GetCurrentWaveSpawnerTableRow() const
{
    const FName RowName(*FString::Printf(TEXT("Wave%d"), CurrentWaveCount));
    if (!EnemyWaveSpawnerDataTable) return nullptr;
    FRPGDemoEnemyWaveSpawnerTableRow* Row = EnemyWaveSpawnerDataTable->FindRow<FRPGDemoEnemyWaveSpawnerTableRow>(RowName, FString());
    if (!Row) UE_LOG(LogTemp, Error, TEXT("Could not find wave row %s"), *RowName.ToString());
    return Row;
}

int32 ARPGDemoSurvivalGameMode::TrySpawnWaveEnemies()
{
    if (IsMatchTerminal() || !IsWavePreloadReady()) return 0;
    if (TargetPointsArray.IsEmpty()) UGameplayStatics::GetAllActorsOfClass(this, ATargetPoint::StaticClass(), TargetPointsArray);
    if (TargetPointsArray.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("No wave target points in %s"), *GetWorld()->GetName()); return 0; }
    int32 Spawned = 0;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    for (const FRPGDemoEnemyWaveSpawnerInfo& Info : GetCurrentWaveSpawnerTableRow()->EnemyWaveSpawnerDefinitions)
    {
        if (Info.SoftEnemyClassToSpawn.IsNull()) continue;
        UClass* const* LoadedClass = PreloadedEnemyClassMap.Find(Info.SoftEnemyClassToSpawn);
        if (!LoadedClass || !*LoadedClass) continue;
        const int32 Count = FMath::RandRange(Info.MinPerSpawnCount, Info.MaxPerSpawnCount);
        for (int32 Index = 0; Index < Count && ShouldKeepSpawnEnemies(); ++Index)
        {
            AActor* Point = TargetPointsArray[FMath::RandHelper(TargetPointsArray.Num())];
            if (!IsValid(Point)) continue;
            FVector Location = Point->GetActorLocation();
            if (!UNavigationSystemV1::K2_GetRandomLocationInNavigableRadius(this, Point->GetActorLocation(), Location, 400.f))
                Location = Point->GetActorLocation();
            Location.Z += 150.f;
            if (ARPGDemoEnemyCharacter* Enemy = GetWorld()->SpawnActor<ARPGDemoEnemyCharacter>(*LoadedClass, Location, Point->GetActorRotation(), Params))
            {
                Enemy->OnDestroyed.AddUniqueDynamic(this, &ThisClass::OnEnemyDestroyed);
                ++Spawned; ++TotalSpawnedEnemiesThisWaveCounter;
            }
        }
    }
    return Spawned;
}

bool ARPGDemoSurvivalGameMode::ShouldKeepSpawnEnemies() const
{
    const auto* Row = GetCurrentWaveSpawnerTableRow();
    return !IsMatchTerminal() && Row && TotalSpawnedEnemiesThisWaveCounter < Row->TotalEnemyToSpawnThisWave;
}

void ARPGDemoSurvivalGameMode::OnEnemyDestroyed(AActor*)
{
    if (IsMatchTerminal() || GetWorld()->bIsTearingDown) return;
    CurrentSpawnedEnemiesCounter = FMath::Max(0, CurrentSpawnedEnemiesCounter - 1);
    if (ShouldKeepSpawnEnemies())
    {
        CurrentSpawnedEnemiesCounter += TrySpawnWaveEnemies();
        if (CurrentSpawnedEnemiesCounter == 0) SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::SpawningNewWave);
    }
    else if (CurrentSpawnedEnemiesCounter == 0)
    {
        TotalSpawnedEnemiesThisWaveCounter = 0;
        SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::WaveCompleted);
    }
}

void ARPGDemoSurvivalGameMode::RegisterSpawnedEnemies(const TArray<ARPGDemoEnemyCharacter*>& Enemies)
{
    for (ARPGDemoEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy) { ++CurrentSpawnedEnemiesCounter; Enemy->OnDestroyed.AddUniqueDynamic(this, &ThisClass::OnEnemyDestroyed); }
    }
}

void ARPGDemoSurvivalGameMode::NotifyPlayerDied(AController* DeadController)
{
    if (!HasAuthority() || !DeadController || IsMatchTerminal()) return;
    ARPGDemoPlayerState* PlayerState = DeadController->GetPlayerState<ARPGDemoPlayerState>();
    APawn* OldPawn = DeadController->GetPawn();
    if (!PlayerState || PlayerState->LifeState == ERPGDemoPlayerLifeState::Dead || !OldPawn) return;

    if (ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(OldPawn)) Hero->BeginCharacterDeath();
    URPGDemoAbilitySystemComponent* ASC = PlayerState->GetRPGDemoAbilitySystemComponent();
    if (ASC)
    {
        ASC->CancelAllAbilities();
        TArray<FGameplayAbilitySpecHandle> PawnAbilityHandles;
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (Spec.SourceObject.Get() == OldPawn) PawnAbilityHandles.Add(Spec.Handle);
        }
        for (const FGameplayAbilitySpecHandle Handle : PawnAbilityHandles) ASC->ClearAbility(Handle);
    }
    if (ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(OldPawn))
    {
        Hero->GetCharacterMovement()->StopMovementImmediately();
        Hero->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    const double RespawnAt = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + RespawnDelaySeconds;
    PlayerState->SetLifeState(ERPGDemoPlayerLifeState::Dead, bMultiplayerMatch ? RespawnAt : 0.0);
    DeadController->UnPossess();
    if (APlayerController* PC = Cast<APlayerController>(DeadController)) PC->StartSpectatingOnly();

    if (!bMultiplayerMatch)
    {
        EnterTerminalState(ERPGDemoSurvivalGameModeState::TeamDefeated);
        return;
    }

    RecalculateLivingPlayers();
    if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::TeamDefeated) return;
    ScheduleRespawn(PlayerState, OldPawn, RespawnDelaySeconds);
    UpdateSpectators();
}

void ARPGDemoSurvivalGameMode::ScheduleRespawn(ARPGDemoPlayerState* PlayerState, APawn* OldPawn, float DelaySeconds)
{
    FTimerHandle& Timer = RespawnTimers.FindOrAdd(PlayerState);
    GetWorldTimerManager().SetTimer(Timer,
        FTimerDelegate::CreateWeakLambda(this, [this, WeakState = TWeakObjectPtr<ARPGDemoPlayerState>(PlayerState), WeakPawn = TWeakObjectPtr<APawn>(OldPawn)]()
        {
            RespawnPlayer(WeakState, WeakPawn);
        }), DelaySeconds, false);
}

AActor* ARPGDemoSurvivalGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (AActor* Start = Super::ChoosePlayerStart_Implementation(Player)) return Start;
    const ARPGDemoPlayerState* State = Player ? Player->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
    // Keep the nearby-space search anchored to a real spawn point when every
    // start is blocked; GameModeBase otherwise falls back to world origin.
    if (State && State->LifeState == ERPGDemoPlayerLifeState::Dead)
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) return *It;
    return nullptr;
}

APawn* ARPGDemoSurvivalGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
    const ARPGDemoPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
    if (!State || State->LifeState != ERPGDemoPlayerLifeState::Dead)
        return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);

    UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
    const APawn* PawnTemplate = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
    if (!PawnTemplate) return nullptr;
    const FRotator Rotation = SpawnTransform.Rotator();
    const auto TryLocation = [this, NewPlayer, PawnTemplate, &Rotation](const FVector& Candidate) -> APawn*
    {
        FVector Location = Candidate;
        if (!GetWorld()->FindTeleportSpot(PawnTemplate, Location, Rotation) ||
            FVector::DistSquared(Location, Candidate) > FMath::Square(200.f)) return nullptr;
        return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, FTransform(Rotation, Location));
    };
    if (APawn* Pawn = TryLocation(SpawnTransform.GetLocation())) return Pawn;

    // A cached PlayerStart can be occupied by the boss or a teammate. Search
    // walkable nearby positions without forcing a pawn into blocking geometry.
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const UCapsuleComponent* Capsule = PawnTemplate->FindComponentByClass<UCapsuleComponent>();
    if (!Navigation || !Capsule) return nullptr;
    TArray<FVector> Origins{SpawnTransform.GetLocation()};
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) Origins.AddUnique(It->GetActorLocation());
    for (const FVector& Origin : Origins)
        for (const float Radius : {200.f, 400.f, 800.f, 1200.f})
            for (int32 Direction = 0; Direction < 8; ++Direction)
            {
                const float Angle = Direction * PI / 4.f;
                const FVector Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
                FNavLocation Ground;
                if (!Navigation->ProjectPointToNavigation(Candidate, Ground, FVector(100.f, 100.f, 250.f))) continue;
                const FVector Location = Ground.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
                if (APawn* Pawn = TryLocation(Location))
                {
                    UE_LOG(LogTemp, Log, TEXT("Respawn relocated for %s from %s to %s"),
                        *State->GetName(), *SpawnTransform.GetLocation().ToString(), *Pawn->GetActorLocation().ToString());
                    return Pawn;
                }
            }
    return nullptr;
}

void ARPGDemoSurvivalGameMode::RespawnPlayer(TWeakObjectPtr<ARPGDemoPlayerState> PlayerState, TWeakObjectPtr<APawn> OldPawn)
{
    RespawnTimers.Remove(PlayerState);
    if (!PlayerState.IsValid() || CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::TeamDefeated ||
        CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::AllWavesDone) return;
    AController* Controller = Cast<AController>(PlayerState->GetOwner());
    if (!Controller) return;
    if (OldPawn.IsValid()) OldPawn->Destroy();
    // StartSpectatingOnly also marks PlayerState as spectator-only. Leaving the
    // controller state alone does not clear that flag, so RestartPlayer refuses
    // to spawn the new pawn. Clear death before its startup abilities run.
    PlayerState->SetIsOnlyASpectator(false);
    PlayerState->SetIsSpectator(false);
    // Ask the engine to choose an unoccupied start instead of reusing the last
    // spawn point unconditionally after every death.
    Controller->StartSpot.Reset();
    URPGDemoAbilitySystemComponent* ASC = PlayerState->GetRPGDemoAbilitySystemComponent();
    if (ASC) ASC->SetLooseGameplayTagCount(RPGDemoGameplayTags::Shared_Status_Dead, 0, EGameplayTagReplicationState::TagAndCountToAll);
    Controller->ChangeState(NAME_Playing);
    URPGDemoAttributeSet* Attributes = PlayerState->GetRPGDemoAttributeSet();
    if (ASC && Attributes)
    {
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), Attributes->GetMaxHealth());
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentRageAttribute(), 0.f);
        Attributes->SynchronizeRageStatus();
    }
    RestartPlayer(Controller);
    if (!IsValid(Controller->GetPawn()))
    {
        // Spawn failure is not a team wipe. Keep this player dead/spectating
        // and retry; the normal living-player check still handles a real wipe.
        if (ASC) ASC->SetLooseGameplayTagCount(RPGDemoGameplayTags::Shared_Status_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
        if (APlayerController* PC = Cast<APlayerController>(Controller)) PC->StartSpectatingOnly();
        const double RetryAt = GetGameState<ARPGDemoGameState>()->GetServerWorldTimeSeconds() + RPGDemoMatch::RespawnRetrySeconds;
        PlayerState->SetLifeState(ERPGDemoPlayerLifeState::Dead, RetryAt);
        ScheduleRespawn(PlayerState.Get(), nullptr, RPGDemoMatch::RespawnRetrySeconds);
        UpdateSpectators();
        UE_LOG(LogTemp, Warning, TEXT("Respawn deferred for %s: no pawn could be spawned; retrying in %.1fs (wave %d)"),
            *PlayerState->GetName(), RPGDemoMatch::RespawnRetrySeconds, CurrentWaveCount);
        return;
    }
    PlayerState->SetLifeState(ERPGDemoPlayerLifeState::Alive, 0.0);
    UpdateSpectators();
}

void ARPGDemoSurvivalGameMode::CancelAllRespawns()
{
    for (TPair<TWeakObjectPtr<ARPGDemoPlayerState>, FTimerHandle>& Pair : RespawnTimers)
    {
        GetWorldTimerManager().ClearTimer(Pair.Value);
        if (Pair.Key.IsValid()) Pair.Key->SetLifeState(ERPGDemoPlayerLifeState::Dead, 0.0);
    }
    RespawnTimers.Empty();
}

APawn* ARPGDemoSurvivalGameMode::FindLivingPawn(const ARPGDemoPlayerState* Excluded) const
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        const ARPGDemoPlayerState* State = PC ? PC->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
        if (State && State != Excluded && State->LifeState == ERPGDemoPlayerLifeState::Alive && PC->GetPawn()) return PC->GetPawn();
    }
    return nullptr;
}

void ARPGDemoSurvivalGameMode::UpdateSpectators()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        ARPGDemoPlayerState* State = PC ? PC->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
        if (State && State->LifeState == ERPGDemoPlayerLifeState::Dead)
        {
            if (APawn* Target = FindLivingPawn(State)) PC->ClientSetViewTarget(Target, FViewTargetTransitionParams());
        }
    }
}

void ARPGDemoSurvivalGameMode::RecalculateLivingPlayers()
{
    if (IsMatchTerminal()) return;
    int32 Connected = 0;
    int32 Living = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        const ARPGDemoPlayerState* State = PC ? PC->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
        if (!State) continue;
        ++Connected;
        if (State->LifeState == ERPGDemoPlayerLifeState::Alive) ++Living;
    }
    if (Connected == 0) ReturnAllPlayersToLobby();
    else if (Living == 0) EnterTerminalState(ERPGDemoSurvivalGameModeState::TeamDefeated);
}

void ARPGDemoSurvivalGameMode::Logout(AController* Exiting)
{
    if (ARPGDemoPlayerState* State = Exiting ? Exiting->GetPlayerState<ARPGDemoPlayerState>() : nullptr)
    {
        if (FTimerHandle* Timer = RespawnTimers.Find(State)) GetWorldTimerManager().ClearTimer(*Timer);
        RespawnTimers.Remove(State);
        // A dead controller is unpossessed, so the normal Logout pawn cleanup cannot find its corpse.
        for (TActorIterator<ARPGDemoHeroCharacter> It(GetWorld()); It; ++It)
            if (It->GetRPGDemoAbilitySystemComponent() == State->GetRPGDemoAbilitySystemComponent() && It->IsDeathPresentationActive()) It->Destroy();
    }
    Super::Logout(Exiting);
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        RecalculateLivingPlayers();
        UpdateSpectators();
    }));
}

void ARPGDemoSurvivalGameMode::ReturnAllPlayersToLobby()
{
    if (HasAuthority() && bMultiplayerMatch && GetWorld()) GetWorld()->ServerTravel(RPGDemoMatch::LobbyTravel, true);
}


void ARPGDemoSurvivalGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if (ARPGDemoPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr)
    {
        if (!InitializedMatchPlayers.Contains(State))
        {
            InitializedMatchPlayers.Add(State);
            State->ResetForNewMatch();
        }
    }
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void ARPGDemoSurvivalGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelAllRespawns();
    CancelWavePreload();
    GetWorldTimerManager().ClearTimer(ReturnToLobbyTimer);
    Super::EndPlay(EndPlayReason);
}
