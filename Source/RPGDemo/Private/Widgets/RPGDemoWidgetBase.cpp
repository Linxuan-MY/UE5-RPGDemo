// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/RPGDemoWidgetBase.h"
#include "Interfaces/PawnUIInterface.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Components/UI/HeroUIComponent.h"

TArray<TWeakObjectPtr<URPGDemoWidgetBase>> URPGDemoWidgetBase::MenuLayerStack;

void URPGDemoWidgetBase::PruneMenuLayerStack()
{
	MenuLayerStack.RemoveAll([](const TWeakObjectPtr<URPGDemoWidgetBase>& Widget)
	{
		return !Widget.IsValid() || !Widget->IsInViewport();
	});
}

void URPGDemoWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(GetOwningPlayerPawn()))
	{
		if (UHeroUIComponent* HeroUIComponent = PawnUIInterface->GetHeroUIComponent())
		{
			BP_OnOwningHeroUIComponentInitialized(HeroUIComponent);

			// Replicated attributes may arrive before the overlay binds its delegates.
			// Push the current snapshot immediately after Blueprint finishes binding.
			if (IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwningPlayerPawn()))
			{
				if (const UAbilitySystemComponent* ASC = AbilitySystemInterface->GetAbilitySystemComponent())
				{
					if (const URPGDemoAttributeSet* Attributes = ASC->GetSet<URPGDemoAttributeSet>())
					{
						HeroUIComponent->OnCurrentHealthChanged.Broadcast(
							Attributes->GetMaxHealth() > 0.f ? Attributes->GetCurrentHealth() / Attributes->GetMaxHealth() : 0.f);
						HeroUIComponent->OnCurrentRageChanged.Broadcast(
							Attributes->GetMaxRage() > 0.f ? Attributes->GetCurrentRage() / Attributes->GetMaxRage() : 0.f);
					}
				}
			}
		}
	}

	if (bParticipateInMenuLayerStack)
	{
		PruneMenuLayerStack();

		const ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();

		for (const TWeakObjectPtr<URPGDemoWidgetBase>& ExistingWidget : MenuLayerStack)
		{
			if (ExistingWidget.IsValid() &&
				ExistingWidget->GetOwningLocalPlayer() == OwningLocalPlayer)
			{
				ExistingWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}

		MenuLayerStack.AddUnique(this);
		SetVisibility(ESlateVisibility::Visible);
	}
}

void URPGDemoWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (bParticipateInMenuLayerStack)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			FInputModeUIOnly InputMode;
			if (IsFocusable()) { InputMode.SetWidgetToFocus(TakeWidget()); }
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = true;
		}

		if (IsFocusable()) { SetFocus(); }
	}
}

void URPGDemoWidgetBase::NativeDestruct()
{
	if (bParticipateInMenuLayerStack)
	{
		MenuLayerStack.Remove(this);
		PruneMenuLayerStack();

		const ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();
		for (int32 Index = MenuLayerStack.Num() - 1; Index >= 0; --Index)
		{
			if (URPGDemoWidgetBase* PreviousWidget = MenuLayerStack[Index].Get();
				PreviousWidget && PreviousWidget->GetOwningLocalPlayer() == OwningLocalPlayer)
			{
				PreviousWidget->SetVisibility(ESlateVisibility::Visible);
				if (PreviousWidget->IsFocusable()) { PreviousWidget->SetFocus(); }

				if (APlayerController* PlayerController = PreviousWidget->GetOwningPlayer())
				{
					FInputModeUIOnly InputMode;
					if (PreviousWidget->IsFocusable()) { InputMode.SetWidgetToFocus(PreviousWidget->TakeWidget()); }
					PlayerController->SetInputMode(InputMode);
					PlayerController->bShowMouseCursor = true;
				}

				break;
			}
		}
	}

	Super::NativeDestruct();
}

void URPGDemoWidgetBase::InitEnemyCreatedWidget(AActor* OwningEnemyActor)
{
	if (IPawnUIInterface* PawnUIInterface = Cast<IPawnUIInterface>(OwningEnemyActor))
	{
		UEnemyUIComponent* EnemyUIComponent = PawnUIInterface->GetEnemyUIComponent();

		checkf(EnemyUIComponent, TEXT("InitEnemyCreatedWidget was called with an actor that implements IPawnUIInterface but does not return a valid EnemyUIComponent. Actor: %s"), *OwningEnemyActor->GetActorNameOrLabel());

		BP_OnOwningEnemyUIComponentInitialized(EnemyUIComponent);
	}
}
