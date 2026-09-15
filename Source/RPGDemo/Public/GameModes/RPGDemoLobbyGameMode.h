// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "RPGDemoLobbyGameMode.generated.h"

UCLASS()
class RPGDEMO_API ARPGDemoLobbyGameMode : public ARPGDemoBaseGameMode
{
    GENERATED_BODY()

public:
    ARPGDemoLobbyGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    void StartGameForHost(APlayerController* RequestingPlayer);
    void SetDifficultyForHost(APlayerController* RequestingPlayer, ERPGDemoGameDifficulty Difficulty);

private:
    void AssignLobbyHost();
    uint32 NextJoinOrder = 1;
    bool bMatchStarting = false;
};
