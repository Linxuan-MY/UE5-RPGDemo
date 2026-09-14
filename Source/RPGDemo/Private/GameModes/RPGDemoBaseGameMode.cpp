// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/RPGDemoBaseGameMode.h"
#include "GameModes/RPGDemoGameState.h"
#include "GameModes/RPGDemoPlayerState.h"

ARPGDemoBaseGameMode::ARPGDemoBaseGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	GameStateClass = ARPGDemoGameState::StaticClass();
	PlayerStateClass = ARPGDemoPlayerState::StaticClass();
}

bool ARPGDemoBaseGameMode::SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate)
{
	if (GetNetMode() != NM_Standalone)
	{
		// The local pause widget still takes UI focus and disables only that player's
		// input. Never pause the authoritative world in a networked game.
		return false;
	}

	return Super::SetPause(PC, MoveTemp(CanUnpauseDelegate));
}
