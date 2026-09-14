// Fill out your copyright notice in the Description page of Project Settings.


#include "Controllers/RPGDemoHeroController.h"

ARPGDemoHeroController::ARPGDemoHeroController()
{
	HeroTeamId = FGenericTeamId(0);
}

void ARPGDemoHeroController::BeginPlay()
{
	Super::BeginPlay();
	RestoreGameplayInput();
}

void ARPGDemoHeroController::AcknowledgePossession(APawn* PossessedPawn)
{
	Super::AcknowledgePossession(PossessedPawn);
	RestoreGameplayInput();
}

void ARPGDemoHeroController::RestoreGameplayInput()
{
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	FlushPressedKeys();

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->EnableInput(this);
	}

	UE_LOG(LogTemp, Log, TEXT("Restored gameplay input for %s"), *GetNameSafe(this));
}

FGenericTeamId ARPGDemoHeroController::GetGenericTeamId() const
{
    return HeroTeamId;
}
