// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/RPGDemoWidgetBase.h"
#include "Interfaces/PawnUIInterface.h"

void URPGDemoWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if(IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(GetOwningPlayerPawn()))
	{
		if(UHeroUIComponent* HeroUIComponent = PawnUIInterface->GetHeroUIComponent())
		{
			BP_OnOwningHeroUIComponentInitialized(HeroUIComponent);
		}
	}
}

void URPGDemoWidgetBase::InitEnemyCreatedWidget(AActor* OwningEnemyActor)
{
	if(IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(OwningEnemyActor))
	{
		UEnemyUIComponent* EnemyUIComponent = PawnUIInterface->GetEnemyUIComponent();

		checkf(EnemyUIComponent, TEXT("InitEnemyCreatedWidget was called with an actor that implements IPawnUIInterface but does not return a valid EnemyUIComponent. Actor: %s"), *OwningEnemyActor->GetActorNameOrLabel());

		BP_OnOwningEnemyUIComponentInitialized(EnemyUIComponent);
	}
}

