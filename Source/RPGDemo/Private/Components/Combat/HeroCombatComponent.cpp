// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/HeroCombatComponent.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"

ARPGDemoHeroWeapon* UHeroCombatComponent::GetHeroCarriedWeaponByTag(FGameplayTag InWeaponTag) const
{
	return Cast<ARPGDemoHeroWeapon>(GetCharacterCarriedWeaponByTag(InWeaponTag));
}
