// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RPGDemoStructTypes.generated.h"

class URPGDemoHeroLinkedAnimLayer;

USTRUCT(BlueprintType)
struct FRPGDemoHeroWeaponData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<URPGDemoHeroLinkedAnimLayer> WeaponAnimLayerToLink;
	
};