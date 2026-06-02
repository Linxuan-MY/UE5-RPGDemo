// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGDemoFunctionLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"

URPGDemoAbilitySystemComponent* URPGDemoFunctionLibrary::NativeGetRPGDemoASCFromActor(AActor* InActor)
{
	check(InActor);

	return CastChecked<URPGDemoAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor));
}

void URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	if(!ASC->HasMatchingGameplayTag(TagToAdd))
	{
		ASC->AddLooseGameplayTag(TagToAdd);
	}
}

void URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	if(ASC->HasMatchingGameplayTag(TagToRemove))
	{
		ASC->RemoveLooseGameplayTag(TagToRemove);
	}
}

bool URPGDemoFunctionLibrary::NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	return ASC->HasMatchingGameplayTag(TagToCheck);
}

void URPGDemoFunctionLibrary::BP_DoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck, ERPGDemoConfirmType& OutConfirmType)
{
	NativeDoesActorHaveTag(InActor, TagToCheck) ? OutConfirmType = ERPGDemoConfirmType::Yes : OutConfirmType = ERPGDemoConfirmType::No;
}
