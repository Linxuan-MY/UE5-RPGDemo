// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RPGDemoAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBlackboardComponent;

/**
 * 
 */
UCLASS()
class RPGDEMO_API ARPGDemoAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARPGDemoAIController(const FObjectInitializer& ObjectInitializer);

	//~Begin IGenericTeamAgentInterface Interface
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~End IGenericTeamAgentInterface Interface

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAIPerceptionComponent* EnemyPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAISenseConfig_Sight* AISenseConfig_Sight;

	UFUNCTION()
	virtual void OnEnemyPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	AActor* FindNearestPerceivedHostile(const AActor* ExcludedActor = nullptr) const;
	void SetTargetActor(UBlackboardComponent& BlackboardComponent, AActor* NewTarget, const TCHAR* SwitchReason);
	bool IsTargetSwitchLocked() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config")
	bool bEnableDetourCrowdAvoidance = true;

	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config", meta = (EditCondition = "bEnableDetourCrowdAvoidance", UIMin = "1", UIMax = "4"))
	int32 DetourCrowdAvoidanceQuality = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Detour Crowd Avoidance Config", meta = (EditCondition = "bEnableDetourCrowdAvoidance"))
	float CollisionQueryRange = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Target Selection", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MinimumTargetLockDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Target Selection", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float TargetSwitchDistanceRatio = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Target Selection", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float TargetSwitchMinDistanceAdvantage = 300.0f;

	double TargetLockEndTime = 0.0;
};
