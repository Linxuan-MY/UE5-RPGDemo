// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGDemoGameInstance.h"
#include "Widgets/RPGDemoWidgetBase.h"
#include "RPGDemoMultiplayerWidget.generated.h"

class UButton;
class UTextBlock;
class UUserWidget;

UCLASS()
class RPGDEMO_API URPGDemoMultiplayerWidget : public URPGDemoWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UButton* ResolveInnerButton(UUserWidget* ButtonWidget) const;
	void SetStatusText(ERPGDemoConnectionState State, const FString& Message);

	UFUNCTION()
	void HandleConnectClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleConnectionStateChanged(ERPGDemoConnectionState State, const FString& Message);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> CreateRoomButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> BackButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;
};
