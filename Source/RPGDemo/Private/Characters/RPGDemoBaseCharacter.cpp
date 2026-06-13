// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/RPGDemoBaseCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"

// Sets default values
ARPGDemoBaseCharacter::ARPGDemoBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetMesh()->bReceivesDecals = false;

	RPGDemoAbilitySystemComponent = CreateDefaultSubobject<URPGDemoAbilitySystemComponent>(TEXT("RPGDemoAbilitySystemComponent"));

	RPGDemoAttributeSet = CreateDefaultSubobject<URPGDemoAttributeSet>(TEXT("RPGDemoAttributeSet"));
}

UAbilitySystemComponent* ARPGDemoBaseCharacter::GetAbilitySystemComponent() const
{
	return GetRPGDemoAbilitySystemComponent();
}

UPawnCombatComponent* ARPGDemoBaseCharacter::GetPawnCombatComponent() const
{
	return nullptr;
}

UPawnUIComponent* ARPGDemoBaseCharacter::GetPawnUIComponent() const
{
	return nullptr;
}

void ARPGDemoBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (RPGDemoAbilitySystemComponent)
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);

		ensureMsgf(!CharacterStartUpData.IsNull(), TEXT("CharacterStartUpData is null for %s"), *GetName());
	}
}



