// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPGDemoFunctionLibrary.generated.h"

class URPGDemoAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static URPGDemoAbilitySystemComponent* NativeGetRPGDemoASCFromActor(AActor* InActor);
	
};
