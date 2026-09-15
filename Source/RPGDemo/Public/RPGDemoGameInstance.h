// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "RPGDemoGameInstance.generated.h"

class SWidget;
class UGameViewportClient;
class UNetDriver;

UENUM(BlueprintType)
enum class ERPGDemoConnectionState : uint8
{
    Idle,
    Connecting,
    Connected,
    Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FRPGDemoConnectionStateChanged,
    ERPGDemoConnectionState, State,
    const FString&, Message);

USTRUCT(BlueprintType)
struct FRPGDemoGameLevelSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "GameData.Level"))
    FGameplayTag LevelTag;

    UPROPERTY(EditDefaultsOnly)
    TSoftObjectPtr<UWorld> Level;

    bool IsValid() const { return LevelTag.IsValid() && !Level.IsNull(); }
};

UCLASS()
class RPGDEMO_API URPGDemoGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;

    UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer")
    FRPGDemoConnectionStateChanged OnConnectionStateChanged;

    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer")
    void ConnectToDedicatedServer();

    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer")
    void DisconnectToMainMenu();

    UFUNCTION(BlueprintPure, Category = "RPGDemo|Multiplayer")
    FString GetResolvedDedicatedServerEndpoint() const;

    UFUNCTION(BlueprintPure, Category = "RPGDemo|Multiplayer")
    ERPGDemoConnectionState GetConnectionState() const { return ConnectionState; }

    void RefreshLobbyWaitingPlayers() const;

    static bool IsValidDedicatedServerEndpoint(const FString& Endpoint);

    UFUNCTION(BlueprintPure, meta = (Categories = "GameData.Level"))
    TSoftObjectPtr<UWorld> GetGameLevelByTag(FGameplayTag InTag) const;

    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Loading Screen", meta = (Categories = "GameData.Level"))
    void OpenGameLevelWithLoadingScreen(FGameplayTag InTag);

protected:
    void OnDestinationWorldLoaded(UWorld* LoadedWorld);
    void HandleTravelFailure(const FString& ErrorString);
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
    bool OpenPendingGameLevel(float DeltaTime);
    bool HideEditorLoadingScreenWhenReady(float DeltaTime);
    bool ShowEditorLoadingScreen();
    void HideEditorLoadingScreen();
    void CancelLoadingScreenTickers();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FRPGDemoGameLevelSet> GameLevelSets;

private:
    void SetConnectionState(ERPGDemoConnectionState NewState, const FString& Message);

    TSoftObjectPtr<UWorld> PendingGameLevel;
    TSharedPtr<SWidget> EditorLoadingScreenWidget;
    TWeakObjectPtr<UGameViewportClient> EditorLoadingScreenViewport;
    FTSTicker::FDelegateHandle OpenLevelTickerHandle;
    FTSTicker::FDelegateHandle HideLoadingScreenTickerHandle;
    double LoadingScreenShownAt = 0.0;
    bool bGameLevelTravelInProgress = false;
    ERPGDemoConnectionState ConnectionState = ERPGDemoConnectionState::Idle;
};
