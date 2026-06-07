// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/RPGDemoEnemyGameplayAbility.h"
#include "Characters/RPGDemoEnemyCharacter.h"

ARPGDemoEnemyCharacter* URPGDemoEnemyGameplayAbility::GetEnemyCharacterFromActorInfo()
{
    if(!CachedRPGDemoEnemyCharacter.IsValid())
	{
		CachedRPGDemoEnemyCharacter = Cast<ARPGDemoEnemyCharacter>(CurrentActorInfo->AvatarActor);
	}
	return CachedRPGDemoEnemyCharacter.IsValid() ? CachedRPGDemoEnemyCharacter.Get() : nullptr;
}

UEnemyCombatComponent* URPGDemoEnemyGameplayAbility::GetEnemyCombatComponentFromActorInfo()
{
	return GetEnemyCharacterFromActorInfo()->GetEnemyCombatComponent();
}
