// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/RPGDemoGameplayAbility.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Combat/RPGDemoCombatLog.h"
#include "Characters/RPGDemoEnemyCharacter.h"

void URPGDemoGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // Damage, an owned death tag and direct scripted activation share the same
    // presentation. Queue resolution before Blueprint chooses its montage.
    if (ActorInfo && ActorInfo->IsNetAuthority() && GetAssetTags().HasTagExact(RPGDemoGameplayTags::Shared_Ability_Death))
    {
        if (auto* Enemy = Cast<ARPGDemoEnemyCharacter>(ActorInfo->AvatarActor.Get())) Enemy->BeginReplicatedDeathPresentation();
        else if (auto* Character = Cast<ARPGDemoBaseCharacter>(ActorInfo->AvatarActor.Get())) Character->BeginCharacterDeath();
    }
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

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
                            AActor* SourceActor = Cast<AActor>(CurrentSpec->SourceObject.Get());
                            if (SourceActor && SourceActor->IsA<APawn>() && SourceActor != ASC->GetAvatarActor()) return;
							ASC->TryActivateAbility(SpecHandle);
						}
					}
				}));
		}
	}
}

void URPGDemoGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{

    SetHitReactFlashEnabled(false);
    const FGameplayTagContainer& Tags = GetAssetTags();
    if (ActorInfo && ActorInfo->IsNetAuthority() &&
        (Tags.HasTag(RPGDemoGameplayTags::Enemy_Ability_Melee) ||
         Tags.HasTag(RPGDemoGameplayTags::Player_Ability_LightAttack_Axe) ||
         Tags.HasTag(RPGDemoGameplayTags::Player_Ability_HeavyAttack_Axe)))
    {
        if (UPawnCombatComponent* Combat = GetPawnCombatComponentFromActorInfo()) Combat->ResetDamageCollision();
    }
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (AbilityActivationPolicy == ERPGDemoAbilityActivationPolicy::OnGiven)
	{
		if (ActorInfo && ActorInfo->IsNetAuthority())
		{
			ActorInfo->AbilitySystemComponent->ClearAbility(Handle);
		}
	}
}

void URPGDemoGameplayAbility::SetHitReactFlashEnabled(bool bEnabled)
{
    if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !CurrentActorInfo->AbilitySystemComponent.IsValid()) return;
    UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
    if (bEnabled && !bHitReactFlashActive)
    {
        bHitReactFlashActive = true;
        FGameplayCueParameters Parameters;
        Parameters.EffectContext = ASC->MakeEffectContext();
        ASC->AddGameplayCue(RPGDemoGameplayTags::GameplayCue_FX_HitFlash, Parameters);
    }
    else if (!bEnabled && bHitReactFlashActive)
    {
        bHitReactFlashActive = false;
        ASC->RemoveGameplayCue(RPGDemoGameplayTags::GameplayCue_FX_HitFlash);
    }
}

UPawnCombatComponent* URPGDemoGameplayAbility::GetPawnCombatComponentFromActorInfo() const
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    return Avatar ? Avatar->FindComponentByClass<UPawnCombatComponent>() : nullptr;
}

URPGDemoAbilitySystemComponent* URPGDemoGameplayAbility::GetRPGDemoAbilitySystemComponentFromActorInfo() const
{
	return Cast<URPGDemoAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
}

FActiveGameplayEffectHandle URPGDemoGameplayAbility::NativeApplyEffectSpecToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

    if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !IsValid(TargetASC) || !InSpecHandle.IsValid())
        return FActiveGameplayEffectHandle();

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
