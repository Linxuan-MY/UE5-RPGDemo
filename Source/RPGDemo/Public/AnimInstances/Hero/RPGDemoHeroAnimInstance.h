// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/RPGDemoCharacterAnimInstance.h"
#include "RPGDemoHeroAnimInstance.generated.h"

class ARPGDemoHeroCharacter;

/**
 * 
 */
UCLASS()
class RPGDEMO_API URPGDemoHeroAnimInstance : public URPGDemoCharacterAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
    void BeginLocalHitStop(float Duration);
    float GetLocalHitStopAlpha() const { return LocalHitStopAlpha; }

protected:
    UPROPERTY(Transient, BlueprintReadOnly, Category = "AnimData|HitFeedback")
    float LocalHitStopAlpha = 0.f;
    double LocalHitStopEndRealTime = 0.0;
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|References")
	ARPGDemoHeroCharacter* OwningHeroCharacter;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bShouldEnterRelaxState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float EnterRelaxStateThreshold = 5.f;

	float IdleElapsedTime;
	
};
