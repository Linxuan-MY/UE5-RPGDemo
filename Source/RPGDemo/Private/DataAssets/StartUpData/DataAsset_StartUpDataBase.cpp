// Fill out your copyright notice in the Description page of Project Settings.

#include "DataAssets/StartUpData/DataAsset_StartUpDataBase.h"
#include "AbilitySystem/Abilities/RPGDemoGameplayAbility.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"

void UDataAsset_StartUpDataBase::GiveToAbilitySystemComponent(URPGDemoAbilitySystemComponent* ASC, int32 ApplyLevel)
{
    check(ASC);
    if (!ASC->GetOwner() || !ASC->GetOwner()->HasAuthority()) return;
    GrantAbilities(ActivateOnGivenAbilities, ASC, ApplyLevel);
    GrantAbilities(ReactiveAbilities, ASC, ApplyLevel);
    for (const TSubclassOf<UGameplayEffect>& EffectClass : StartUpGameplayEffects)
    {
        if (EffectClass)
        {
            ASC->ApplyGameplayEffectToSelf(EffectClass->GetDefaultObject<UGameplayEffect>(), ApplyLevel, ASC->MakeEffectContext());
        }
    }
}

void UDataAsset_StartUpDataBase::GrantAbilities(
    const TArray<TSubclassOf<URPGDemoGameplayAbility>>& Abilities, URPGDemoAbilitySystemComponent* ASC, int32 ApplyLevel)
{
    for (const TSubclassOf<URPGDemoGameplayAbility>& Ability : Abilities)
    {
        if (!Ability || ASC->FindAbilitySpecFromClass(Ability)) continue;
        FGameplayAbilitySpec Spec(Ability);
        Spec.SourceObject = ASC->GetOwnerActor();
        Spec.Level = ApplyLevel;
        ASC->GiveAbility(Spec);
    }
}
