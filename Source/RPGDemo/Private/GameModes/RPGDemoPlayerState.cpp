// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoGameInstance.h"

ARPGDemoPlayerState::ARPGDemoPlayerState()
{
	bReplicates = true;
}

void ARPGDemoPlayerState::OnRep_IsLobbyHost()
{
	if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
}

void ARPGDemoPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoPlayerState, bIsReady);
	DOREPLIFETIME(ARPGDemoPlayerState, bIsLobbyHost);
}
