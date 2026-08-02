// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoSaveGame.generated.h"

/**
 *
 */
UCLASS()
class RPGDEMO_API URPGDemoSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	ERPGDemoGameDifficulty SavedGameDifficulty;
};
