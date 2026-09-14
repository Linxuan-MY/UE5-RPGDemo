// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/RPGDemoGameplayAbility.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"

void URPGDemoGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (AbilityActivationPolicy == ERPGDemoAbilityActivationPolicy::OnGiven)
	{
		if (!ActorInfo || Spec.IsActive())
		{
			return;
		}

		const bool bAuthorityPolicy =
			NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly ||
			NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;
		const bool bShouldActivate = bAuthorityPolicy
			? ActorInfo->IsNetAuthority()
			: ActorInfo->IsLocallyControlled();

		if (!bShouldActivate)
		{
			return;
		}

		// OnGiveAbility can run while a replicated spec is still being inserted on the
		// owning client. Activating on the next tick avoids an invalid SpecHandle and
		// also keeps local-only UI abilities off remote server controller copies.
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ActorInfo->AbilitySystemComponent.Get();
		const FGameplayAbilitySpecHandle SpecHandle = Spec.Handle;
		UWorld* World = ActorInfo->AvatarActor.IsValid()
			? ActorInfo->AvatarActor->GetWorld()
			: nullptr;
		if (World)
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
				[WeakASC, SpecHandle]()
				{
					if (UAbilitySystemComponent* ASC = WeakASC.Get())
					{
						if (FGameplayAbilitySpec* CurrentSpec = ASC->FindAbilitySpecFromHandle(SpecHandle);
							CurrentSpec && !CurrentSpec->IsActive())
						{
							ASC->TryActivateAbility(SpecHandle);
						}
					}
				}));
		}
	}
}

void URPGDemoGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (AbilityActivationPolicy == ERPGDemoAbilityActivationPolicy::OnGiven)
	{
		if (ActorInfo && ActorInfo->IsNetAuthority())
		{
			ActorInfo->AbilitySystemComponent->ClearAbility(Handle);
		}
	}
}

UPawnCombatComponent* URPGDemoGameplayAbility::GetPawnCombatComponentFromActorInfo() const
{
	return GetAvatarActorFromActorInfo()->FindComponentByClass<UPawnCombatComponent>();
}

URPGDemoAbilitySystemComponent* URPGDemoGameplayAbility::GetRPGDemoAbilitySystemComponentFromActorInfo() const
{
	return Cast<URPGDemoAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
}

FActiveGameplayEffectHandle URPGDemoGameplayAbility::NativeApplyEffectSpecToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

	check(TargetASC && InSpecHandle.IsValid());

	return GetRPGDemoAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
		*InSpecHandle.Data,
		TargetASC
	);

}

FActiveGameplayEffectHandle URPGDemoGameplayAbility::BP_ApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle, ERPGDemoSuccessType& OutSuccessType)
{
	FActiveGameplayEffectHandle ActiveGameplayEffectHandle = NativeApplyEffectSpecToTarget(TargetActor, InSpecHandle);

	OutSuccessType = ActiveGameplayEffectHandle.WasSuccessfullyApplied() ? ERPGDemoSuccessType::Successful : ERPGDemoSuccessType::Failed;

	return ActiveGameplayEffectHandle;

}

void URPGDemoGameplayAbility::ApplyGameplayEffectSpecHandleToHitResaults(const FGameplayEffectSpecHandle& InSpecHandle,
	const TArray<FHitResult>& InHitResults)
{
	if (InHitResults.IsEmpty())
	{
		return;
	}

	APawn* OwningPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	for (const FHitResult& HitResult : InHitResults)
	{
		if (APawn* HitPawn = Cast<APawn>(HitResult.GetActor()))
		{
			if (URPGDemoFunctionLibrary::TargetPawnHostile(OwningPawn, HitPawn))
			{
				FActiveGameplayEffectHandle ActiveGameplayEffectHandle = NativeApplyEffectSpecToTarget(HitPawn, InSpecHandle);

				if (ActiveGameplayEffectHandle.WasSuccessfullyApplied())
				{
					FGameplayEventData Data;
					Data.Instigator = OwningPawn;
					Data.Target = HitPawn;

					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
						HitPawn,
						RPGDemoGameplayTags::Shared_Event_HitReact,
						Data
					);
				}
			}
		}
	}
}
