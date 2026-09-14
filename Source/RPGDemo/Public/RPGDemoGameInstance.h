// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"
#include "RPGDemoGameInstance.generated.h"

class SWidget;
class UGameViewportClient;

USTRUCT(BlueprintType)
struct FRPGDemoRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString RoomCode;
	UPROPERTY(BlueprintReadOnly) FString HostName;
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 4;
	UPROPERTY(BlueprintReadOnly) bool bIsFull = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGDemoRoomsUpdated, const TArray<FRPGDemoRoomInfo>&, Rooms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGDemoSessionOperationComplete, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGDemoLobbyMembersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGDemoRoomClosed, const FString&, Message);

USTRUCT(BlueprintType)
struct FRPGDemoGameLevelSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, meta = (Categories = "GameData.Level"))
	FGameplayTag LevelTag;

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UWorld> Level;

	bool IsValid() const
	{
		return LevelTag.IsValid() && !Level.IsNull();
	}
};

UCLASS()
class RPGDEMO_API URPGDemoGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer") FRPGDemoRoomsUpdated OnRoomsUpdated;
	UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer") FRPGDemoSessionOperationComplete OnSessionOperationComplete;
	UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer") FRPGDemoLobbyMembersChanged OnLobbyMembersChanged;
	UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Multiplayer") FRPGDemoRoomClosed OnRoomClosed;

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void RefreshRooms();
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void BeginRoomAutoRefresh();
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void EndRoomAutoRefresh();
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void CreateRoom(const FString& HostName);
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void JoinRoomByCode(const FString& RoomCode);
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void LeaveRoom();
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void StartMultiplayerGame(ERPGDemoGameDifficulty Difficulty);
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Multiplayer") void ReturnToMainMenu();
	void RefreshLobbyWaitingPlayers() const;
	UFUNCTION(BlueprintPure, Category = "RPGDemo|Multiplayer") bool IsSessionOperationInProgress() const { return bSessionOperationInProgress; }
	UFUNCTION(BlueprintPure, Category = "RPGDemo|Multiplayer") bool IsRoomHost() const { return bIsRoomHost; }
	UFUNCTION(BlueprintPure, Category = "RPGDemo|Multiplayer") FString GetCurrentRoomCode() const { return CurrentRoomCode; }

protected:
	virtual void OnDestinationWorldLoaded(UWorld* LoadedWorld);
	void HandleTravelFailure(const FString& ErrorString);
	bool OpenPendingGameLevel(float DeltaTime);
	bool HideEditorLoadingScreenWhenReady(float DeltaTime);
	bool ShowEditorLoadingScreen();
	void HideEditorLoadingScreen();
	void CancelLoadingScreenTickers();
	void FindRooms(bool bForCreate, const FString& RequestedCode = FString());
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void ClearSessionDelegates();
	void FinishSessionOperation(bool bSuccess, const FString& Message);
	FString GenerateUnusedRoomCode() const;
	void ShowRoomClosedScreen();
	void UpdateMultiplayerScreenText(FName WidgetName, const FString& Text) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FRPGDemoGameLevelSet> GameLevelSets;

private:
	TSoftObjectPtr<UWorld> PendingGameLevel;
	TSharedPtr<SWidget> EditorLoadingScreenWidget;
	TWeakObjectPtr<UGameViewportClient> EditorLoadingScreenViewport;
	FTSTicker::FDelegateHandle OpenLevelTickerHandle;
	FTSTicker::FDelegateHandle HideLoadingScreenTickerHandle;
	double LoadingScreenShownAt = 0.0;
	bool bGameLevelTravelInProgress = false;
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle CreateSessionHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle DestroySessionHandle;
	FString PendingHostName;
	FString PendingJoinCode;
	FString CurrentRoomCode;
	bool bCreateAfterSearch = false;
	bool bSessionOperationInProgress = false;
	bool bIsRoomHost = false;
	bool bReturnToMenuAfterDestroy = false;
	FTimerHandle RoomRefreshTimerHandle;

public:
	UFUNCTION(BlueprintPure, meta = (Categories = "GameData.Level"))
	TSoftObjectPtr<UWorld> GetGameLevelByTag(FGameplayTag InTag) const;

	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Loading Screen", meta = (Categories = "GameData.Level"))
	void OpenGameLevelWithLoadingScreen(FGameplayTag InTag);
};
