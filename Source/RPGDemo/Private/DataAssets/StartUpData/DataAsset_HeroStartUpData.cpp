// Fill out your copyright notice in the Description page of Project Settings.

#include "DataAssets/StartUpData/DataAsset_HeroStartUpData.h"
#include "AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"

void UDataAsset_HeroStartUpData::GiveToAbilitySystemComponent(URPGDemoAbilitySystemComponent* ASC, int32 ApplyLevel)
{
    if (!ASC || !ASC->GetOwner() || !ASC->GetOwner()->HasAuthority()) return;
    Super::GiveToAbilitySystemComponent(ASC, ApplyLevel);
    for (const FRPGDemoHeroAbilitySet& AbilitySet : HeroStartUpAbilitySets)
    {
        if (!AbilitySet.IsValid() || ASC->FindAbilitySpecFromClass(AbilitySet.AbilityToGrant)) continue;
        FGameplayAbilitySpec Spec(AbilitySet.AbilityToGrant);
        Spec.SourceObject = ASC->GetOwnerActor();
        Spec.Level = ApplyLevel;
        Spec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);
        ASC->GiveAbility(Spec);
    }
}
