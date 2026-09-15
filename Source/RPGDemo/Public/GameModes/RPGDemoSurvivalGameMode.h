// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "GameModes/RPGDemoGameState.h"
#include "RPGDemoSurvivalGameMode.generated.h"

class ARPGDemoEnemyCharacter;
class ARPGDemoPlayerState;

USTRUCT(BlueprintType)
struct FRPGDemoEnemyWaveSpawnerInfo
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TSoftClassPtr<ARPGDemoEnemyCharacter> SoftEnemyClassToSpawn;
    UPROPERTY(EditAnywhere) int32 MinPerSpawnCount = 1;
    UPROPERTY(EditAnywhere) int32 MaxPerSpawnCount = 3;
};

USTRUCT(BlueprintType)
struct FRPGDemoEnemyWaveSpawnerTableRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TArray<FRPGDemoEnemyWaveSpawnerInfo> EnemyWaveSpawnerDefinitions;
    UPROPERTY(EditAnywhere) int32 TotalEnemyToSpawnThisWave = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurvivalGameModeStateChangedDelegate, ERPGDemoSurvivalGameModeState, CurrentState);

UCLASS()
class RPGDEMO_API ARPGDemoSurvivalGameMode : public ARPGDemoBaseGameMode
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable) void RegisterSpawnedEnemies(const TArray<ARPGDemoEnemyCharacter*>& InEnemiesToRegister);
    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Survival") void NotifyPlayerDied(AController* DeadController);

protected:
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void Logout(AController* Exiting) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

private:
    void SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState NewState);
    void EnterTerminalState(ERPGDemoSurvivalGameModeState TerminalState);
    bool HasFinishedAllWaves() const;
    void PreLoadNextWaveEnemies();
    FRPGDemoEnemyWaveSpawnerTableRow* GetCurrentWaveSpawnerTableRow() const;
    int32 TrySpawnWaveEnemies();
    bool ShouldKeepSpawnEnemies() const;
    UFUNCTION() void OnEnemyDestroyed(AActor* DestroyedActor);

    void RespawnPlayer(TWeakObjectPtr<ARPGDemoPlayerState> PlayerState, TWeakObjectPtr<APawn> OldPawn);
    void CancelAllRespawns();
    void RecalculateLivingPlayers();
    APawn* FindLivingPawn(const ARPGDemoPlayerState* ExcludedPlayerState = nullptr) const;
    void UpdateSpectators();
    void ReturnAllPlayersToLobby();

    UPROPERTY() ERPGDemoSurvivalGameModeState CurrentSurvivalGameModeState = ERPGDemoSurvivalGameModeState::WaitSpawnNewWave;
    UPROPERTY(BlueprintAssignable, BlueprintCallable) FOnSurvivalGameModeStateChangedDelegate OnSurvivalGameModeStateChanged;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaveDefinition", meta = (AllowPrivateAccess = "true")) TObjectPtr<UDataTable> EnemyWaveSpawnerDataTable;
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WaveDefinition", meta = (AllowPrivateAccess = "true")) int32 TotalWavesToSpawn = 0;
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WaveDefinition", meta = (AllowPrivateAccess = "true")) int32 CurrentWaveCount = 1;
    UPROPERTY() int32 CurrentSpawnedEnemiesCounter = 0;
    UPROPERTY() int32 TotalSpawnedEnemiesThisWaveCounter = 0;
    UPROPERTY() TArray<AActor*> TargetPointsArray;
    UPROPERTY() float TimePassedSinceStart = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaveDefinition", meta = (AllowPrivateAccess = "true")) float SpawnNewWaveWaitTime = 5.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "WaveDefinition", meta = (AllowPrivateAccess = "true")) float SpawnEnemiesDelayTime = 2.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaveDefinition", meta = (AllowPrivateAccess = "true")) float WaveCompletedWaitTime = 5.f;
    UPROPERTY(EditDefaultsOnly, Category = "RPGDemo|Survival") float RespawnDelaySeconds = 10.f;
    UPROPERTY(EditDefaultsOnly, Category = "RPGDemo|Survival") float ResultDisplaySeconds = 8.f;
    UPROPERTY() TMap<TSoftClassPtr<ARPGDemoEnemyCharacter>, UClass*> PreloadedEnemyClassMap;
    TMap<TWeakObjectPtr<ARPGDemoPlayerState>, FTimerHandle> RespawnTimers;
    FTimerHandle ReturnToLobbyTimer;
    bool bMultiplayerMatch = false;
};
