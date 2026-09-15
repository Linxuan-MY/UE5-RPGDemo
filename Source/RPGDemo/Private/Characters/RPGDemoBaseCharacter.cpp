// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/RPGDemoBaseCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "MotionWarpingComponent.h"

ARPGDemoBaseCharacter::ARPGDemoBaseCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    GetMesh()->bReceivesDecals = false;
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

UAbilitySystemComponent* ARPGDemoBaseCharacter::GetAbilitySystemComponent() const
{
    return RPGDemoAbilitySystemComponent;
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
    if (RPGDemoAbilitySystemComponent && RPGDemoAbilitySystemComponent->GetAvatarActor() != this)
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
        AActor* AbilityOwner = RPGDemoAbilitySystemComponent->GetOwnerActor();
        if (!AbilityOwner || AbilityOwner == this)
        {
            RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
        }
    }
}
