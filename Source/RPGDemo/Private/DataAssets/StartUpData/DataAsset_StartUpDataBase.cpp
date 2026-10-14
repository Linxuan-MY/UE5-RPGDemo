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
    const TArray<TSubclassOf<URPGDemoGameplayAbility>>& Abilities, URPGDemoAbilitySystemComponent* ASC, int32 ApplyLevel, UObject* SourceObject)
{
    for (const TSubclassOf<URPGDemoGameplayAbility>& Ability : Abilities)
    {
        if (!Ability) continue;
        bool bAlreadyGranted = false;
        for (const FGameplayAbilitySpec& Existing : ASC->GetActivatableAbilities())
            if (Existing.Ability && Existing.Ability->GetClass() == Ability &&
                Existing.SourceObject.Get() == (SourceObject ? SourceObject : ASC->GetOwnerActor()))
            { bAlreadyGranted = true; break; }
        if (bAlreadyGranted) continue;
        FGameplayAbilitySpec Spec(Ability);
        Spec.SourceObject = SourceObject ? SourceObject : ASC->GetOwnerActor();
        Spec.Level = ApplyLevel;
        ASC->GiveAbility(Spec);
    }
}

void UDataAsset_StartUpDataBase::GiveAvatarAbilitiesToAbilitySystemComponent(URPGDemoAbilitySystemComponent* ASC, int32 ApplyLevel)
{
    if (ASC && ASC->GetOwner() && ASC->GetOwner()->HasAuthority())
        GrantAbilities(AvatarStartUpAbilities, ASC, ApplyLevel, ASC->GetAvatarActor());
}
