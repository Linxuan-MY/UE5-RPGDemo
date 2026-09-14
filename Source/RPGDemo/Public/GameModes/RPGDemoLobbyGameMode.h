// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "RPGDemoLobbyGameMode.generated.h"

UCLASS()
class RPGDEMO_API ARPGDemoLobbyGameMode : public ARPGDemoBaseGameMode
{
	GENERATED_BODY()

public:
	ARPGDemoLobbyGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void StartGameForHost(APlayerController* RequestingPlayer);
	void SetDifficultyForHost(APlayerController* RequestingPlayer, ERPGDemoGameDifficulty Difficulty);
};
