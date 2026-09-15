// Fill out your copyright notice in the Description page of Project Settings.

#include "RPGDemoGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MoviePlayer.h"
#include "Settings/RPGDemoNetworkSettings.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWidget.h"
#include "Widgets/RPGDemoLobbyWaitingWidget.h"

namespace RPGDemoNetwork
{
    const FString MainMenuMap(TEXT("/Game/Maps/MainMenuMap"));
    const FString LocalFallback(TEXT("127.0.0.1:7777"));
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
}

void URPGDemoGameInstance::Shutdown()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    if (GEngine)
    {
        GEngine->OnTravelFailure().RemoveAll(this);
        GEngine->OnNetworkFailure().RemoveAll(this);
    }
    CancelLoadingScreenTickers();
    HideEditorLoadingScreen();
    PendingGameLevel.Reset();
    bGameLevelTravelInProgress = false;
    Super::Shutdown();
}

bool URPGDemoGameInstance::IsValidDedicatedServerEndpoint(const FString& Endpoint)
{
    FString Host;
    FString PortText;
    if (!Endpoint.TrimStartAndEnd().Split(TEXT(":"), &Host, &PortText, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        return false;
    }
    Host = Host.TrimStartAndEnd();
    PortText = PortText.TrimStartAndEnd();
    if (Host.IsEmpty() || PortText.IsEmpty() || !PortText.IsNumeric() || Host.Contains(TEXT(" ")))
    {
        return false;
    }
    const int32 Port = FCString::Atoi(*PortText);
    return Port > 0 && Port <= 65535;
}

FString URPGDemoGameInstance::GetResolvedDedicatedServerEndpoint() const
{
    FString CommandLineEndpoint;
    if (FParse::Value(FCommandLine::Get(), TEXT("RPGDemoServer="), CommandLineEndpoint))
    {
        CommandLineEndpoint = CommandLineEndpoint.TrimQuotes().TrimStartAndEnd();
        if (IsValidDedicatedServerEndpoint(CommandLineEndpoint))
        {
            return CommandLineEndpoint;
        }
        UE_LOG(LogTemp, Warning, TEXT("Ignoring invalid -RPGDemoServer endpoint: %s"), *CommandLineEndpoint);
    }

    const URPGDemoNetworkSettings* Settings = GetDefault<URPGDemoNetworkSettings>();
    const FString ConfiguredEndpoint = Settings ? Settings->DedicatedServerEndpoint.TrimStartAndEnd() : FString();
    if (IsValidDedicatedServerEndpoint(ConfiguredEndpoint))
    {
        return ConfiguredEndpoint;
    }

    UE_LOG(LogTemp, Warning, TEXT("DedicatedServerEndpoint is invalid; using localhost fallback."));
    return RPGDemoNetwork::LocalFallback;
}

void URPGDemoGameInstance::SetConnectionState(ERPGDemoConnectionState NewState, const FString& Message)
{
    ConnectionState = NewState;
    OnConnectionStateChanged.Broadcast(ConnectionState, Message);
}

void URPGDemoGameInstance::ConnectToDedicatedServer()
{
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer))
    {
        SetConnectionState(ERPGDemoConnectionState::Failed, TEXT("A dedicated server cannot connect as a client."));
        return;
    }

    APlayerController* PlayerController = GetFirstLocalPlayerController();
    if (!PlayerController)
    {
        SetConnectionState(ERPGDemoConnectionState::Failed, TEXT("No local player controller is available."));
        return;
    }

    const FString Endpoint = GetResolvedDedicatedServerEndpoint();
    if (!IsValidDedicatedServerEndpoint(Endpoint))
    {
        SetConnectionState(ERPGDemoConnectionState::Failed, TEXT("The dedicated server address is invalid."));
        return;
    }

    SetConnectionState(ERPGDemoConnectionState::Connecting, FString::Printf(TEXT("Connecting to %s"), *Endpoint));
    PlayerController->ClientTravel(Endpoint, TRAVEL_Absolute);
}

void URPGDemoGameInstance::DisconnectToMainMenu()
{
    SetConnectionState(ERPGDemoConnectionState::Idle, TEXT("Disconnected."));
    if (APlayerController* PlayerController = GetFirstLocalPlayerController();
        PlayerController && GetWorld() && GetWorld()->GetNetMode() == NM_Client)
    {
        PlayerController->ClientTravel(RPGDemoNetwork::MainMenuMap, TRAVEL_Absolute);
        return;
    }
    UGameplayStatics::OpenLevel(this, FName(*RPGDemoNetwork::MainMenuMap));
}

void URPGDemoGameInstance::HandleNetworkFailure(
    UWorld*, UNetDriver*, ENetworkFailure::Type, const FString& ErrorString)
{
    FString Message = ErrorString;
    if (ErrorString.Contains(TEXT("RPGDEMO_SERVER_FULL")))
    {
        Message = TEXT("RPGDEMO_SERVER_FULL: The dedicated server is full.");
    }
    else if (ErrorString.Contains(TEXT("RPGDEMO_MATCH_IN_PROGRESS")))
    {
        Message = TEXT("RPGDEMO_MATCH_IN_PROGRESS: A match is already in progress.");
    }
    else if (Message.IsEmpty())
    {
        Message = TEXT("The connection to the dedicated server failed.");
    }
    SetConnectionState(ERPGDemoConnectionState::Failed, Message);
}

