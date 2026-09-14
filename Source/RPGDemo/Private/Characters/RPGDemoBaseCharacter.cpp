// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/RPGDemoBaseCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "MotionWarpingComponent.h"

// Sets default values
ARPGDemoBaseCharacter::ARPGDemoBaseCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetMesh()->bReceivesDecals = false;

	RPGDemoAbilitySystemComponent = CreateDefaultSubobject<URPGDemoAbilitySystemComponent>(TEXT("RPGDemoAbilitySystemComponent"));
	RPGDemoAbilitySystemComponent->SetIsReplicated(true);
	RPGDemoAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	RPGDemoAttributeSet = CreateDefaultSubobject<URPGDemoAttributeSet>(TEXT("RPGDemoAttributeSet"));

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
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

void ARPGDemoBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Remote player controllers are intentionally not replicated to other clients.
	// Initialize GAS for every character copy so replicated gameplay cues and
	// montages can still drive simulated proxies.
	if (RPGDemoAbilitySystemComponent &&
		RPGDemoAbilitySystemComponent->GetAvatarActor() != this)
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
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

void ARPGDemoBaseCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (RPGDemoAbilitySystemComponent && GetController())
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}



