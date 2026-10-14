// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/HeroCombatComponent.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Components/UI/HeroUIComponent.h"
#include "AnimInstances/Hero/RPGDemoHeroLinkedAnimLayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

#include "RPGDemoDebugHelper.h"

ARPGDemoHeroWeapon* UHeroCombatComponent::GetHeroCarriedWeaponByTag(FGameplayTag InWeaponTag) const
{
	return Cast<ARPGDemoHeroWeapon>(GetCharacterCarriedWeaponByTag(InWeaponTag));
}

ARPGDemoHeroWeapon* UHeroCombatComponent::GetHeroCurrentEquippedWeapon() const
{
	return Cast<ARPGDemoHeroWeapon>(GetCharacterCurrentlyEquippedWeapon());
}

float UHeroCombatComponent::GetHeroCurrentEquippedWeaponDamageAtLevel(float InLevel) const
{
    const ARPGDemoHeroWeapon* Weapon = GetHeroCurrentEquippedWeapon();
	return Weapon ? Weapon->HeroWeaponData.WeaponBaseDamage.GetValueAtLevel(InLevel) : 0.f;
}

void UHeroCombatComponent::EquipHeroWeapon(ARPGDemoHeroWeapon* Weapon, int32 AbilityLevel)
{
    ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(GetOwner());
    if (!Hero || !IsValid(Weapon) || Weapon->GetOwner() != Hero) return;
    // Registration can arrive after the equipped tag on a client. Both paths refresh.
    if (Hero->HasAuthority())
    {
        if (URPGDemoAbilitySystemComponent* ASC = Hero->GetRPGDemoAbilitySystemComponent())
        {
            TArray<FGameplayAbilitySpecHandle> Handles;
            ASC->GrantHeroWeaponAbilities(Weapon->HeroWeaponData.DefaultWeaponAbilities,
                Weapon->HeroWeaponData.SpecialWeaponAbilities, AbilityLevel, Handles);
            Weapon->AssignGrantedAbilitySpecHandles(Handles);
        }
    }
    // The equipped tag is set by the equip animation event; refresh after that event too.
    RefreshEquippedWeaponPresentation();
}

void UHeroCombatComponent::UnequipHeroWeapon()
{
    ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(GetOwner());
    if (Hero && Hero->HasAuthority())
        if (ARPGDemoHeroWeapon* Weapon = GetHeroCurrentEquippedWeapon() ? GetHeroCurrentEquippedWeapon() : PresentedWeapon.Get())
            if (URPGDemoAbilitySystemComponent* ASC = Hero->GetRPGDemoAbilitySystemComponent())
            {
                TArray<FGameplayAbilitySpecHandle> Handles = Weapon->GetGrantedAbilitySpecHandles();
                ASC->RemoveGrantedHeroWeaponAbilities(Handles);
                Weapon->AssignGrantedAbilitySpecHandles(Handles);
            }
    CurrentlyEquippedWeaponTag = FGameplayTag();
    RefreshEquippedWeaponPresentation();
}

void UHeroCombatComponent::RefreshEquippedWeaponPresentation()
{
    ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(GetOwner());
    if (!Hero) return;
    ARPGDemoHeroWeapon* Weapon = GetHeroCurrentEquippedWeapon();
    const bool bLocalMappingReady = !Hero->IsLocallyControlled() ||
        AppliedWeaponMapping == (Weapon ? Weapon->HeroWeaponData.WeaponInputMappingContext : nullptr);
    if (PresentedWeapon.Get() == Weapon && (!Weapon || AppliedWeaponAnimLayer) && bLocalMappingReady) return;
    if (UAnimInstance* Anim = Hero->GetMesh()->GetAnimInstance())
    {
        if (AppliedWeaponAnimLayer) Anim->UnlinkAnimClassLayers(AppliedWeaponAnimLayer);
        AppliedWeaponAnimLayer = Weapon ? Weapon->HeroWeaponData.WeaponAnimLayerToLink : nullptr;
        if (AppliedWeaponAnimLayer) Anim->LinkAnimClassLayers(AppliedWeaponAnimLayer);
    }
    if (Hero->IsLocallyControlled())
    {
        APlayerController* PC = Cast<APlayerController>(Hero->GetController());
        UEnhancedInputLocalPlayerSubsystem* Input = PC && PC->GetLocalPlayer()
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
        if (Input)
        {
            if (AppliedWeaponMapping) Input->RemoveMappingContext(AppliedWeaponMapping);
            AppliedWeaponMapping = Weapon ? Weapon->HeroWeaponData.WeaponInputMappingContext : nullptr;
            if (AppliedWeaponMapping) Input->AddMappingContext(AppliedWeaponMapping, 1);
        }
        if (ARPGDemoHeroWeapon* Previous = PresentedWeapon.Get())
            for (const auto& Ability : Previous->HeroWeaponData.SpecialWeaponAbilities)
                Hero->GetHeroUIComponent()->OnAbilityIconSlotUpdated.Broadcast(Ability.InputTag, nullptr);
        Hero->GetHeroUIComponent()->RefreshPresentation();
    }
    PresentedWeapon = Weapon;
}

void UHeroCombatComponent::OnHitTargetActor(AActor* HitActor)
{
	if (OverlappedActors.Contains(HitActor))
	{
		return;
	}

	OverlappedActors.AddUnique(HitActor);

	FGameplayEventData Data;
	Data.Instigator = GetOwningPawn();
	Data.Target = HitActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwningPawn(),
		RPGDemoGameplayTags::Shared_Event_MeleeHit,
		Data
	);

}

void UHeroCombatComponent::OnWeaponPulledFromTargetActor(AActor* InteractedActor)
{
    // Feedback is sent once after authoritative damage, not when leaving a box.
}
