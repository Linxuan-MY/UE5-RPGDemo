// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoBaseGameMode.generated.h"

/**
 *
 */
UCLASS()
class RPGDEMO_API ARPGDemoBaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARPGDemoBaseGameMode();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	ERPGDemoGameDifficulty CurrentGameDifficulty;

public:
	FORCEINLINE ERPGDemoGameDifficulty GetCurrentGameDifficulty() const { return CurrentGameDifficulty; }
};
