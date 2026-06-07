// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/RPGDemoGameplayAbility.h"
#include "RPGDemoEnemyGameplayAbility.generated.h"

class ARPGDemoEnemyCharacter;
class UEnemyCombatComponent;

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoEnemyGameplayAbility : public URPGDemoGameplayAbility
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	ARPGDemoEnemyCharacter* GetEnemyCharacterFromActorInfo();

	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	UEnemyCombatComponent* GetEnemyCombatComponentFromActorInfo();

private:
	TWeakObjectPtr<ARPGDemoEnemyCharacter> CachedRPGDemoEnemyCharacter;

};
