// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/EnemyCombatComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "RPGDemoFunctionLibrary.h"

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
