#pragma once

#include "CoreMinimal.h"
#include "Widgets/RPGDemoWidgetBase.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoOptionScreenWidget.generated.h"

class UButton;
class UTextBlock;
class UUserWidget;

UCLASS()
class RPGDEMO_API URPGDemoOptionScreenWidget : public URPGDemoWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UButton* ResolveInnerButton(UUserWidget* ButtonWidget) const;
	void ClearBlueprintClickBinding(UUserWidget* ButtonWidget) const;
	void RefreshDifficultyText();
	void StepDifficulty(int32 Direction);

	UFUNCTION()
	void HandleDifficultyBack();

	UFUNCTION()
	void HandleDifficultyForward();

	UFUNCTION()
	void HandleBack();

	UFUNCTION()
	void HandleLobbyDifficultyChanged(ERPGDemoGameDifficulty Difficulty);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TextBlock_Difficulty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> WBP_DifficultyBackButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> WBP_DifficultyForwardButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> WBP_BackButton;

	ERPGDemoGameDifficulty CurrentDifficulty = ERPGDemoGameDifficulty::Normal;
	bool bLobbyMode = false;
};
