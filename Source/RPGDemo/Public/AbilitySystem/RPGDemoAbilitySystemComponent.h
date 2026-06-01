// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RPGDemoAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void OnAbilityInputPressed(const FGameplayTag InInputTag);
	void OnAbilityInputReleased(const FGameplayTag InInputTag);
	
};
