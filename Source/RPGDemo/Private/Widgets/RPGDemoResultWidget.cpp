// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/RPGDemoResultWidget.h"

#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"

void URPGDemoResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}

	if (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		for (const FName RestartWidgetName : {FName(TEXT("WBP_PlayAgainButton")), FName(TEXT("WBP_TryAgainButton"))})
		{
			if (UWidget* RestartWidget = GetWidgetFromName(RestartWidgetName))
			{
				RestartWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}
