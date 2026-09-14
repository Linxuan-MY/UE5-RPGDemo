// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "RPGDemoHeroController.generated.h"

/**
 * 
 */
UCLASS()
class RPGDEMO_API ARPGDemoHeroController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ARPGDemoHeroController();
	virtual void AcknowledgePossession(APawn* PossessedPawn) override;

	//~Begin IGenericTeamAgentInterface Interface
	virtual FGenericTeamId GetGenericTeamId() const override;
	//~End IGenericTeamAgentInterface Interface

private:
	virtual void BeginPlay() override;
	void RestoreGameplayInput();

	FGenericTeamId HeroTeamId;
	
};
