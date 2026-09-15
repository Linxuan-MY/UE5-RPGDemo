// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoGameState.h"

#include "Net/UnrealNetwork.h"
#include "RPGDemoGameInstance.h"

ARPGDemoGameState::ARPGDemoGameState()
{
    bReplicates = true;
}

void ARPGDemoGameState::SetSurvivalSnapshot(
    ERPGDemoSurvivalGameModeState State, int32 CurrentWave, int32 TotalWaves, float DurationSeconds)
{
    if (!HasAuthority()) return;
    SurvivalSnapshot.State = State;
    SurvivalSnapshot.CurrentWave = CurrentWave;
    SurvivalSnapshot.TotalWaves = TotalWaves;
    SurvivalSnapshot.StateEndServerTime = DurationSeconds > 0.f
        ? GetServerWorldTimeSeconds() + DurationSeconds : 0.0;
    ++SurvivalSnapshot.Revision;
    BroadcastSurvivalSnapshot();
    ForceNetUpdate();
}

void ARPGDemoGameState::SetSurvivalState(ERPGDemoSurvivalGameModeState NewState, float DurationSeconds)
{
    SetSurvivalSnapshot(NewState, SurvivalSnapshot.CurrentWave, SurvivalSnapshot.TotalWaves, DurationSeconds);
}

void ARPGDemoGameState::SetWaveProgress(int32 InCurrentWave, int32 InTotalWaves)
{
    if (!HasAuthority()) return;
    SurvivalSnapshot.CurrentWave = InCurrentWave;
    SurvivalSnapshot.TotalWaves = InTotalWaves;
    ++SurvivalSnapshot.Revision;
    BroadcastSurvivalSnapshot();
    ForceNetUpdate();
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

void ARPGDemoGameState::BroadcastSurvivalSnapshot()
{
    OnSurvivalSnapshotChanged.Broadcast(SurvivalSnapshot);
    OnSurvivalStateChanged.Broadcast(SurvivalSnapshot.State);
}

void ARPGDemoGameState::OnRep_SurvivalSnapshot()
{
    BroadcastSurvivalSnapshot();
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
    DOREPLIFETIME(ARPGDemoGameState, SurvivalSnapshot);
    DOREPLIFETIME(ARPGDemoGameState, LobbyDifficulty);
}
