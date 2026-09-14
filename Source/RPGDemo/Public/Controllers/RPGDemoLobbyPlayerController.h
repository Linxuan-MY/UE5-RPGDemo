// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoLobbyPlayerController.generated.h"

UCLASS()
class RPGDEMO_API ARPGDemoLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARPGDemoLobbyPlayerController();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "RPGDemo|Multiplayer")
	void ServerStartGame();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "RPGDemo|Multiplayer")
	void ServerSetLobbyDifficulty(ERPGDemoGameDifficulty Difficulty);
};
