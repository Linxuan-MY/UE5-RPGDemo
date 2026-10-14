// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.h"
#include "RPGDemoGameplayTags.h"
#include "Characters/RPGDemoBaseCharacter.h"

void URPGDemoAbilitySystemComponent::OnRep_ReplicatedAnimMontage()
{
    const ARPGDemoBaseCharacter* Character = Cast<ARPGDemoBaseCharacter>(GetAvatarActor());
    if (Character && Character->IsDeathPresentationActive()) return;
    Super::OnRep_ReplicatedAnimMontage();
}

void URPGDemoAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag InInputTag)
{
	if(!InInputTag.IsValid() || HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead)) return;

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InInputTag)) continue;

		if (InInputTag.MatchesTag(RPGDemoGameplayTags::InputTag_Toggleable) && AbilitySpec.IsActive())
		{
			CancelAbilityHandle(AbilitySpec.Handle);
		}
		else
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void URPGDemoAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag InInputTag)
{
	if (!InInputTag.IsValid() || !InInputTag.MatchesTag(RPGDemoGameplayTags::InputTag_MustBeHeld)) return;

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InInputTag) && Spec.IsActive())
		{
			CancelAbilityHandle(Spec.Handle);
		}
	}
}

void URPGDemoAbilitySystemComponent::CancelInputHeldAbilities()
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive() &&
			Spec.GetDynamicSpecSourceTags().HasTag(RPGDemoGameplayTags::InputTag_MustBeHeld))
		{
			CancelAbilityHandle(Spec.Handle);
		}
	}
}

void URPGDemoAbilitySystemComponent::GrantHeroWeaponAbilities(const TArray<FRPGDemoHeroAbilitySet>& InDefaultWeaponAbilities, const TArray<FRPGDemoHeroSpecialAbilitySet>& InSpecialWeaponAbilities, int32 ApplyLevel, TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilitySpecHandles)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetAvatarActor()) return;
    auto Grant = [this, ApplyLevel, &OutGrantedAbilitySpecHandles](const FRPGDemoHeroAbilitySet& AbilitySet)
    {
        if (!AbilitySet.IsValid()) return;
        for (const FGameplayAbilitySpec& Existing : GetActivatableAbilities())
            if (Existing.Ability && Existing.Ability->GetClass() == AbilitySet.AbilityToGrant &&
                Existing.SourceObject.Get() == GetAvatarActor() && Existing.GetDynamicSpecSourceTags().HasTagExact(AbilitySet.InputTag))
            { OutGrantedAbilitySpecHandles.AddUnique(Existing.Handle); return; }

        FGameplayAbilitySpec Spec(AbilitySet.AbilityToGrant, ApplyLevel);
        Spec.SourceObject = GetAvatarActor();
        Spec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);
        OutGrantedAbilitySpecHandles.AddUnique(GiveAbility(Spec));
    };

	for(const FRPGDemoHeroAbilitySet& AbilitySet : InDefaultWeaponAbilities)
	{
        Grant(AbilitySet);
	}

	for(const FRPGDemoHeroSpecialAbilitySet& AbilitySet : InSpecialWeaponAbilities)
	{
        Grant(AbilitySet);
	}
}

void URPGDemoAbilitySystemComponent::RemoveGrantedHeroWeaponAbilities(UPARAM(ref)TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || InSpecHandlesToRemove.IsEmpty()) return;

	for (const FGameplayAbilitySpecHandle& SpecHandle : InSpecHandlesToRemove)
	{
		if(SpecHandle.IsValid())
		{
			ClearAbility(SpecHandle);
		}
	}

	InSpecHandlesToRemove.Empty();
}

bool URPGDemoAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate)
{
	check(AbilityTagToActivate.IsValid());

	TArray<FGameplayAbilitySpec*> FoundAbilitiesSpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTagToActivate.GetSingleTagContainer(), FoundAbilitiesSpecs);

	if(!FoundAbilitiesSpecs.IsEmpty())
	{
		const int32 RandomAbilityIndex = FMath::RandRange(0, FoundAbilitiesSpecs.Num() - 1);
		FGameplayAbilitySpec* AbilitySpecToActivate = FoundAbilitiesSpecs[RandomAbilityIndex];

		check(AbilitySpecToActivate);
		if(!AbilitySpecToActivate->IsActive())
		{
			return TryActivateAbility(AbilitySpecToActivate->Handle);
		}
	}

	return false;
}
