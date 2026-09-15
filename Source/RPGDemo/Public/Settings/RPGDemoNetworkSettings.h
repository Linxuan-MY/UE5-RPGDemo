// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RPGDemoNetworkSettings.generated.h"

UCLASS(Config=Game, DefaultConfig)
class RPGDEMO_API URPGDemoNetworkSettings : public UObject
{
    GENERATED_BODY()

public:
    URPGDemoNetworkSettings();

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RPGDemo|Network")
    FString DedicatedServerEndpoint;
};
