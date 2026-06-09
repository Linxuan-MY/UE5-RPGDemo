// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Controllers/RPGDemoHeroController.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"

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

FGameplayEffectSpecHandle URPGDemoHeroGameplayAbility::MakeHeroDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag, int32 InUsedComboCount)
{
	check(EffectClass);

	FGameplayEffectContextHandle ContextHandle = GetRPGDemoAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	ContextHandle.SetAbility(this);
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());
	ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle EffectSpecHandle = GetRPGDemoAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
		EffectClass,
		GetAbilityLevel(),
		ContextHandle
	);

	EffectSpecHandle.Data->SetSetByCallerMagnitude(
		RPGDemoGameplayTags::Shared_SetByCaller_BaseDamage,
		InWeaponBaseDamage
	);

	if(InCurrentAttackTypeTag.IsValid())
	{
		EffectSpecHandle.Data->SetSetByCallerMagnitude(
			InCurrentAttackTypeTag,
			InUsedComboCount
		);
	}

	return EffectSpecHandle;
}
