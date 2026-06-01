// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RPGDemoTypes/RPGDemoStructTypes.h"
#include "RPGDemoAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void OnAbilityInputPressed(const FGameplayTag InInputTag);
	void OnAbilityInputReleased(const FGameplayTag InInputTag);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Ability", meta = (ApplyLevel = "1"))
	void GrantHeroWeaponAbilities(const TArray<FRPGDemoHeroAbilitySet>& InDefaultWeaponAbilities, int32 ApplyLevel, TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilitySpecHandles);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Ability")
	void RemoveGrantedHeroWeaponAbilities(UPARAM(ref) TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove);

};
