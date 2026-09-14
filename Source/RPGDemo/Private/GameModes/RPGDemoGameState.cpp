// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoGameState.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoGameInstance.h"

ARPGDemoGameState::ARPGDemoGameState()
{
	bReplicates = true;
}

void ARPGDemoGameState::SetSurvivalState(ERPGDemoSurvivalGameModeState NewState)
{
	if (HasAuthority())
	{
		SurvivalState = NewState;
		StateStartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		OnSurvivalStateChanged.Broadcast(SurvivalState);
	}
}

void ARPGDemoGameState::SetWaveProgress(int32 InCurrentWave, int32 InTotalWaves)
{
	if (HasAuthority())
	{
		CurrentWaveCount = InCurrentWave;
		TotalWavesToSpawn = InTotalWaves;
	}
}

void ARPGDemoGameState::SetLobbyDifficulty(ERPGDemoGameDifficulty NewDifficulty)
{
	if (HasAuthority() && LobbyDifficulty != NewDifficulty)
	{
		LobbyDifficulty = NewDifficulty;
		OnLobbyDifficultyChanged.Broadcast(LobbyDifficulty);
		if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
		ForceNetUpdate();
	}
}

void ARPGDemoGameState::OnRep_SurvivalState()
{
	OnSurvivalStateChanged.Broadcast(SurvivalState);
}

void ARPGDemoGameState::OnRep_LobbyDifficulty()
{
	OnLobbyDifficultyChanged.Broadcast(LobbyDifficulty);
	if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
}

void ARPGDemoGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	OnLobbyPlayersChanged.Broadcast();
	if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
}

void ARPGDemoGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	OnLobbyPlayersChanged.Broadcast();
	if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
}

void ARPGDemoGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoGameState, SurvivalState);
	DOREPLIFETIME(ARPGDemoGameState, CurrentWaveCount);
	DOREPLIFETIME(ARPGDemoGameState, TotalWavesToSpawn);
	DOREPLIFETIME(ARPGDemoGameState, StateStartServerTime);
	DOREPLIFETIME(ARPGDemoGameState, LobbyDifficulty);
}
