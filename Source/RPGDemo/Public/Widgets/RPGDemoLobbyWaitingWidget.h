#pragma once

#include "CoreMinimal.h"
#include "Widgets/RPGDemoWidgetBase.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoLobbyWaitingWidget.generated.h"

class UButton;
class UTextBlock;
class UUserWidget;

UCLASS()
class RPGDEMO_API URPGDemoLobbyWaitingWidget : public URPGDemoWidgetBase
{
	GENERATED_BODY()

public:
	void RefreshLobbyPresentation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UButton* ResolveInnerButton(UUserWidget* ButtonWidget) const;
	void RefreshLobbyDifficulty();

	UFUNCTION()
	void HandleOptionsClicked();

	UFUNCTION()
	void HandleLobbyDifficultyChanged(ERPGDemoGameDifficulty Difficulty);

	UFUNCTION()
	void HandleLobbyPlayersChanged();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DifficultyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUserWidget> OptionsButton;
};