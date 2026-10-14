// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/PickUps/RPGDemoStoneBase.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"

bool ARPGDemoStoneBase::Consume(URPGDemoAbilitySystemComponent* AbilitySystemComponent, int32 ApplyLevel)
{
	if (!HasAuthority() || bConsumed || bConsumptionInProgress || !AbilitySystemComponent || !StoneGameplayEffectClass ||
        AbilitySystemComponent->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead))
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
    OnRep_Consumed();
	ForceNetUpdate();
	MulticastOnStoneConsumed();
    SetLifeSpan(1.f);
	return true;
}

void ARPGDemoStoneBase::MulticastOnStoneConsumed_Implementation()
{
    bConsumed = true;
    OnRep_Consumed();
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

void ARPGDemoStoneBase::OnRep_Consumed()
{
    if (!bConsumed) return;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    TInlineComponentArray<UNiagaraComponent*> Effects(this);
    for (UNiagaraComponent* Effect : Effects) Effect->DeactivateImmediate();
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
    // Owning clients present the range prompt; F uses the validated server RPC.
}
