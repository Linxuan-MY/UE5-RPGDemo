// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/Hero/RPGDemoHeroAnimInstance.h"
#include "Characters/RPGDemoHeroCharacter.h"

void URPGDemoHeroAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if(OwningCharacter)
	{
		OwningHeroCharacter = Cast<ARPGDemoHeroCharacter>(OwningCharacter);
	}
}

void URPGDemoHeroAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if(bHasAcceleration)
	{
		IdleElapsedTime = 0.f;
		bShouldEnterRelaxState = false;
	}
	else
	{
		IdleElapsedTime += DeltaSeconds;
		bShouldEnterRelaxState = (IdleElapsedTime >= EnterRelaxStateThreshold);
	}
}
