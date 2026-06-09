// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/RPGDemoGameplayAbility.h"
#include "RPGDemoHeroGameplayAbility.generated.h"

class ARPGDemoHeroCharacter;
class ARPGDemoHeroController;

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoHeroGameplayAbility : public URPGDemoGameplayAbility
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	ARPGDemoHeroCharacter* GetHeroCharacterFromActorInfo();

	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	ARPGDemoHeroController* GetHeroControllerFromActorInfo();

	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	UHeroCombatComponent* GetHeroCombatComponentFromActorInfo();

	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	FGameplayEffectSpecHandle MakeHeroDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag, int32 InUsedComboCount);


private:
	TWeakObjectPtr<ARPGDemoHeroCharacter> CachedRPGDemoHeroCharacter;
	TWeakObjectPtr<ARPGDemoHeroController> CachedRPGDemoHeroController;
};
