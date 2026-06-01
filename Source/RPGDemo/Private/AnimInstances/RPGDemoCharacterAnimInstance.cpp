// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/RPGDemoCharacterAnimInstance.h"
#include "Characters/RPGDemoBaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void URPGDemoCharacterAnimInstance::NativeInitializeAnimation()
{
	OwningCharacter = Cast<ARPGDemoBaseCharacter>(TryGetPawnOwner());

	if(OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
	}
}


void URPGDemoCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	if(!OwningCharacter || !OwningMovementComponent)
	{
		return;
	}

	GroundSpeed = OwningCharacter->GetVelocity().Size2D();

	bHasAcceleration = OwningMovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;

}
