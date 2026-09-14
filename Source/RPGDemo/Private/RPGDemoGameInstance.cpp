// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGDemoGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "MoviePlayer.h"
#include "Widgets/SWidget.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetDriver.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "UObject/UObjectIterator.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/RPGDemoPlayerState.h"

namespace RPGDemoSessions
{
	const FName RoomCodeKey(TEXT("RPGDemoRoomCode"));
	const FName HostNameKey(TEXT("RPGDemoHostName"));
	const FString LobbyMap(TEXT("/Game/Maps/MultiplayerLobbyMap?listen?game=/Script/RPGDemo.RPGDemoLobbyGameMode"));
	const FString SurvivalMap(TEXT("/Game/Maps/SurvivalGameModeMap?listen"));
	const FString MainMenuMap(TEXT("/Game/Maps/MainMenuMap"));
	constexpr int32 MaxPlayers = 4;
}

namespace RPGDemoLoadingScreen
{
	constexpr float MinimumDisplayTime = 2.f;
	constexpr int32 EditorViewportZOrder = 10000;
}

void URPGDemoGameInstance::Init()
{
	Super::Init();

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnDestinationWorldLoaded);

	if (GEngine)
	{
		GEngine->OnTravelFailure().AddWeakLambda(this,
			[this](UWorld*, ETravelFailure::Type, const FString& ErrorString)
			{
				HandleTravelFailure(ErrorString);
			});
		GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}

	// World-aware lookup is required for PIE: each PIE world has its own
	// OnlineSubsystemNull context and GameSession namespace.
	IOnlineSubsystem* OnlineSubsystem = GetWorld() ? Online::GetSubsystem(GetWorld()) : nullptr;
	if (!OnlineSubsystem)
	{
		OnlineSubsystem = IOnlineSubsystem::Get();
	}
	if (OnlineSubsystem)
	{
		SessionInterface = OnlineSubsystem->GetSessionInterface();
		UE_LOG(LogTemp, Log, TEXT("Multiplayer session service initialized: %s (World=%s)"),
			*OnlineSubsystem->GetSubsystemName().ToString(), *GetNameSafe(GetWorld()));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No online subsystem is available for multiplayer rooms."));
	}
}

void URPGDemoGameInstance::Shutdown()
{
	EndRoomAutoRefresh();
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
		GEngine->OnNetworkFailure().RemoveAll(this);
	}

	ClearSessionDelegates();
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SessionInterface->DestroySession(NAME_GameSession);
	}
	SessionSearch.Reset();
	SessionInterface.Reset();

	CancelLoadingScreenTickers();
	HideEditorLoadingScreen();
	PendingGameLevel.Reset();
	bGameLevelTravelInProgress = false;

	Super::Shutdown();
}

void URPGDemoGameInstance::RefreshRooms()
{
	bool bHasMultiplayerScreen = false;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		if (*It && It->GetWorld() == GetWorld() && It->GetClass()->GetName().StartsWith(TEXT("WBP_MultiScreen")))
		{
			bHasMultiplayerScreen = true;
			break;
		}
	}
	if (!bHasMultiplayerScreen)
	{
		EndRoomAutoRefresh();
		return;
	}
	if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(RoomRefreshTimerHandle))
	{
		World->GetTimerManager().SetTimer(RoomRefreshTimerHandle, this, &ThisClass::RefreshRooms, 5.f, true);
	}
	if (bSessionOperationInProgress) return;
	FindRooms(false);
}

void URPGDemoGameInstance::BeginRoomAutoRefresh()
{
	RefreshRooms();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RoomRefreshTimerHandle, this, &ThisClass::RefreshRooms, 5.f, true);
	}
}

void URPGDemoGameInstance::EndRoomAutoRefresh()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RoomRefreshTimerHandle);
}

