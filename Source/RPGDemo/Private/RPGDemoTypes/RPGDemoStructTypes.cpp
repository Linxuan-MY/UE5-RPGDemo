// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGDemoTypes/RPGDemoStructTypes.h"
#include "AbilitySystem/Abilities/RPGDemoGameplayAbility.h"

bool FRPGDemoHeroAbilitySet::IsValid() const
{
	return InputTag.IsValid() && *AbilityToGrant;
}
