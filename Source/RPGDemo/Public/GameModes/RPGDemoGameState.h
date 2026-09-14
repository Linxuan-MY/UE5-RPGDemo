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
	PlayerDied
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPGDemoSurvivalStateChanged, ERPGDemoSurvivalGameModeState, CurrentState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRPGDemoLobbyPlayersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPGDemoLobbyDifficultyChanged, ERPGDemoGameDifficulty, Difficulty);

UCLASS()
class RPGDEMO_API ARPGDemoGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARPGDemoGameState();

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "RPGDemo|Survival")
	FOnRPGDemoSurvivalStateChanged OnSurvivalStateChanged;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "RPGDemo|Multiplayer")
	FOnRPGDemoLobbyPlayersChanged OnLobbyPlayersChanged;

	UPROPERTY(BlueprintAssignable, BlueprintReadOnly, Category = "RPGDemo|Multiplayer")
	FOnRPGDemoLobbyDifficultyChanged OnLobbyDifficultyChanged;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyDifficulty, BlueprintReadOnly, Category = "RPGDemo|Multiplayer")
	ERPGDemoGameDifficulty LobbyDifficulty = ERPGDemoGameDifficulty::Normal;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivalState, BlueprintReadOnly, Category = "RPGDemo|Survival")
	ERPGDemoSurvivalGameModeState SurvivalState = ERPGDemoSurvivalGameModeState::WaitSpawnNewWave;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RPGDemo|Survival")
	int32 CurrentWaveCount = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RPGDemo|Survival")
	int32 TotalWavesToSpawn = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RPGDemo|Survival")
	float StateStartServerTime = 0.f;

	void SetSurvivalState(ERPGDemoSurvivalGameModeState NewState);
	void SetWaveProgress(int32 InCurrentWave, int32 InTotalWaves);
	void SetLobbyDifficulty(ERPGDemoGameDifficulty NewDifficulty);
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_SurvivalState();

	UFUNCTION()
	void OnRep_LobbyDifficulty();
};