void URPGDemoGameInstance::CreateRoom(const FString& HostName)
{
	PendingHostName = HostName.TrimStartAndEnd().IsEmpty() ? TEXT("Host") : HostName.TrimStartAndEnd();

	if (bSessionOperationInProgress)
	{
		// The multiplayer screen always starts with RefreshRooms. Reuse that
		// in-flight search instead of dropping a create click made while the
		// initial refresh is still completing.
		if (FindSessionsHandle.IsValid())
		{
			bCreateAfterSearch = true;
			PendingJoinCode.Reset();
			UpdateMultiplayerScreenText(TEXT("StatusText"), TEXT("Creating room..."));
			UE_LOG(LogTemp, Log, TEXT("CreateRoom queued behind the active LAN search."));
			return;
		}

		OnSessionOperationComplete.Broadcast(false, TEXT("Another multiplayer operation is already in progress."));
		UE_LOG(LogTemp, Warning, TEXT("CreateRoom rejected because a non-search session operation is active."));
		return;
	}

	UpdateMultiplayerScreenText(TEXT("StatusText"), TEXT("Creating room..."));
	UE_LOG(LogTemp, Log, TEXT("CreateRoom starting LAN availability search for host '%s'."), *PendingHostName);
	FindRooms(true);
}

void URPGDemoGameInstance::JoinRoomByCode(const FString& RoomCode)
{
	const FString NormalizedRoomCode = RoomCode.TrimStartAndEnd();
	if (NormalizedRoomCode.Len() != 6 || !NormalizedRoomCode.IsNumeric())
	{
		OnSessionOperationComplete.Broadcast(false, TEXT("Enter a six-digit room code."));
		return;
	}

	if (bSessionOperationInProgress)
	{
		// Room discovery is continuously refreshed while the multiplayer screen
		// is open. Queue the join behind an active search so a button click is not
		// lost during the LAN query window.
		if (FindSessionsHandle.IsValid())
		{
			bCreateAfterSearch = false;
			PendingJoinCode = NormalizedRoomCode;
			UpdateMultiplayerScreenText(TEXT("StatusText"), TEXT("Joining room..."));
			UE_LOG(LogTemp, Log, TEXT("JoinRoomByCode queued room %s behind the active LAN search."),
				*PendingJoinCode);
			return;
		}

		OnSessionOperationComplete.Broadcast(false, TEXT("Another multiplayer operation is already in progress."));
		UE_LOG(LogTemp, Warning, TEXT("JoinRoomByCode rejected because a non-search session operation is active."));
		return;
	}

	UpdateMultiplayerScreenText(TEXT("StatusText"), TEXT("Joining room..."));
	UE_LOG(LogTemp, Log, TEXT("JoinRoomByCode starting LAN search for room %s."), *NormalizedRoomCode);
	FindRooms(false, NormalizedRoomCode);
}
void URPGDemoGameInstance::FindRooms(bool bForCreate, const FString& RequestedCode)
{
	if (!SessionInterface.IsValid())
	{
		FinishSessionOperation(false, TEXT("LAN service is unavailable."));
		return;
	}

	bSessionOperationInProgress = true;
	bCreateAfterSearch = bForCreate;
	PendingJoinCode = RequestedCode;
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->PingBucketSize = 50;

	FindSessionsHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));
	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		FindSessionsHandle.Reset();
		FinishSessionOperation(false, TEXT("Could not start the LAN room search."));
	}
}

