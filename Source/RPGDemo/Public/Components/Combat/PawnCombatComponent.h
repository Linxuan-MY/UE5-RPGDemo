// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "PawnCombatComponent.generated.h"

class ARPGDemoWeaponBase;

UENUM(BlueprintType)
enum class EToggleDamageType : uint8
{
	CurrentEquippedWeapon,
	LeftHand,
	RightHand
};

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

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	void ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType = EToggleDamageType::CurrentEquippedWeapon);

	virtual void OnHitTargetActor(AActor* HitActor);
	virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor);

protected:
	TArray<AActor*> OverlappedActors;

private:
	TMap<FGameplayTag, ARPGDemoWeaponBase*> CharacterCarriedWeaponMap;

	
};
