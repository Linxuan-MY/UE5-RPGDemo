// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataAsset_StartUpDataBase.generated.h"

class URPGDemoGameplayAbility;
class URPGDemoAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class RPGDEMO_API UDataAsset_StartUpDataBase : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual void GiveToAbilitySystemComponent(URPGDemoAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "StartUpData")
	TArray < TSubclassOf < URPGDemoGameplayAbility > > ActivateOnGivenAbilities;
	
	UPROPERTY(EditDefaultsOnly, Category = "StartUpData")
	TArray < TSubclassOf < URPGDemoGameplayAbility > > ReactiveAbilities;

	void GrantAbilities(const TArray<TSubclassOf<URPGDemoGameplayAbility>>& InAbilitiesToGive, URPGDemoAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1);
};
