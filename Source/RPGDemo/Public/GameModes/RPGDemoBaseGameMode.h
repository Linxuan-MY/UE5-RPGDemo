// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoBaseGameMode.generated.h"

class ARPGDemoGameState;
class ARPGDemoPlayerState;

/**
 *
 */
UCLASS()
class RPGDEMO_API ARPGDemoBaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARPGDemoBaseGameMode();

	/**
	 * A listen server shares its world with every connected player, so pausing that
	 * world from the host's local pause menu would freeze all clients. Multiplayer
	 * pause menus are UI/input-only; real world pause remains available in standalone.
	 */
	virtual bool SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	ERPGDemoGameDifficulty CurrentGameDifficulty;

public:
	FORCEINLINE ERPGDemoGameDifficulty GetCurrentGameDifficulty() const { return CurrentGameDifficulty; }
};
