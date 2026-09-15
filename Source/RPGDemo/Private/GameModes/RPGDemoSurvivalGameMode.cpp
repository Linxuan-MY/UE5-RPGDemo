// Fill out your copyright notice in the Description page of Project Settings.

#include "GameModes/RPGDemoSurvivalGameMode.h"

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
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
    checkf(EnemyWaveSpawnerDataTable, TEXT("Forgot to assign a valid data table"));
    TotalWavesToSpawn = EnemyWaveSpawnerDataTable->GetRowNames().Num();
    if (ARPGDemoGameState* State = GetGameState<ARPGDemoGameState>())
    {
        State->SetWaveProgress(CurrentWaveCount, TotalWavesToSpawn);
    }
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
        TimePassedSinceStart = 0.f;
        CurrentSpawnedEnemiesCounter += TrySpawnWaveEnemies();
        SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::InProgress);
    }
    else if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::WaveCompleted &&
        TimePassedSinceStart >= WaveCompletedWaitTime)
    {
        TimePassedSinceStart = 0.f;
        ++CurrentWaveCount;
        if (ARPGDemoGameState* State = GetGameState<ARPGDemoGameState>()) State->SetWaveProgress(CurrentWaveCount, TotalWavesToSpawn);
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
    if (TerminalState == ERPGDemoSurvivalGameModeState::TeamDefeated) CancelAllRespawns();
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

void ARPGDemoSurvivalGameMode::PreLoadNextWaveEnemies()
{
    if (HasFinishedAllWaves()) return;
    PreloadedEnemyClassMap.Empty();
    for (const FRPGDemoEnemyWaveSpawnerInfo& Info : GetCurrentWaveSpawnerTableRow()->EnemyWaveSpawnerDefinitions)
    {
        if (Info.SoftEnemyClassToSpawn.IsNull()) continue;
        UAssetManager::GetStreamableManager().RequestAsyncLoad(Info.SoftEnemyClassToSpawn.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this, Info]()
            {
                if (UClass* LoadedClass = Info.SoftEnemyClassToSpawn.Get()) PreloadedEnemyClassMap.Emplace(Info.SoftEnemyClassToSpawn, LoadedClass);
            }));
    }
}

FRPGDemoEnemyWaveSpawnerTableRow* ARPGDemoSurvivalGameMode::GetCurrentWaveSpawnerTableRow() const
{
    const FName RowName(*FString::Printf(TEXT("Wave%d"), CurrentWaveCount));
    FRPGDemoEnemyWaveSpawnerTableRow* Row = EnemyWaveSpawnerDataTable->FindRow<FRPGDemoEnemyWaveSpawnerTableRow>(RowName, FString());
    checkf(Row, TEXT("Could not find row %s"), *RowName.ToString());
    return Row;
}

int32 ARPGDemoSurvivalGameMode::TrySpawnWaveEnemies()
{
    if (TargetPointsArray.IsEmpty()) UGameplayStatics::GetAllActorsOfClass(this, ATargetPoint::StaticClass(), TargetPointsArray);
    checkf(!TargetPointsArray.IsEmpty(), TEXT("No target points in %s"), *GetWorld()->GetName());
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
            FVector Location;
            UNavigationSystemV1::K2_GetRandomLocationInNavigableRadius(this, Point->GetActorLocation(), Location, 400.f);
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
    return TotalSpawnedEnemiesThisWaveCounter < GetCurrentWaveSpawnerTableRow()->TotalEnemyToSpawnThisWave;
}

void ARPGDemoSurvivalGameMode::OnEnemyDestroyed(AActor*)
{
    CurrentSpawnedEnemiesCounter = FMath::Max(0, CurrentSpawnedEnemiesCounter - 1);
    if (ShouldKeepSpawnEnemies()) CurrentSpawnedEnemiesCounter += TrySpawnWaveEnemies();
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
    if (!HasAuthority() || !DeadController || CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::TeamDefeated) return;
    ARPGDemoPlayerState* PlayerState = DeadController->GetPlayerState<ARPGDemoPlayerState>();
    APawn* OldPawn = DeadController->GetPawn();
    if (!PlayerState || PlayerState->LifeState == ERPGDemoPlayerLifeState::Dead || !OldPawn) return;

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
        Hero->GetCharacterMovement()->DisableMovement();
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
    FTimerHandle& Timer = RespawnTimers.FindOrAdd(PlayerState);
    GetWorldTimerManager().SetTimer(Timer,
        FTimerDelegate::CreateWeakLambda(this, [this, WeakState = TWeakObjectPtr<ARPGDemoPlayerState>(PlayerState), WeakPawn = TWeakObjectPtr<APawn>(OldPawn)]()
        {
            RespawnPlayer(WeakState, WeakPawn);
        }), RespawnDelaySeconds, false);
    UpdateSpectators();
}

void ARPGDemoSurvivalGameMode::RespawnPlayer(TWeakObjectPtr<ARPGDemoPlayerState> PlayerState, TWeakObjectPtr<APawn> OldPawn)
{
    RespawnTimers.Remove(PlayerState);
    if (!PlayerState.IsValid() || CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::TeamDefeated ||
        CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::AllWavesDone) return;
    AController* Controller = Cast<AController>(PlayerState->GetOwner());
    if (!Controller) return;
    if (OldPawn.IsValid()) OldPawn->Destroy();
    Controller->ChangeState(NAME_Playing);
    RestartPlayer(Controller);
    URPGDemoAbilitySystemComponent* ASC = PlayerState->GetRPGDemoAbilitySystemComponent();
    URPGDemoAttributeSet* Attributes = PlayerState->GetRPGDemoAttributeSet();
    if (ASC && Attributes)
    {
        ASC->RemoveLooseGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead);
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), Attributes->GetMaxHealth());
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentRageAttribute(), 0.f);
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
