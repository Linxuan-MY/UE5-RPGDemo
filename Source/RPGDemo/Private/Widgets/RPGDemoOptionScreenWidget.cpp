#include "Widgets/RPGDemoOptionScreenWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Controllers/RPGDemoLobbyPlayerController.h"
#include "GameModes/RPGDemoGameState.h"
#include "RPGDemoFunctionLibrary.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

UButton* URPGDemoOptionScreenWidget::ResolveInnerButton(UUserWidget* ButtonWidget) const
{
	return ButtonWidget ? Cast<UButton>(ButtonWidget->GetWidgetFromName(TEXT("RPGDemoButton_Base"))) : nullptr;
}

void URPGDemoOptionScreenWidget::ClearBlueprintClickBinding(UUserWidget* ButtonWidget) const
{
	if (!ButtonWidget) return;
	if (const FMulticastDelegateProperty* ClickProperty =
		FindFProperty<FMulticastDelegateProperty>(ButtonWidget->GetClass(), TEXT("OnButtonClicked")))
	{
		ClickProperty->ContainerPtrToValuePtr<FMulticastScriptDelegate>(ButtonWidget)->Clear();
	}
}

void URPGDemoOptionScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARPGDemoLobbyPlayerController* LobbyPC = Cast<ARPGDemoLobbyPlayerController>(GetOwningPlayer());
	bLobbyMode = IsValid(LobbyPC);

	if (bLobbyMode)
	{
		if (ARPGDemoGameState* State = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr)
		{
			CurrentDifficulty = State->LobbyDifficulty;
			State->OnLobbyDifficultyChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyDifficultyChanged);
		}
	}
	else
	{
		URPGDemoFunctionLibrary::TryLoadSavedGameDifficulty(CurrentDifficulty);
	}

	ClearBlueprintClickBinding(WBP_DifficultyBackButton);
	ClearBlueprintClickBinding(WBP_DifficultyForwardButton);
	ClearBlueprintClickBinding(WBP_BackButton);

	if (UButton* Button = ResolveInnerButton(WBP_DifficultyBackButton))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDifficultyBack);
	}
	if (UButton* Button = ResolveInnerButton(WBP_DifficultyForwardButton))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDifficultyForward);
	}
	if (UButton* Button = ResolveInnerButton(WBP_BackButton))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	}

	RefreshDifficultyText();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			RefreshDifficultyText();
		}));
	}
}

void URPGDemoOptionScreenWidget::NativeDestruct()
{
	if (bLobbyMode)
	{
		if (ARPGDemoGameState* State = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr)
		{
			State->OnLobbyDifficultyChanged.RemoveDynamic(this, &ThisClass::HandleLobbyDifficultyChanged);
		}
	}

	Super::NativeDestruct();
}

void URPGDemoOptionScreenWidget::RefreshDifficultyText()
{
	if (TextBlock_Difficulty)
	{
		TextBlock_Difficulty->SetText(URPGDemoFunctionLibrary::GetGameDifficultyDisplayText(CurrentDifficulty));
	}
}

void URPGDemoOptionScreenWidget::StepDifficulty(int32 Direction)
{
	constexpr int32 DifficultyCount = 4;
	const int32 CurrentIndex = static_cast<int32>(CurrentDifficulty);
	CurrentDifficulty = static_cast<ERPGDemoGameDifficulty>((CurrentIndex + Direction + DifficultyCount) % DifficultyCount);
	RefreshDifficultyText();

	if (bLobbyMode)
	{
		if (ARPGDemoLobbyPlayerController* LobbyPC = Cast<ARPGDemoLobbyPlayerController>(GetOwningPlayer()))
		{
			LobbyPC->ServerSetLobbyDifficulty(CurrentDifficulty);
		}
	}
}

void URPGDemoOptionScreenWidget::HandleDifficultyBack()
{
	StepDifficulty(-1);
}

void URPGDemoOptionScreenWidget::HandleDifficultyForward()
{
	StepDifficulty(1);
}

void URPGDemoOptionScreenWidget::HandleBack()
{
	if (!bLobbyMode)
	{
		URPGDemoFunctionLibrary::SaveCurrentGameDifficulty(CurrentDifficulty);
	}
	RemoveFromParent();
}

void URPGDemoOptionScreenWidget::HandleLobbyDifficultyChanged(ERPGDemoGameDifficulty Difficulty)
{
	CurrentDifficulty = Difficulty;
	RefreshDifficultyText();
}
