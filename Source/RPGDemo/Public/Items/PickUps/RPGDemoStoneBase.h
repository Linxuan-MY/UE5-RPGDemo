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
	friend class FRPGDemoNetworkRegressionCommand;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-authoritative, one-shot consumption. */
	bool Consume(URPGDemoAbilitySystemComponent* AbilitySystemComponent, int32 ApplyLevel);

	UFUNCTION(BlueprintPure, Category = "RPGDemo|PickUp")
	bool IsConsumed() const { return bConsumed; }

protected:
	virtual void OnPickUpCollisionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Stone Consumed"))
	void BP_OnStoneConsumed();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> StoneGameplayEffectClass;

private:
	bool bConsumptionInProgress = false;
	bool bLocalConsumptionPresented = false;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnStoneConsumed();

	UPROPERTY(Replicated)
	bool bConsumed = false;
};
