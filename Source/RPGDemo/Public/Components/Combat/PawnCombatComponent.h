// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "PawnCombatComponent.generated.h"

class ARPGDemoWeaponBase;

/**
 * 
 */
UCLASS()
class RPGDEMO_API UPawnCombatComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	void RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ARPGDemoWeaponBase* InWeaponToRegister,bool bReigsterAsEquippedWeapon = false);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	ARPGDemoWeaponBase* GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const;

	UPROPERTY(BlueprintReadWrite, Category = "RPGDemo|Combat")
	FGameplayTag CurrentlyEquippedWeaponTag;

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	ARPGDemoWeaponBase* GetCharacterCurrentlyEquippedWeapon() const;

private:
	TMap<FGameplayTag, ARPGDemoWeaponBase*> CharacterCarriedWeaponMap;

	
};
