// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/PickUps/RPGDemoStoneBase.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

bool ARPGDemoStoneBase::Consume(URPGDemoAbilitySystemComponent* AbilitySystemComponent, int32 ApplyLevel)
{
	if (!HasAuthority() || bConsumed || bConsumptionInProgress || !AbilitySystemComponent || !StoneGameplayEffectClass)
	{
		return false;
	}

	UGameplayEffect* EffectCDO = StoneGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	if (!EffectCDO)
	{
		return false;
	}

	// Applying an effect may synchronously invoke gameplay callbacks that try to consume again.
	TGuardValue<bool> ConsumptionGuard(bConsumptionInProgress, true);
	const FActiveGameplayEffectHandle AppliedHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
		EffectCDO,
		ApplyLevel,
		AbilitySystemComponent->MakeEffectContext()
	);
	if (!AppliedHandle.WasSuccessfullyApplied())
	{
		return false;
	}

	bConsumed = true;
	PickUpCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForceNetUpdate();
	MulticastOnStoneConsumed();
	return true;
}

void ARPGDemoStoneBase::MulticastOnStoneConsumed_Implementation()
{
	if (bLocalConsumptionPresented)
	{
		return;
	}
	bLocalConsumptionPresented = true;
	if (GetNetMode() != NM_DedicatedServer)
	{
		BP_OnStoneConsumed();
	}
}

void ARPGDemoStoneBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoStoneBase, bConsumed);
}

void ARPGDemoStoneBase::OnPickUpCollisionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                            AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                                            const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ARPGDemoHeroCharacter* OverlappedHeroCharacter = Cast<ARPGDemoHeroCharacter>(OtherActor))
	{
		OverlappedHeroCharacter->GetRPGDemoAbilitySystemComponent()->TryActivateAbilityByTag(RPGDemoGameplayTags::Player_Ability_PickUp_Stones);
	}
}