void URPGDemoGameInstance::RefreshLobbyWaitingPlayers() const
{
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!State || (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer))
    {
        return;
    }

    FString PlayersText;
    for (const APlayerState* PlayerState : State->PlayerArray)
    {
        if (!PlayerState) continue;
        const ARPGDemoPlayerState* RPGPlayerState = Cast<ARPGDemoPlayerState>(PlayerState);
        PlayersText += FString::Printf(TEXT("%s%s\n"), *PlayerState->GetPlayerName(),
            RPGPlayerState && RPGPlayerState->bIsLobbyHost ? TEXT("    (Host)") : TEXT(""));
    }
    PlayersText += FString::Printf(TEXT("\n%d / %d players"), State->PlayerArray.Num(), RPGDemoNetwork::MaxPlayers);

    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        UUserWidget* Widget = *It;
        if (!Widget || Widget->GetWorld() != GetWorld() ||
            !Widget->GetClass()->GetName().StartsWith(TEXT("WBP_LobbyWaiting")))
        {
            continue;
        }
        if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PlayerListText"))))
        {
            TextBlock->SetText(FText::FromString(PlayersText));
        }
        if (URPGDemoLobbyWaitingWidget* WaitingWidget = Cast<URPGDemoLobbyWaitingWidget>(Widget))
        {
            WaitingWidget->RefreshLobbyPresentation();
        }
    }
}

void URPGDemoGameInstance::OpenGameLevelWithLoadingScreen(FGameplayTag InTag)
{
    if (bGameLevelTravelInProgress) return;
    PendingGameLevel = GetGameLevelByTag(InTag);
    if (PendingGameLevel.IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("Level tag %s has no configured level."), *InTag.ToString());
        return;
    }
    bGameLevelTravelInProgress = true;
    if (IsMoviePlayerEnabled())
    {
        FLoadingScreenAttributes Attributes;
        Attributes.bAutoCompleteWhenLoadingCompletes = true;
        Attributes.MinimumLoadingScreenDisplayTime = RPGDemoLoadingScreen::MinimumDisplayTime;
        Attributes.WidgetLoadingScreen = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
        GetMoviePlayer()->SetupLoadingScreen(Attributes);
        OpenPendingGameLevel(0.f);
        return;
    }
    if (!ShowEditorLoadingScreen())
    {
        PendingGameLevel.Reset();
        bGameLevelTravelInProgress = false;
        return;
    }
    OpenLevelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::OpenPendingGameLevel), 0.f);
}

void URPGDemoGameInstance::OnDestinationWorldLoaded(UWorld* LoadedWorld)
{
    PendingGameLevel.Reset();
    bGameLevelTravelInProgress = false;
    if (LoadedWorld && LoadedWorld->GetNetMode() == NM_Client &&
        ConnectionState == ERPGDemoConnectionState::Connecting)
    {
        SetConnectionState(ERPGDemoConnectionState::Connected, TEXT("Connected."));
    }
    if (!EditorLoadingScreenWidget.IsValid()) return;
    const double Elapsed = FPlatformTime::Seconds() - LoadingScreenShownAt;
    const float Remaining = FMath::Max(0.f, RPGDemoLoadingScreen::MinimumDisplayTime - static_cast<float>(Elapsed));
    if (Remaining <= 0.f)
    {
        HideEditorLoadingScreen();
        return;
    }
    if (HideLoadingScreenTickerHandle.IsValid()) FTSTicker::RemoveTicker(HideLoadingScreenTickerHandle);
    HideLoadingScreenTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::HideEditorLoadingScreenWhenReady), Remaining);
}

bool URPGDemoGameInstance::OpenPendingGameLevel(float)
{
    OpenLevelTickerHandle.Reset();
    if (PendingGameLevel.IsNull())
    {
        HandleTravelFailure(TEXT("The pending game level became invalid."));
        return false;
    }
    UGameplayStatics::OpenLevelBySoftObjectPtr(this, PendingGameLevel);
    return false;
}

bool URPGDemoGameInstance::HideEditorLoadingScreenWhenReady(float)
{
    HideLoadingScreenTickerHandle.Reset();
    HideEditorLoadingScreen();
    return false;
}

bool URPGDemoGameInstance::ShowEditorLoadingScreen()
{
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!Viewport) return false;
    HideEditorLoadingScreen();
    EditorLoadingScreenWidget = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
    EditorLoadingScreenViewport = Viewport;
    LoadingScreenShownAt = FPlatformTime::Seconds();
    Viewport->AddViewportWidgetContent(EditorLoadingScreenWidget.ToSharedRef(), RPGDemoLoadingScreen::EditorViewportZOrder);
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
    CancelLoadingScreenTickers();
    HideEditorLoadingScreen();
    PendingGameLevel.Reset();
    bGameLevelTravelInProgress = false;
    if (ConnectionState == ERPGDemoConnectionState::Connecting)
    {
        SetConnectionState(ERPGDemoConnectionState::Failed,
            ErrorString.IsEmpty() ? TEXT("Travel to the dedicated server failed.") : ErrorString);
    }
}

TSoftObjectPtr<UWorld> URPGDemoGameInstance::GetGameLevelByTag(FGameplayTag InTag) const
{
    for (const FRPGDemoGameLevelSet& LevelSet : GameLevelSets)
    {
        if (LevelSet.IsValid() && LevelSet.LevelTag == InTag) return LevelSet.Level;
    }
    return {};
}
