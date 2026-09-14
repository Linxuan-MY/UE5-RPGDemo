// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoLobbyGameMode.h"
#include "Controllers/RPGDemoLobbyPlayerController.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "RPGDemoGameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "GameModes/RPGDemoGameState.h"

ARPGDemoLobbyGameMode::ARPGDemoLobbyGameMode()
{
	PlayerControllerClass = ARPGDemoLobbyPlayerController::StaticClass();
	bUseSeamlessTravel = true;
}

void ARPGDemoLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (ARPGDemoPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr)
	{
		State->bIsLobbyHost = GameState && GameState->PlayerArray.Num() == 1;
		State->ForceNetUpdate();
	}
	if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->OnLobbyMembersChanged.Broadcast();
}

void ARPGDemoLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->OnLobbyMembersChanged.Broadcast();
}

void ARPGDemoLobbyGameMode::StartGameForHost(APlayerController* RequestingPlayer)
{
	const ARPGDemoPlayerState* State = RequestingPlayer ? RequestingPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
	if (State && State->bIsLobbyHost)
	{
		const ARPGDemoGameState* RPGGameState = GetGameState<ARPGDemoGameState>();
		if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>())
		{
			GI->StartMultiplayerGame(RPGGameState ? RPGGameState->LobbyDifficulty : ERPGDemoGameDifficulty::Normal);
		}
	}
}

void ARPGDemoLobbyGameMode::SetDifficultyForHost(APlayerController* RequestingPlayer, ERPGDemoGameDifficulty Difficulty)
{
	const ARPGDemoPlayerState* State = RequestingPlayer ? RequestingPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
	const bool bValidDifficulty = Difficulty >= ERPGDemoGameDifficulty::Easy && Difficulty <= ERPGDemoGameDifficulty::ExtremelyHard;
	if (State && State->bIsLobbyHost && bValidDifficulty)
	{
		if (ARPGDemoGameState* RPGGameState = GetGameState<ARPGDemoGameState>()) RPGGameState->SetLobbyDifficulty(Difficulty);
	}
}