void URPGDemoGameInstance::HandleFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("LAN room search completed. Success=%s Results=%d CreateAfterSearch=%s"),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0,
		bCreateAfterSearch ? TEXT("true") : TEXT("false"));
	if (SessionInterface.IsValid() && FindSessionsHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		FindSessionsHandle.Reset();
	}

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		FinishSessionOperation(false, TEXT("Room search failed."));
		return;
	}

	TArray<FRPGDemoRoomInfo> Rooms;
	for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
	{
		FString Code;
		if (!Result.Session.SessionSettings.Get(RPGDemoSessions::RoomCodeKey, Code) || Code.Len() != 6)
		{
			continue;
		}

		FRPGDemoRoomInfo& Room = Rooms.AddDefaulted_GetRef();
		Room.RoomCode = Code;
		Result.Session.SessionSettings.Get(RPGDemoSessions::HostNameKey, Room.HostName);
		Room.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Room.CurrentPlayers = Room.MaxPlayers - Result.Session.NumOpenPublicConnections;
		Room.bIsFull = Result.Session.NumOpenPublicConnections <= 0;
	}

	OnRoomsUpdated.Broadcast(Rooms);
	FString RoomListText;
	for (const FRPGDemoRoomInfo& Room : Rooms)
	{
		RoomListText += FString::Printf(TEXT("Room %s    Host: %s    %d/%d%s\n"),
			*Room.RoomCode, *Room.HostName, Room.CurrentPlayers, Room.MaxPlayers,
			Room.bIsFull ? TEXT("    FULL") : TEXT(""));
	}
	if (RoomListText.IsEmpty()) RoomListText = TEXT("No LAN rooms found.");
	UpdateMultiplayerScreenText(TEXT("RoomListText"), RoomListText);

	if (bCreateAfterSearch)
	{
		bCreateAfterSearch = false;
		CurrentRoomCode = GenerateUnusedRoomCode();
		if (CurrentRoomCode.IsEmpty())
		{
			FinishSessionOperation(false, TEXT("Could not allocate a room code."));
			return;
		}

		FOnlineSessionSettings Settings;
		Settings.bIsLANMatch = true;
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bAllowJoinViaPresence = true;
		Settings.NumPublicConnections = RPGDemoSessions::MaxPlayers;
		Settings.Set(RPGDemoSessions::RoomCodeKey, CurrentRoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings.Set(RPGDemoSessions::HostNameKey, PendingHostName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		CreateSessionHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));
		UE_LOG(LogTemp, Log, TEXT("Creating LAN room %s with %d public connections."),
			*CurrentRoomCode, RPGDemoSessions::MaxPlayers);
		if (!SessionInterface->CreateSession(0, NAME_GameSession, Settings))
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
			CreateSessionHandle.Reset();
			FinishSessionOperation(false, TEXT("Could not create the room."));
		}
		return;
	}

	if (!PendingJoinCode.IsEmpty())
	{
		const FString RequestedCode = PendingJoinCode;
		PendingJoinCode.Reset();
		for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
		{
			FString Code;
			Result.Session.SessionSettings.Get(RPGDemoSessions::RoomCodeKey, Code);
			if (Code == RequestedCode)
			{
				UE_LOG(LogTemp, Log, TEXT("Found requested LAN room %s. OpenConnections=%d."),
					*RequestedCode, Result.Session.NumOpenPublicConnections);
				if (Result.Session.NumOpenPublicConnections <= 0)
				{
					FinishSessionOperation(false, TEXT("That room is full."));
					return;
				}
				JoinSessionHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
					FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));
				if (!SessionInterface->JoinSession(0, NAME_GameSession, Result))
				{
					SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
					JoinSessionHandle.Reset();
					FinishSessionOperation(false, TEXT("Could not join the room."));
				}
				return;
			}
		}
		FinishSessionOperation(false, TEXT("Room not found."));
		return;
	}

	FinishSessionOperation(true, TEXT("Room list refreshed."));
}

FString URPGDemoGameInstance::GenerateUnusedRoomCode() const
{
	for (int32 Attempt = 0; Attempt < 100; ++Attempt)
	{
		const FString Candidate = FString::Printf(TEXT("%06d"), FMath::RandRange(0, 999999));
		bool bUsed = false;
		if (SessionSearch.IsValid())
		{
			for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
			{
				FString Existing;
				Result.Session.SessionSettings.Get(RPGDemoSessions::RoomCodeKey, Existing);
				bUsed |= Existing == Candidate;
			}
		}
		if (!bUsed) return Candidate;
	}
	return FString();
}

void URPGDemoGameInstance::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("CreateSession completed. Session=%s Success=%s Room=%s"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"), *CurrentRoomCode);
	if (SessionInterface.IsValid() && CreateSessionHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		CreateSessionHandle.Reset();
	}
	bIsRoomHost = bWasSuccessful;
	FinishSessionOperation(bWasSuccessful, bWasSuccessful ? TEXT("Room created.") : TEXT("Room creation failed."));
	if (bWasSuccessful && GetWorld()) GetWorld()->ServerTravel(RPGDemoSessions::LobbyMap);
}

