// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoLobbyGameMode.h"

#include "Controllers/RPGDemoLobbyPlayerController.h"
#include "GameModes/RPGDemoGameState.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "RPGDemoGameInstance.h"

namespace RPGDemoLobby
{
    constexpr int32 MaxPlayers = 4;
    const FString SurvivalMap(TEXT("/Game/Maps/SurvivalGameModeMap"));
}

ARPGDemoLobbyGameMode::ARPGDemoLobbyGameMode()
{
    PlayerControllerClass = ARPGDemoLobbyPlayerController::StaticClass();
    bUseSeamlessTravel = true;
}

void ARPGDemoLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();
    for (APlayerState* BaseState : GameState->PlayerArray)
    {
        if (ARPGDemoPlayerState* State = Cast<ARPGDemoPlayerState>(BaseState))
        {
            if (State->ServerJoinOrder == 0) State->ServerJoinOrder = NextJoinOrder++;
            State->SetLifeState(ERPGDemoPlayerLifeState::Alive);
        }
    }
    AssignLobbyHost();
}

void ARPGDemoLobbyGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty()) return;
    if (bMatchStarting) ErrorMessage = TEXT("RPGDEMO_MATCH_IN_PROGRESS");
    else if (GetNumPlayers() >= RPGDemoLobby::MaxPlayers) ErrorMessage = TEXT("RPGDEMO_SERVER_FULL");
}

void ARPGDemoLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (ARPGDemoPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr)
    {
        if (State->ServerJoinOrder == 0) State->ServerJoinOrder = NextJoinOrder++;
        State->SetLifeState(ERPGDemoPlayerLifeState::Alive);
    }
    AssignLobbyHost();
}

void ARPGDemoLobbyGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    AssignLobbyHost();
}

void ARPGDemoLobbyGameMode::AssignLobbyHost()
{
    ARPGDemoPlayerState* Host = nullptr;
    for (APlayerState* BaseState : GameState->PlayerArray)
    {
        ARPGDemoPlayerState* State = Cast<ARPGDemoPlayerState>(BaseState);
        if (State && (!Host || State->ServerJoinOrder < Host->ServerJoinOrder)) Host = State;
    }
    for (APlayerState* BaseState : GameState->PlayerArray)
    {
        if (ARPGDemoPlayerState* State = Cast<ARPGDemoPlayerState>(BaseState)) State->SetLobbyHost(State == Host);
    }
    if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
}

void ARPGDemoLobbyGameMode::StartGameForHost(APlayerController* RequestingPlayer)
{
    const ARPGDemoPlayerState* State = RequestingPlayer ? RequestingPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
    if (!State || !State->bIsLobbyHost || bMatchStarting) return;
    bMatchStarting = true;
    const ARPGDemoGameState* RPGState = GetGameState<ARPGDemoGameState>();
    const ERPGDemoGameDifficulty Difficulty = RPGState ? RPGState->LobbyDifficulty : ERPGDemoGameDifficulty::Normal;
    FString DifficultyName(TEXT("Normal"));
    switch (Difficulty)
    {
    case ERPGDemoGameDifficulty::Easy: DifficultyName = TEXT("Easy"); break;
    case ERPGDemoGameDifficulty::Hard: DifficultyName = TEXT("Hard"); break;
    case ERPGDemoGameDifficulty::ExtremelyHard: DifficultyName = TEXT("ExtremelyHard"); break;
    default: break;
    }
    GetWorld()->ServerTravel(FString::Printf(TEXT("%s?RPGDemoMultiplayer=1?RPGDemoDifficulty=%s"),
        *RPGDemoLobby::SurvivalMap, *DifficultyName), true);
}

void ARPGDemoLobbyGameMode::SetDifficultyForHost(APlayerController* RequestingPlayer, ERPGDemoGameDifficulty Difficulty)
{
    const ARPGDemoPlayerState* State = RequestingPlayer ? RequestingPlayer->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
    const bool bValid = Difficulty >= ERPGDemoGameDifficulty::Easy && Difficulty <= ERPGDemoGameDifficulty::ExtremelyHard;
    if (State && State->bIsLobbyHost && bValid)
    {
        if (ARPGDemoGameState* RPGState = GetGameState<ARPGDemoGameState>()) RPGState->SetLobbyDifficulty(Difficulty);
    }
}
