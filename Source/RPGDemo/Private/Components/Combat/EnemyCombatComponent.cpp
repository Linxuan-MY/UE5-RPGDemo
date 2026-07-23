// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/EnemyCombatComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "RPGDemoFunctionLibrary.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Components/BoxComponent.h"

#include "RPGDemoDebugHelper.h"

void UEnemyCombatComponent::OnHitTargetActor(AActor* HitActor)
{
	if (OverlappedActors.Contains(HitActor))
	{
		return;
	}

	OverlappedActors.AddUnique(HitActor);

	bool bIsValidBlock = false;

	const bool bIsPlayerBlocking = URPGDemoFunctionLibrary::NativeDoesActorHaveTag(HitActor, RPGDemoGameplayTags::Player_Status_Block);
	const bool bIsMyAttackUnblockable = URPGDemoFunctionLibrary::NativeDoesActorHaveTag(GetOwningPawn(), RPGDemoGameplayTags::Enemy_Status_Unblockable);

	if(bIsPlayerBlocking && !bIsMyAttackUnblockable)
	{
		bIsValidBlock = URPGDemoFunctionLibrary::IsValidBlock(GetOwningPawn(), HitActor);
	}

	FGameplayEventData EventData;
	EventData.Instigator = GetOwningPawn();
	EventData.Target = HitActor;

	if (bIsValidBlock)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			HitActor,
			RPGDemoGameplayTags::Player_Event_SuccessfulBlock,
			EventData
		);
	}
	else
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			GetOwningPawn(),
			RPGDemoGameplayTags::Shared_Event_MeleeHit,
			EventData
		);
	}
}

void UEnemyCombatComponent::ToggleBodyCollision(bool bShouldEnable, EToggleDamageType ToggleDamageType)
{
	ARPGDemoEnemyCharacter* OwningEnemyCharacter = GetOwningPawn<ARPGDemoEnemyCharacter>();

	check(OwningEnemyCharacter);

	UBoxComponent* LeftHandCollisionBox = OwningEnemyCharacter->GetLeftHandCollisionBox();
	UBoxComponent* RightHandCollisionBox = OwningEnemyCharacter->GetRightHandCollisionBox();

	check(LeftHandCollisionBox && RightHandCollisionBox);

	switch (ToggleDamageType)
	{
		case EToggleDamageType::LeftHand:
			LeftHandCollisionBox->SetCollisionEnabled(bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			break;
		case EToggleDamageType::RightHand:
			RightHandCollisionBox->SetCollisionEnabled(bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			break;
	}

	if (!bShouldEnable)
	{
		OverlappedActors.Empty();
	}
}