void URPGDemoGameInstance::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid() && JoinSessionHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		JoinSessionHandle.Reset();
	}
	FString Address;
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success && SessionInterface.IsValid() &&
		SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);
	UE_LOG(LogTemp, Log, TEXT("JoinSession completed. Session=%s Result=%d ResolvedAddress=%s Success=%s"),
		*SessionName.ToString(), static_cast<int32>(Result),
		Address.IsEmpty() ? TEXT("<none>") : *Address, bSuccess ? TEXT("true") : TEXT("false"));
	if (bSuccess)
	{
		bIsRoomHost = false;
		if (const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession))
		{
			Session->SessionSettings.Get(RPGDemoSessions::RoomCodeKey, CurrentRoomCode);
		}
		if (APlayerController* PC = GetFirstLocalPlayerController()) PC->ClientTravel(Address, TRAVEL_Absolute);
	}
	FinishSessionOperation(bSuccess, bSuccess ? TEXT("Joined room.") : TEXT("Could not connect to the room."));
}

void URPGDemoGameInstance::LeaveRoom()
{
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		bIsRoomHost = false;
		CurrentRoomCode.Reset();
		FinishSessionOperation(true, TEXT("Left room."));
		return;
	}
	if (bSessionOperationInProgress) return;
	bSessionOperationInProgress = true;
	DestroySessionHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
		DestroySessionHandle.Reset();
		FinishSessionOperation(false, TEXT("Could not leave the room."));
	}
}

void URPGDemoGameInstance::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && DestroySessionHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
		DestroySessionHandle.Reset();
	}
	bIsRoomHost = false;
	CurrentRoomCode.Reset();
	FinishSessionOperation(bWasSuccessful, bWasSuccessful ? TEXT("Left room.") : TEXT("Could not close the room cleanly."));
	if (bReturnToMenuAfterDestroy)
	{
		bReturnToMenuAfterDestroy = false;
		UGameplayStatics::OpenLevel(this, FName(*RPGDemoSessions::MainMenuMap));
	}
}

void URPGDemoGameInstance::StartMultiplayerGame(ERPGDemoGameDifficulty Difficulty)
{
	if (!bIsRoomHost || !GetWorld())
	{
		OnSessionOperationComplete.Broadcast(false, TEXT("Only the host can start the game."));
		return;
	}
	if (SessionInterface.IsValid())
	{
		if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession))
		{
			Session->SessionSettings.bAllowJoinInProgress = false;
			Session->SessionSettings.bShouldAdvertise = false;
			SessionInterface->UpdateSession(NAME_GameSession, Session->SessionSettings, true);
		}
	}
	FString DifficultyOption;
	switch (Difficulty)
	{
	case ERPGDemoGameDifficulty::Easy: DifficultyOption = TEXT("Easy"); break;
	case ERPGDemoGameDifficulty::Normal: DifficultyOption = TEXT("Normal"); break;
	case ERPGDemoGameDifficulty::Hard: DifficultyOption = TEXT("Hard"); break;
	case ERPGDemoGameDifficulty::ExtremelyHard: DifficultyOption = TEXT("ExtremelyHard"); break;
	default: DifficultyOption = TEXT("Normal"); break;
	}
	// The lobby is entered with an explicit ?game= option. Relative server travel
	// inherits that option and would run the lobby GameMode on the survival map.
	// Absolute travel flushes the lobby URL so the destination map can select its
	// configured BP_SurvivalGameMode while retaining the options supplied here.
	GetWorld()->ServerTravel(FString::Printf(TEXT("%s?RPGDemoMultiplayer=1?RPGDemoDifficulty=%s"),
		*RPGDemoSessions::SurvivalMap, *DifficultyOption), true);
}

void URPGDemoGameInstance::ReturnToMainMenu()
{
	bReturnToMenuAfterDestroy = true;
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession)) LeaveRoom();
	else
	{
		bReturnToMenuAfterDestroy = false;
		UGameplayStatics::OpenLevel(this, FName(*RPGDemoSessions::MainMenuMap));
	}
}

void URPGDemoGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!bIsRoomHost && (!CurrentRoomCode.IsEmpty() || (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))))
	{
		OnRoomClosed.Broadcast(TEXT("Host disconnected / Room closed"));
		ShowRoomClosedScreen();
	}
}

