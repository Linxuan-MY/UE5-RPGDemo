// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Controllers/RPGDemoHeroController.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "GameplayEffect.h"
#include "AnimInstances/Hero/RPGDemoHeroAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	TSubclassOf<UGameplayEffect> GetRageCostGameplayEffectClass()
	{
		static TSubclassOf<UGameplayEffect> RageCostClass = []()
		{
			return LoadClass<UGameplayEffect>(nullptr,
				TEXT("/Game/PlayerCharacter/GameplayEffect/GE_Hero_Cost_Rage.GE_Hero_Cost_Rage_C"));
		}();
		return RageCostClass;
	}
}

void URPGDemoHeroGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	if (!GetAssetTags().HasTagExact(RPGDemoGameplayTags::Player_Ability_Rage) || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> RageCostClass = GetRageCostGameplayEffectClass();
	if (!RageCostClass)
	{
		return;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = RageCostClass;
	if (ActorInfo->AbilitySystemComponent->GetActiveEffects(Query).IsEmpty())
	{
		FGameplayEffectContextHandle Context = ActorInfo->AbilitySystemComponent->MakeEffectContext();
		Context.SetAbility(this);
		const FGameplayEffectSpecHandle Spec = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(RageCostClass, GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
}

ARPGDemoHeroCharacter* URPGDemoHeroGameplayAbility::GetHeroCharacterFromActorInfo()
{
    if (!CurrentActorInfo) return nullptr;
	if(!CachedRPGDemoHeroCharacter.IsValid() || CachedRPGDemoHeroCharacter.Get() != CurrentActorInfo->AvatarActor.Get())
	{
		CachedRPGDemoHeroCharacter = Cast<ARPGDemoHeroCharacter>(CurrentActorInfo->AvatarActor);
	}

	return CachedRPGDemoHeroCharacter.IsValid() ? CachedRPGDemoHeroCharacter.Get() : nullptr;
}

void URPGDemoHeroGameplayAbility::StartLocalHitPause(float Duration)
{
    ARPGDemoHeroCharacter* Hero = GetHeroCharacterFromActorInfo();
    if (!Hero || !Hero->IsLocallyControlled()) return;
    if (URPGDemoHeroAnimInstance* Anim = Cast<URPGDemoHeroAnimInstance>(Hero->GetMesh()->GetAnimInstance()))
        Anim->BeginLocalHitStop(Duration);
}

ARPGDemoHeroController* URPGDemoHeroGameplayAbility::GetHeroControllerFromActorInfo()
{
    if (!CurrentActorInfo) return nullptr;
	if(!CachedRPGDemoHeroController.IsValid() || CachedRPGDemoHeroController.Get() != CurrentActorInfo->PlayerController.Get())
	{
		CachedRPGDemoHeroController = Cast<ARPGDemoHeroController>(CurrentActorInfo->PlayerController);
	}
	return CachedRPGDemoHeroController.IsValid() ? CachedRPGDemoHeroController.Get() : nullptr;
}

UHeroCombatComponent* URPGDemoHeroGameplayAbility::GetHeroCombatComponentFromActorInfo()
{
    ARPGDemoHeroCharacter* Hero = GetHeroCharacterFromActorInfo();
	return Hero ? Hero->GetHeroCombatComponent() : nullptr;
}

UHeroUIComponent* URPGDemoHeroGameplayAbility::GetHeroUIComponentFromActorInfo()
{
    ARPGDemoHeroCharacter* Hero = GetHeroCharacterFromActorInfo();
	return Hero ? Hero->GetHeroUIComponent() : nullptr;
}

FGameplayEffectSpecHandle URPGDemoHeroGameplayAbility::MakeHeroDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass, float InWeaponBaseDamage, FGameplayTag InCurrentAttackTypeTag, int32 InUsedComboCount)
{
	check(EffectClass);

	FGameplayEffectContextHandle ContextHandle = GetRPGDemoAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	ContextHandle.SetAbility(this);
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());
	ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle EffectSpecHandle = GetRPGDemoAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
		EffectClass,
		GetAbilityLevel(),
		ContextHandle
	);

	EffectSpecHandle.Data->SetSetByCallerMagnitude(
		RPGDemoGameplayTags::Shared_SetByCaller_BaseDamage,
		InWeaponBaseDamage
	);

	if(InCurrentAttackTypeTag.IsValid())
	{
		EffectSpecHandle.Data->SetSetByCallerMagnitude(
			InCurrentAttackTypeTag,
			InUsedComboCount
		);
	}

	return EffectSpecHandle;
}

bool URPGDemoHeroGameplayAbility::GetAbilityRemainingCooldownByTag(FGameplayTag InCooldownTag, float& TotalCooldownTime,
	float& RemainingCooldownTime)
{
	check(InCooldownTag.IsValid());

	FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(InCooldownTag.GetSingleTagContainer());

	TArray < TPair < float, float > > TimeRemainingAndDuration = GetAbilitySystemComponentFromActorInfo()->GetActiveEffectsTimeRemainingAndDuration(CooldownQuery);

	if (!TimeRemainingAndDuration.IsEmpty())
	{
		RemainingCooldownTime = TimeRemainingAndDuration[0].Key;
		TotalCooldownTime = TimeRemainingAndDuration[0].Value;
	}
	else
	{
		RemainingCooldownTime = 0;
		TotalCooldownTime = 0;
	}

	return RemainingCooldownTime > 0.f;
}
