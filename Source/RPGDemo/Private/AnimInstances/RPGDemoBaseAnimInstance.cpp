// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/RPGDemoBaseAnimInstance.h"
#include "RPGDemoFunctionLibrary.h"

bool URPGDemoBaseAnimInstance::DoesOwnerHaveTag(FGameplayTag TagToCheck) const
{
    if(APawn* OwningPawn = TryGetPawnOwner())
	{
		return URPGDemoFunctionLibrary::NativeDoesActorHaveTag(OwningPawn, TagToCheck);
	}

	return false;
}