void URPGDemoGameInstance::ShowRoomClosedScreen()
{
	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!PlayerController) return;

	const TSoftClassPtr<UUserWidget> RoomClosedClass(
		FSoftObjectPath(TEXT("/Game/Widgets/GameModeWidgets/WBP_RoomClosed.WBP_RoomClosed_C")));
	if (UClass* LoadedClass = RoomClosedClass.LoadSynchronous())
	{
		if (UUserWidget* RoomClosedScreen = CreateWidget<UUserWidget>(PlayerController, LoadedClass))
		{
			RoomClosedScreen->AddToViewport(10000);
		}
	}
}

void URPGDemoGameInstance::FinishSessionOperation(bool bSuccess, const FString& Message)
{
	bSessionOperationInProgress = false;
	UpdateMultiplayerScreenText(TEXT("StatusText"), Message);
	OnSessionOperationComplete.Broadcast(bSuccess, Message);
}

void URPGDemoGameInstance::UpdateMultiplayerScreenText(FName WidgetName, const FString& Text) const
{
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!Widget || Widget->GetWorld() != GetWorld() || !Widget->GetClass()->GetName().StartsWith(TEXT("WBP_MultiScreen"))) continue;
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget->GetWidgetFromName(WidgetName)))
		{
			TextBlock->SetText(FText::FromString(Text));
		}
	}
}

void URPGDemoGameInstance::RefreshLobbyWaitingPlayers() const
{
	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!State) return;

	FString PlayersText;
	for (const APlayerState* PlayerState : State->PlayerArray)
	{
		if (!PlayerState) continue;
		const ARPGDemoPlayerState* RPGPlayerState = Cast<ARPGDemoPlayerState>(PlayerState);
		PlayersText += FString::Printf(TEXT("%s%s\n"), *PlayerState->GetPlayerName(),
			RPGPlayerState && RPGPlayerState->bIsLobbyHost ? TEXT("    (Host)") : TEXT(""));
	}
	PlayersText += FString::Printf(TEXT("\n%d / %d players"), State->PlayerArray.Num(), RPGDemoSessions::MaxPlayers);

	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!Widget || Widget->GetWorld() != GetWorld() || !Widget->GetClass()->GetName().StartsWith(TEXT("WBP_LobbyWaiting"))) continue;
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PlayerListText"))))
		{
			TextBlock->SetText(FText::FromString(PlayersText));
		}
	}
}

void URPGDemoGameInstance::ClearSessionDelegates()
{
	if (!SessionInterface.IsValid()) return;
	if (FindSessionsHandle.IsValid()) SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
	if (CreateSessionHandle.IsValid()) SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	if (JoinSessionHandle.IsValid()) SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	if (DestroySessionHandle.IsValid()) SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
	FindSessionsHandle.Reset(); CreateSessionHandle.Reset(); JoinSessionHandle.Reset(); DestroySessionHandle.Reset();
}

void URPGDemoGameInstance::OpenGameLevelWithLoadingScreen(FGameplayTag InTag)
{
	if (bGameLevelTravelInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ignoring duplicate level travel request for tag %s."), *InTag.ToString());
		return;
	}

	PendingGameLevel = GetGameLevelByTag(InTag);
	if (PendingGameLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot open a game level because tag %s has no configured level."), *InTag.ToString());
		return;
	}

	bGameLevelTravelInProgress = true;

	if (IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes LoadingScreenAttributes;
		LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes = true;
		LoadingScreenAttributes.MinimumLoadingScreenDisplayTime = RPGDemoLoadingScreen::MinimumDisplayTime;
		LoadingScreenAttributes.WidgetLoadingScreen = FLoadingScreenAttributes::NewTestLoadingScreenWidget();

		GetMoviePlayer()->SetupLoadingScreen(LoadingScreenAttributes);
		UE_LOG(LogTemp, Log, TEXT("Configured MoviePlayer loading screen for %s."), *PendingGameLevel.ToSoftObjectPath().ToString());
		OpenPendingGameLevel(0.f);
		return;
	}

	if (!ShowEditorLoadingScreen())
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot display the editor loading screen because no game viewport is available."));
		PendingGameLevel.Reset();
		bGameLevelTravelInProgress = false;
		return;
	}

	OpenLevelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::OpenPendingGameLevel), 0.f);
	UE_LOG(LogTemp, Log, TEXT("Displayed PIE loading screen for %s; level travel is deferred until the next frame."), *PendingGameLevel.ToSoftObjectPath().ToString());
}

