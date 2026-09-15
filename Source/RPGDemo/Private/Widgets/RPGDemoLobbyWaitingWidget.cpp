#include "Widgets/RPGDemoLobbyWaitingWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameModes/RPGDemoGameState.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameInstance.h"

UButton* URPGDemoLobbyWaitingWidget::ResolveInnerButton(UUserWidget* ButtonWidget) const
{
	return ButtonWidget ? Cast<UButton>(ButtonWidget->GetWidgetFromName(TEXT("RPGDemoButton_Base"))) : nullptr;
}

void URPGDemoLobbyWaitingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UButton* Button = ResolveInnerButton(OptionsButton))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOptionsClicked);
	}

	if (ARPGDemoGameState* State = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr)
	{
		State->OnLobbyDifficultyChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyDifficultyChanged);
		State->OnLobbyPlayersChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyPlayersChanged);
	}
	RefreshLobbyPresentation();
}

void URPGDemoLobbyWaitingWidget::NativeDestruct()
{
	if (ARPGDemoGameState* State = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr)
	{
		State->OnLobbyDifficultyChanged.RemoveDynamic(this, &ThisClass::HandleLobbyDifficultyChanged);
		State->OnLobbyPlayersChanged.RemoveDynamic(this, &ThisClass::HandleLobbyPlayersChanged);
	}
	Super::NativeDestruct();
}

void URPGDemoLobbyWaitingWidget::RefreshLobbyPresentation()
{
	const APlayerController* OwningController = GetOwningPlayer();
	const ARPGDemoPlayerState* PlayerState = OwningController ? OwningController->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
	if (OptionsButton)
	{
		OptionsButton->SetVisibility(PlayerState && PlayerState->bIsLobbyHost
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	RefreshLobbyDifficulty();
}
void URPGDemoLobbyWaitingWidget::RefreshLobbyDifficulty()
{
	const ARPGDemoGameState* State = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr;
	if (State && DifficultyText)
	{
		DifficultyText->SetText(FText::Format(
			NSLOCTEXT("RPGDemoDifficulty", "LobbyDifficulty", "Difficulty: {0}"),
			URPGDemoFunctionLibrary::GetGameDifficultyDisplayText(State->LobbyDifficulty)));
	}
}

void URPGDemoLobbyWaitingWidget::HandleOptionsClicked()
{
	const APlayerController* OwningController = GetOwningPlayer();
	const ARPGDemoPlayerState* PlayerState = OwningController ? OwningController->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->bIsLobbyHost || !GetOwningPlayer())
	{
		return;
	}

	const TSoftClassPtr<UUserWidget> OptionsClass(
		FSoftObjectPath(TEXT("/Game/Widgets/GameModeWidgets/WBP_OptionScreen.WBP_OptionScreen_C")));
	if (UClass* LoadedClass = OptionsClass.LoadSynchronous())
	{
		if (UUserWidget* OptionsScreen = CreateWidget<UUserWidget>(GetOwningPlayer(), LoadedClass))
		{
			OptionsScreen->AddToViewport(101);
		}
	}
}

void URPGDemoLobbyWaitingWidget::HandleLobbyDifficultyChanged(ERPGDemoGameDifficulty Difficulty)
{
	RefreshLobbyPresentation();
}

void URPGDemoLobbyWaitingWidget::HandleLobbyPlayersChanged()
{
	RefreshLobbyPresentation();
}