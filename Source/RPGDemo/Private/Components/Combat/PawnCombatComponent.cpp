// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/PawnCombatComponent.h"
#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "Components/BoxComponent.h"

#include "RPGDemoDebugHelper.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Combat/RPGDemoCombatLog.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UPawnCombatComponent::UPawnCombatComponent() { SetIsReplicatedByDefault(true); }
void UPawnCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UPawnCombatComponent, CurrentlyEquippedWeaponTag);
}
void UPawnCombatComponent::OnRep_EquippedWeapon() { RefreshEquippedWeaponPresentation(); }
void UPawnCombatComponent::SetEquippedWeaponTag(FGameplayTag WeaponTag)
{
    CurrentlyEquippedWeaponTag = WeaponTag;
    RefreshEquippedWeaponPresentation();
    if (GetOwner() && GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
}

void UPawnCombatComponent::RegisterSpawnedWeapon(FGameplayTag InWeaponTagToRegister, ARPGDemoWeaponBase* InWeaponToRegister, bool bReigsterAsEquippedWeapon)
{
    if (!IsValid(InWeaponToRegister) || !InWeaponTagToRegister.IsValid()) return;

	if (ARPGDemoWeaponBase* ExistingWeapon = GetCharacterCarriedWeaponByTag(InWeaponTagToRegister))
	{
		if (IsValid(ExistingWeapon))
		{
            if (ExistingWeapon == InWeaponToRegister)
            {
                if (bReigsterAsEquippedWeapon) CurrentlyEquippedWeaponTag = InWeaponTagToRegister;
                RefreshEquippedWeaponPresentation();
                return;
            }
            // Repeated OnRep calls are harmless; a second spawned actor is not.
            if (GetOwningPawn()->HasAuthority())
            {
                UE_LOG(LogRPGDemoCombat, Warning, TEXT("Destroying duplicate carried weapon %s on %s"),
                    *InWeaponToRegister->GetName(), *GetNameSafe(GetOwningPawn()));
                InWeaponToRegister->OnWeaponHitTarget.Unbind();
                InWeaponToRegister->OnWeaponPulledFromTarget.Unbind();
                InWeaponToRegister->Destroy();
            }
			return;
		}

		CharacterCarriedWeaponMap.Remove(InWeaponTagToRegister);
	}

	CharacterCarriedWeaponMap.Emplace(InWeaponTagToRegister, InWeaponToRegister);
    if (RPGDemoCombat::IsDebugEnabled())
        UE_LOG(LogRPGDemoCombat, Log, TEXT("WeaponRegistered pawn=%s weapon=%s equipped=%d netMode=%d"),
            *GetNameSafe(GetOwningPawn()), *InWeaponToRegister->GetName(), bReigsterAsEquippedWeapon, GetWorld()->GetNetMode());

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
    RefreshEquippedWeaponPresentation();
}

ARPGDemoWeaponBase* UPawnCombatComponent::GetCharacterCarriedWeaponByTag(FGameplayTag InWeaponTagToGet) const
{
	if(CharacterCarriedWeaponMap.Contains(InWeaponTagToGet))
	{
		const TWeakObjectPtr<ARPGDemoWeaponBase>* FoundWeapon = CharacterCarriedWeaponMap.Find(InWeaponTagToGet);

		if (FoundWeapon)
		{
			return FoundWeapon->Get();
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
    const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwningPawn());
    if (bShouldEnable && ASC && ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead)) return;

    if (RPGDemoCombat::IsDebugEnabled())
    {
        UE_LOG(LogRPGDemoCombat, Log, TEXT("CollisionWindow pawn=%s enabled=%d type=%d"),
            *OwningPawn->GetName(), bShouldEnable, static_cast<int32>(ToggleDamageType));
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

void UPawnCombatComponent::ResetDamageCollision()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    for (const auto& Pair : CharacterCarriedWeaponMap)
    {
        if (ARPGDemoWeaponBase* Weapon = Pair.Value.Get())
            Weapon->GetWeaponCollisionBox()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (ARPGDemoEnemyCharacter* Enemy = Cast<ARPGDemoEnemyCharacter>(GetOwner()))
    {
        Enemy->GetLeftHandCollisionBox()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Enemy->GetRightHandCollisionBox()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    OverlappedActors.Empty();
}

void UPawnCombatComponent::UnregisterWeapon(ARPGDemoWeaponBase* Weapon)
{
    for (auto It = CharacterCarriedWeaponMap.CreateIterator(); It; ++It)
    {
        if (!It.Value().IsValid() || It.Value().Get() == Weapon)
        {
            if (CurrentlyEquippedWeaponTag == It.Key()) CurrentlyEquippedWeaponTag = FGameplayTag();
            It.RemoveCurrent();
        }
    }
    RefreshEquippedWeaponPresentation();
}

void UPawnCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Empty first: weapon EndPlay can unregister itself while we destroy it.
    const auto Weapons = MoveTemp(CharacterCarriedWeaponMap);
    CurrentlyEquippedWeaponTag = FGameplayTag();
    RefreshEquippedWeaponPresentation();
    OverlappedActors.Empty();
    for (const auto& Pair : Weapons)
    {
        if (ARPGDemoWeaponBase* Weapon = Pair.Value.Get())
        {
            Weapon->OnWeaponHitTarget.Unbind();
            Weapon->OnWeaponPulledFromTarget.Unbind();
            if (GetOwner() && GetOwner()->HasAuthority())
            {
                Weapon->GetWeaponCollisionBox()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                if (RPGDemoCombat::IsDebugEnabled())
                    UE_LOG(LogRPGDemoCombat, Log, TEXT("DestroyCarriedWeapon pawn=%s weapon=%s"), *GetOwner()->GetName(), *Weapon->GetName());
                Weapon->Destroy();
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}
