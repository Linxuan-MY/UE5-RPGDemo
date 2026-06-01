// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "RPGDemoTypes/RPGDemoStructTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "RPGDemoHeroWeapon.generated.h"

/**
 * 
 */
UCLASS()
class RPGDEMO_API ARPGDemoHeroWeapon : public ARPGDemoWeaponBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponData")
	FRPGDemoHeroWeaponData HeroWeaponData;

	UFUNCTION(BlueprintCallable)
	void AssignGrantedAbilitySpecHandles(const TArray<FGameplayAbilitySpecHandle>& InSpecHandles);

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAbilitySpecHandle> GetGrantedAbilitySpecHandles() const;

private:
	TArray<FGameplayAbilitySpecHandle> GrantedAbilitySpecHandles;
	
};