void URPGDemoGameInstance::OnDestinationWorldLoaded(UWorld* LoadedWorld)
{
	UE_LOG(LogTemp, Log, TEXT("Destination world loaded: %s."), *GetNameSafe(LoadedWorld));

	PendingGameLevel.Reset();
	bGameLevelTravelInProgress = false;

	if (!EditorLoadingScreenWidget.IsValid())
	{
		return;
	}

	const double ElapsedTime = FPlatformTime::Seconds() - LoadingScreenShownAt;
	const float RemainingDisplayTime = FMath::Max(0.f,
		RPGDemoLoadingScreen::MinimumDisplayTime - static_cast<float>(ElapsedTime));

	if (RemainingDisplayTime <= 0.f)
	{
		HideEditorLoadingScreen();
		return;
	}

	if (HideLoadingScreenTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(HideLoadingScreenTickerHandle);
	}

	HideLoadingScreenTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HideEditorLoadingScreenWhenReady),
		RemainingDisplayTime);
}

bool URPGDemoGameInstance::OpenPendingGameLevel(float DeltaTime)
{
	OpenLevelTickerHandle.Reset();

	if (PendingGameLevel.IsNull())
	{
		HandleTravelFailure(TEXT("The pending game level became invalid before travel started."));
		return false;
	}

	const TSoftObjectPtr<UWorld> LevelToOpen = PendingGameLevel;
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, LevelToOpen);
	return false;
}

bool URPGDemoGameInstance::HideEditorLoadingScreenWhenReady(float DeltaTime)
{
	HideLoadingScreenTickerHandle.Reset();
	HideEditorLoadingScreen();
	return false;
}

bool URPGDemoGameInstance::ShowEditorLoadingScreen()
{
	UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!GameViewport)
	{
		return false;
	}

	HideEditorLoadingScreen();

	EditorLoadingScreenWidget = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
	EditorLoadingScreenViewport = GameViewport;
	LoadingScreenShownAt = FPlatformTime::Seconds();
	GameViewport->AddViewportWidgetContent(EditorLoadingScreenWidget.ToSharedRef(), RPGDemoLoadingScreen::EditorViewportZOrder);
	return true;
}

void URPGDemoGameInstance::HideEditorLoadingScreen()
{
	if (EditorLoadingScreenWidget.IsValid() && EditorLoadingScreenViewport.IsValid())
	{
		EditorLoadingScreenViewport->RemoveViewportWidgetContent(EditorLoadingScreenWidget.ToSharedRef());
	}

	EditorLoadingScreenWidget.Reset();
	EditorLoadingScreenViewport.Reset();
	LoadingScreenShownAt = 0.0;
}

void URPGDemoGameInstance::CancelLoadingScreenTickers()
{
	if (OpenLevelTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(OpenLevelTickerHandle);
		OpenLevelTickerHandle.Reset();
	}

	if (HideLoadingScreenTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(HideLoadingScreenTickerHandle);
		HideLoadingScreenTickerHandle.Reset();
	}
}

void URPGDemoGameInstance::HandleTravelFailure(const FString& ErrorString)
{
	UE_LOG(LogTemp, Error, TEXT("Game level travel failed: %s"), *ErrorString);
	CancelLoadingScreenTickers();
	HideEditorLoadingScreen();
	PendingGameLevel.Reset();
	bGameLevelTravelInProgress = false;
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		if (bIsRoomHost) SessionInterface->DestroySession(NAME_GameSession);
		else
		{
			OnRoomClosed.Broadcast(TEXT("Host disconnected / Room closed"));
			ShowRoomClosedScreen();
		}
	}
}

TSoftObjectPtr<UWorld> URPGDemoGameInstance::GetGameLevelByTag(FGameplayTag InTag) const
{
	for (const FRPGDemoGameLevelSet& GameLevelSet : GameLevelSets)
	{
		if (!GameLevelSet.IsValid()) continue;

		if (GameLevelSet.LevelTag == InTag)
		{
			return GameLevelSet.Level;
		}
	}

	return TSoftObjectPtr<UWorld>();
}
