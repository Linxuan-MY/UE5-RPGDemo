// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/PawnCombatComponent.h"
#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "Components/BoxComponent.h"

#include "RPGDemoDebugHelper.h"

void UPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ARPGDemoWeaponBase* InWeaponToRegister, bool bReigsterAsEquippedWeapon)
{
	check(InWeaponToRegister);

	if (ARPGDemoWeaponBase* ExistingWeapon = GetCharacterCarriedWeaponByTag(InWeaponTagToRegister))
	{
		if (IsValid(ExistingWeapon))
		{
			UE_LOG(LogTemp, Verbose, TEXT("Ignoring duplicate weapon registration for tag %s on %s."),
				*InWeaponTagToRegister.ToString(), *GetNameSafe(GetOwningPawn()));
			return;
		}

		CharacterCarriedWeaponMap.Remove(InWeaponTagToRegister);
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);

	if (GetOwningPawn()->HasAuthority())
	{
		InWeaponToRegister->SetOwner(GetOwningPawn());
		InWeaponToRegister->SetInstigator(GetOwningPawn());
		InWeaponToRegister->SetWeaponRegistrationData(InWeaponTagToRegister, bReigsterAsEquippedWeapon);
	}
	InWeaponToRegister->SetActorHiddenInGame(false);
	InWeaponToRegister->SetActorEnableCollision(true);

	InWeaponToRegister->OnWeaponHitTarget.BindUObject(this, &ThisClass::OnHitTargetActor);
	InWeaponToRegister->OnWeaponPulledFromTarget.BindUObject(this, &ThisClass::OnWeaponPulledFromTargetActor);

	if (bReigsterAsEquippedWeapon)
	{
		CurrentlyEquippedWeaponTag = InWeaponTagToRegister;
	}
}

ARPGDemoWeaponBase* UPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const
{
	if(CharacterCarriedWeaponMap.Contains(InWeaponTagToGet))
	{
		ARPGDemoWeaponBase* const* FoundWeapon = CharacterCarriedWeaponMap.Find(InWeaponTagToGet);

		if (FoundWeapon)
		{
			return *FoundWeapon;
		}
	}

	return nullptr;
}

ARPGDemoWeaponBase* UPawnCombatComponent::GetCharacterCurrentlyEquippedWeapon() const
{
	if(!CurrentlyEquippedWeaponTag.IsValid())
	{
		return nullptr;
	}

	return GetCharacterCarriedWeaponByTag(CurrentlyEquippedWeaponTag);
}

void UPawnCombatComponent::ToggleWeaponCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType)
{
	// Anim notifies run on every network copy of a character. Only the authority may
	// enable hit collision; client-side weapon maps are presentation state and may
	// not have been rebuilt yet when a replicated attack montage starts.
	const APawn* OwningPawn = GetOwningPawn();
	if (!OwningPawn || !OwningPawn->HasAuthority())
	{
		return;
	}

	if (ToggleDamageType == EToggleDamageType::CurrentEquippedWeapon)
	{
		ToggleCurrentEquippedWeaponCollision(bShouldEnable);
	}
	else
	{
		ToggleBodyCollision(bShouldEnable, ToggleDamageType);
	}
}

void UPawnCombatComponent::OnHitTargetActor(AActor* HitActor)
{
}

void UPawnCombatComponent::OnWeaponPulledFromTargetActor(AActor* InteractedActor)
{
}

void UPawnCombatComponent::ToggleCurrentEquippedWeaponCollision(bool bShouldEnable)
{
	ARPGDemoWeaponBase* WeaponToToggle = GetCharacterCurrentlyEquippedWeapon();
	if (!IsValid(WeaponToToggle) || !IsValid(WeaponToToggle->GetWeaponCollisionBox()))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Cannot %s weapon collision for %s: no valid weapon is registered for equipped tag '%s'."),
			bShouldEnable ? TEXT("enable") : TEXT("disable"),
			*GetNameSafe(GetOwningPawn()),
			*CurrentlyEquippedWeaponTag.ToString());
		return;
	}

	if (bShouldEnable)
	{
		WeaponToToggle->GetWeaponCollisionBox()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		WeaponToToggle->GetWeaponCollisionBox()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OverlappedActors.Empty();
	}
}

void UPawnCombatComponent::ToggleBodyCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType)
{
}
