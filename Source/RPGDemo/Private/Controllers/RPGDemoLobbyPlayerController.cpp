// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/RPGDemoLobbyPlayerController.h"
#include "GameModes/RPGDemoLobbyGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "RPGDemoGameInstance.h"

ARPGDemoLobbyPlayerController::ARPGDemoLobbyPlayerController()
{
	// Lobby presentation always uses the level camera. Without this, a late
	// client's DefaultPawn possession can overwrite the view target selected in
	// BeginPlay and expose the unframed fire effect/black clear background.
	bAutoManageActiveCameraTarget = false;
}

void ARPGDemoLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (ACameraActor* LobbyCamera = Cast<ACameraActor>(
			UGameplayStatics::GetActorOfClass(this, ACameraActor::StaticClass())))
		{
			SetViewTarget(LobbyCamera);
			UE_LOG(LogTemp, Log, TEXT("Lobby camera assigned to local controller %s: %s"),
				*GetName(), *LobbyCamera->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No lobby camera was found for local controller %s."), *GetName());
		}

		const TSoftClassPtr<UUserWidget> WaitingScreenClass(
			FSoftObjectPath(TEXT("/Game/Widgets/GameModeWidgets/WBP_LobbyWaiting.WBP_LobbyWaiting_C")));
		if (UClass* LoadedClass = WaitingScreenClass.LoadSynchronous())
		{
			if (UUserWidget* WaitingScreen = CreateWidget<UUserWidget>(this, LoadedClass))
			{
				WaitingScreen->AddToViewport(100);
				if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
			}
		}
	}
}

void ARPGDemoLobbyPlayerController::ServerSetLobbyDifficulty_Implementation(ERPGDemoGameDifficulty Difficulty)
{
	if (ARPGDemoLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<ARPGDemoLobbyGameMode>() : nullptr)
	{
		Lobby->SetDifficultyForHost(this, Difficulty);
	}
}

void ARPGDemoLobbyPlayerController::ServerStartGame_Implementation()
{
	if (ARPGDemoLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<ARPGDemoLobbyGameMode>() : nullptr)
	{
		Lobby->StartGameForHost(this);
	}
}
