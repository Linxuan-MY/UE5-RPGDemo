// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "HeroCombatComponent.generated.h"

class ARPGDemoHeroWeapon;
class UInputMappingContext;
class UAnimInstance;

/**
 * 
 */
UCLASS()
class RPGDEMO_API UHeroCombatComponent : public UPawnCombatComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	ARPGDemoHeroWeapon* GetHeroCarriedWeaponByTag(FGameplayTag InWeaponTag) const;

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	ARPGDemoHeroWeapon* GetHeroCurrentEquippedWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat")
	float GetHeroCurrentEquippedWeaponDamageAtLevel(float InLevel) const;

	virtual void OnHitTargetActor(AActor* HitActor) override;
	virtual void OnWeaponPulledFromTargetActor(AActor* InteractedActor) override;
    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat") void EquipHeroWeapon(ARPGDemoHeroWeapon* Weapon, int32 AbilityLevel);
    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Combat") void UnequipHeroWeapon();
    virtual void RefreshEquippedWeaponPresentation() override;
private:
    TWeakObjectPtr<ARPGDemoHeroWeapon> PresentedWeapon;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> AppliedWeaponMapping;
    UPROPERTY(Transient) TSubclassOf<UAnimInstance> AppliedWeaponAnimLayer;
};
