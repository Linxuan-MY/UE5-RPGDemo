// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPGDemoFunctionLibrary.generated.h"

class URPGDemoAbilitySystemComponent;

UENUM()
enum class ERPGDemoConfirmType : uint8
{
	Yes,
	No
};

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static URPGDemoAbilitySystemComponent* NativeGetRPGDemoASCFromActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|FunctionLibrary")
	static void AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|FunctionLibrary")
	static void RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove);

	static bool NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|FunctionLibrary", meta = (DisplayName = "Does Actor Have Tag", ExpandEnumAsExecs = "OutConfirmType"))
	static void BP_DoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck, ERPGDemoConfirmType& OutConfirmType);
	
};
