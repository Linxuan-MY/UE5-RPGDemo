// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UI/EnemyUIComponent.h"
#include "Widgets/RPGDemoWidgetBase.h"

void UEnemyUIComponent::RegisterEnemyDrawnWidget(URPGDemoWidgetBase* InWidgetToRegister)
{
	EnemyDrawnWidgets.Add(InWidgetToRegister);
}

void UEnemyUIComponent::RemoveEnemyDrawnWidgetsIfAny()
{
	if (EnemyDrawnWidgets.IsEmpty())
	{
		return;
	}

	for (URPGDemoWidgetBase* Widget : EnemyDrawnWidgets)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}

	EnemyDrawnWidgets.Empty();
}
