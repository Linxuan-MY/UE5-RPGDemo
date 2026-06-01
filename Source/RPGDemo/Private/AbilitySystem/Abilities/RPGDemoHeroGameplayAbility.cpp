// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Controllers/RPGDemoHeroController.h"

ARPGDemoHeroCharacter* URPGDemoHeroGameplayAbility::GetHeroCharacterFromActorInfo()
{
	if(!CachedRPGDemoHeroCharacter.IsValid())
	{
		CachedRPGDemoHeroCharacter = Cast<ARPGDemoHeroCharacter>(CurrentActorInfo->AvatarActor);
	}

	return CachedRPGDemoHeroCharacter.IsValid() ? CachedRPGDemoHeroCharacter.Get() : nullptr;
}

ARPGDemoHeroController* URPGDemoHeroGameplayAbility::GetHeroControllerFromActorInfo()
{
	if(!CachedRPGDemoHeroController.IsValid())
	{
		CachedRPGDemoHeroController = Cast<ARPGDemoHeroController>(CurrentActorInfo->PlayerController);
	}
	return CachedRPGDemoHeroController.IsValid() ? CachedRPGDemoHeroController.Get() : nullptr;
}

UHeroCombatComponent* URPGDemoHeroGameplayAbility::GetHeroCombatComponentFromActorInfo()
{
	return GetHeroCharacterFromActorInfo()->GetHeroCombatComponent();
}
