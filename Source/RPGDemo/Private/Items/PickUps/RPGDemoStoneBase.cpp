// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/PickUps/RPGDemoStoneBase.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"

void ARPGDemoStoneBase::Consume(URPGDemoAbilitySystemComponent* AbilitySystemComponent, int32 ApplyLevel)
{
	check(StoneGameplayEffectClass);

	UGameplayEffect* EffectCDO = StoneGameplayEffectClass->GetDefaultObject<UGameplayEffect>();

	AbilitySystemComponent->ApplyGameplayEffectToSelf(
		EffectCDO,
		ApplyLevel,
		AbilitySystemComponent->MakeEffectContext()
	);

	BP_OnStoneConsumed();
}

void ARPGDemoStoneBase::OnPickUpCollisionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                            AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                                            const FHitResult& SweepResult)
{
	if (ARPGDemoHeroCharacter* OverlappedHeroCharacter = Cast<ARPGDemoHeroCharacter>(OtherActor))
	{
		OverlappedHeroCharacter->GetRPGDemoAbilitySystemComponent()->TryActivateAbilityByTag(RPGDemoGameplayTags::Player_Ability_PickUp_Stones);
	}
}
