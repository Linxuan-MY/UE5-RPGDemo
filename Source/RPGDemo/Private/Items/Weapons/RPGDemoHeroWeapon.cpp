// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/Weapons/RPGDemoHeroWeapon.h"

void ARPGDemoHeroWeapon::AssignGrantedAbilitySpecHandles(const TArray<FGameplayAbilitySpecHandle>& InSpecHandles)
{
	GrantedAbilitySpecHandles = InSpecHandles;
}

TArray<FGameplayAbilitySpecHandle> ARPGDemoHeroWeapon::GetGrantedAbilitySpecHandles() const
{
	return GrantedAbilitySpecHandles;
}
