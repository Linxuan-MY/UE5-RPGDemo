// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"

void URPGDemoAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag InInputTag)
{
	if(!InInputTag.IsValid()) return;

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.DynamicAbilityTags.HasTagExact(InInputTag)) continue;
		
		TryActivateAbility(AbilitySpec.Handle);
	}
}

void URPGDemoAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag InInputTag)
{

}
