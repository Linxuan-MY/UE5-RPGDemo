// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoFunctionLibrary.generated.h"

struct FGameplayTag;
class URPGDemoAbilitySystemComponent;
class UPawnCombatComponent;
struct FScalableFloat;



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

	static UPawnCombatComponent* NativeGetPawnCombatComponentFromActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|FunctionLibrary", meta = (DisplayName = "Get Pawn Combat Component From Actor", ExpandEnumAsExecs = "OutValidType"))
	static UPawnCombatComponent* BP_GetPawnCombatComponentFromActor(AActor* InActor, ERPGDemoValidType& OutValidType);

	UFUNCTION(BlueprintPure, Category = "RPGDemo|FunctionLibrary")
	static bool TargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn);

	UFUNCTION(BlueprintPure, Category = "RPGDemo|FunctionLibrary", meta = (CompactNodeTitle = "Get Value At Level"))
	static float GetScalableFloatValueAtLevel(const FScalableFloat& InScalableFloat, float InLevel = 1.f);

	UFUNCTION(BlueprintPure, Category = "RPGDemo|FunctionLibrary")
	static FGameplayTag ComputeHitReactDirectionTag(AActor* InAttacker, AActor* InVictim, float& OutAngleDiff);

};
