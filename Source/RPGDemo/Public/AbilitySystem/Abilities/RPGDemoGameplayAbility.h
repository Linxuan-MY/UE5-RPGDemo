// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RPGDemoGameplayAbility.generated.h"

class UPawnCombatComponent;
class URPGDemoAbilitySystemComponent;

UENUM(BlueprintType)
enum class ERPGDemoAbilityActivationPolicy : uint8
{
	OnTriggered,
	OnGiven
};

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	//~Begin UGameplayAbility interface
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility interface

	UPROPERTY(EditDefaultsOnly, Category = "RPGDemoAbility")
	ERPGDemoAbilityActivationPolicy AbilityActivationPolicy = ERPGDemoAbilityActivationPolicy::OnTriggered;

	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	UPawnCombatComponent* GetPawnCombatComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "RPGDemo|Ability")
	URPGDemoAbilitySystemComponent* GetRPGDemoAbilitySystemComponentFromActorInfo() const;

	
};
