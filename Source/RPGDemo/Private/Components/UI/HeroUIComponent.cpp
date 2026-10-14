// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UI/HeroUIComponent.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"

void UHeroUIComponent::RefreshPresentation()
{
    ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(GetOwner());
    if (!Hero || !Hero->IsLocallyControlled()) return;
    URPGDemoAbilitySystemComponent* ASC = Hero->GetRPGDemoAbilitySystemComponent();
    const URPGDemoAttributeSet* Attributes = Hero->GetRPGDemoAttributeSet();
    if (Attributes)
    {
        OnCurrentHealthChanged.Broadcast(Attributes->GetMaxHealth() > 0.f ? Attributes->GetCurrentHealth() / Attributes->GetMaxHealth() : 0.f);
        OnCurrentRageChanged.Broadcast(Attributes->GetMaxRage() > 0.f ? Attributes->GetCurrentRage() / Attributes->GetMaxRage() : 0.f);
    }
    const ARPGDemoHeroWeapon* Weapon = Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon();
    OnEquippedWeaponChanged.Broadcast(Weapon ? Weapon->HeroWeaponData.SoftWeaponIconTexture : TSoftObjectPtr<UTexture2D>());
    if (!Weapon || !ASC) return;
    for (const FRPGDemoHeroSpecialAbilitySet& Ability : Weapon->HeroWeaponData.SpecialWeaponAbilities)
    {
        OnAbilityIconSlotUpdated.Broadcast(Ability.InputTag, Ability.SoftAbilityIconMaterial);
        if (!Ability.AbilityCooldownTag.IsValid()) continue;
        const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Ability.AbilityCooldownTag.GetSingleTagContainer());
        const auto Cooldowns = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
        float Remaining = 0.f, Duration = 0.f;
        for (const auto& Cooldown : Cooldowns)
            if (Cooldown.Key > Remaining) { Remaining = Cooldown.Key; Duration = Cooldown.Value; }
        if (Remaining > 0.f) OnAbilityCooldownBegin.Broadcast(Ability.InputTag, Duration, Remaining);
    }
}

