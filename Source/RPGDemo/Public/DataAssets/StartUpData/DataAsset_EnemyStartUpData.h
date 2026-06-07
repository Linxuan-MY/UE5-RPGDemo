// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAssets/StartUpData/DataAsset_StartUpDataBase.h"
#include "DataAsset_EnemyStartUpData.generated.h"

class URPGDemoEnemyGameplayAbility;

/**
 * 
 */
UCLASS()
class RPGDEMO_API UDataAsset_EnemyStartUpData : public UDataAsset_StartUpDataBase
{
	GENERATED_BODY()

private:
	UPROPERTY(EditDefaultsOnly, Category = "StartUpData")
	TArray < TSubclassOf < URPGDemoEnemyGameplayAbility > > EnemyCombatAbilities;

public:
	virtual void GiveToAbilitySystemComponent(URPGDemoAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;
	
};
