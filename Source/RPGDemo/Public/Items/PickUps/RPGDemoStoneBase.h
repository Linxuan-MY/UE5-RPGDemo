// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/PickUps/RPGDemoPickUpBase.h"
#include "RPGDemoStoneBase.generated.h"

class UGameplayEffect;
class URPGDemoAbilitySystemComponent;

/**
 *
 */
UCLASS()
class RPGDEMO_API ARPGDemoStoneBase : public ARPGDemoPickUpBase
{
	GENERATED_BODY()

public:
	void Consume(URPGDemoAbilitySystemComponent* AbilitySystemComponent, int32 ApplyLevel);

protected:
	virtual void OnPickUpCollisionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Stone Consumed"))
	void BP_OnStoneConsumed();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> StoneGameplayEffectClass;
};
