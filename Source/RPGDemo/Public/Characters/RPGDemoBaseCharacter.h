// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "RPGDemoBaseCharacter.generated.h"

class URPGDemoAbilitySystemComponent;
class URPGDemoAttributeSet;
class UDataAsset_StartUpDataBase;

UCLASS()
class RPGDEMO_API ARPGDemoBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ARPGDemoBaseCharacter();

	//~ Begin IAbilitySystemInterface interface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface interface

protected:
	//~ Begin APawn interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	URPGDemoAbilitySystemComponent*	RPGDemoAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	URPGDemoAttributeSet* RPGDemoAttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterData")
	TSoftObjectPtr<UDataAsset_StartUpDataBase> CharacterStartUpData;


public:
	FORCEINLINE URPGDemoAbilitySystemComponent* GetRPGDemoAbilitySystemComponent() const { return RPGDemoAbilitySystemComponent; }
	
	FORCEINLINE URPGDemoAttributeSet* GetRPGDemoAttributeSet() const { return RPGDemoAttributeSet; }
};
