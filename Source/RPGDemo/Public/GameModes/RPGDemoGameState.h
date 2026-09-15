// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoGameState.generated.h"

UENUM(BlueprintType)
enum class ERPGDemoSurvivalGameModeState : uint8
{
    WaitSpawnNewWave,
    SpawningNewWave,
    InProgress,
    WaveCompleted,
    AllWavesDone,
    TeamDefeated
};

USTRUCT(BlueprintType)
struct FRPGDemoSurvivalSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ERPGDemoSurvivalGameModeState State = ERPGDemoSurvivalGameModeState::WaitSpawnNewWave;
    UPROPERTY(BlueprintReadOnly) int32 CurrentWave = 1;
    UPROPERTY(BlueprintReadOnly) int32 TotalWaves = 0;
    UPROPERTY(BlueprintReadOnly) double StateEndServerTime = 0.0;
    UPROPERTY(BlueprintReadOnly) int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPGDemoSurvivalStateChanged, ERPGDemoSurvivalGameModeState, CurrentState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPGDemoSurvivalSnapshotChanged, const FRPGDemoSurvivalSnapshot&, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRPGDemoLobbyPlayersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPGDemoLobbyDifficultyChanged, ERPGDemoGameDifficulty, Difficulty);

UCLASS()
class RPGDEMO_API ARPGDemoGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ARPGDemoGameState();

    UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Survival") FOnRPGDemoSurvivalStateChanged OnSurvivalStateChanged;
    UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Survival") FOnRPGDemoSurvivalSnapshotChanged OnSurvivalSnapshotChanged;
    UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer") FOnRPGDemoLobbyPlayersChanged OnLobbyPlayersChanged;
    UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer") FOnRPGDemoLobbyDifficultyChanged OnLobbyDifficultyChanged;

    UPROPERTY(ReplicatedUsing = OnRep_LobbyDifficulty, BlueprintReadOnly, Category = "RPGDemo|Multiplayer")
    ERPGDemoGameDifficulty LobbyDifficulty = ERPGDemoGameDifficulty::Normal;

    UPROPERTY(ReplicatedUsing = OnRep_SurvivalSnapshot, BlueprintReadOnly, Category = "RPGDemo|Survival")
    FRPGDemoSurvivalSnapshot SurvivalSnapshot;

    void SetSurvivalSnapshot(ERPGDemoSurvivalGameModeState State, int32 CurrentWave, int32 TotalWaves, float DurationSeconds);
    void SetSurvivalState(ERPGDemoSurvivalGameModeState NewState, float DurationSeconds = 0.f);
    void SetWaveProgress(int32 InCurrentWave, int32 InTotalWaves);
    void SetLobbyDifficulty(ERPGDemoGameDifficulty NewDifficulty);
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION() void OnRep_SurvivalSnapshot();
    UFUNCTION() void OnRep_LobbyDifficulty();

private:
    void BroadcastSurvivalSnapshot();
};
