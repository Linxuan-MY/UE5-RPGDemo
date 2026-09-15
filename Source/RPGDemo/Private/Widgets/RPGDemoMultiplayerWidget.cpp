// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/RPGDemoMultiplayerWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

UButton* URPGDemoMultiplayerWidget::ResolveInnerButton(UUserWidget* ButtonWidget) const
{
	return ButtonWidget ? Cast<UButton>(ButtonWidget->GetWidgetFromName(TEXT("RPGDemoButton_Base"))) : nullptr;
}

void URPGDemoMultiplayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UButton* Button = ResolveInnerButton(CreateRoomButton))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConnectClicked);
	}
	if (UButton* Button = ResolveInnerButton(BackButton))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (URPGDemoGameInstance* GameInstance = GetGameInstance<URPGDemoGameInstance>())
	{
		GameInstance->OnConnectionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleConnectionStateChanged);
		SetStatusText(GameInstance->GetConnectionState(),
			FString::Printf(TEXT("Server: %s"), *GameInstance->GetResolvedDedicatedServerEndpoint()));
	}
}

void URPGDemoMultiplayerWidget::NativeDestruct()
{
	if (URPGDemoGameInstance* GameInstance = GetGameInstance<URPGDemoGameInstance>())
	{
		GameInstance->OnConnectionStateChanged.RemoveDynamic(this, &ThisClass::HandleConnectionStateChanged);
	}
	Super::NativeDestruct();
}

void URPGDemoMultiplayerWidget::HandleConnectClicked()
{
	if (URPGDemoGameInstance* GameInstance = GetGameInstance<URPGDemoGameInstance>())
	{
		GameInstance->ConnectToDedicatedServer();
	}
}

void URPGDemoMultiplayerWidget::HandleBackClicked()
{
	RemoveFromParent();
}

void URPGDemoMultiplayerWidget::HandleConnectionStateChanged(
	ERPGDemoConnectionState State, const FString& Message)
{
	SetStatusText(State, Message);
}

void URPGDemoMultiplayerWidget::SetStatusText(ERPGDemoConnectionState State, const FString& Message)
{
	if (!StatusText)
	{
		return;
	}

	const TCHAR* Prefix = TEXT("Idle");
	switch (State)
	{
	case ERPGDemoConnectionState::Connecting: Prefix = TEXT("Connecting"); break;
	case ERPGDemoConnectionState::Connected: Prefix = TEXT("Connected"); break;
	case ERPGDemoConnectionState::Failed: Prefix = TEXT("Failed"); break;
	default: break;
	}
	StatusText->SetText(FText::FromString(Message.IsEmpty()
		? FString(Prefix)
		: FString::Printf(TEXT("%s: %s"), Prefix, *Message)));
}
