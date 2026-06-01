// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/RPGDemoBaseAnimInstance.h"
#include "RPGDemoHeroLinkedAnimLayer.generated.h"

class URPGDemoHeroAnimInstance;

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoHeroLinkedAnimLayer : public URPGDemoBaseAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe))
	URPGDemoHeroAnimInstance* GetHeroAnimInstance() const;
	
};
