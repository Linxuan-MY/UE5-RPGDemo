// Fill out your copyright notice in the Description page of Project Settings.

#include "Controllers/RPGDemoHeroController.h"

#include "TimerManager.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

ARPGDemoHeroController::ARPGDemoHeroController()
{
    HeroTeamId = FGenericTeamId(0);
}

void ARPGDemoHeroController::BeginPlay()
{
    Super::BeginPlay();
    RestoreGameplayInput();
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { BindReplicatedPresentation(); }));
}

void ARPGDemoHeroController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (SurvivalMusicComponent)
    {
        SurvivalMusicComponent->Stop();
        SurvivalMusicComponent = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}
void ARPGDemoHeroController::AcknowledgePossession(APawn* PossessedPawn)
{
    Super::AcknowledgePossession(PossessedPawn);
    RestoreGameplayInput();
    BindReplicatedPresentation();
}

void ARPGDemoHeroController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    BindReplicatedPresentation();
}

void ARPGDemoHeroController::BindReplicatedPresentation()
{
    if (!IsLocalController()) return;
    if (ARPGDemoGameState* State = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr)
    {
        State->OnSurvivalSnapshotChanged.AddUniqueDynamic(this, &ThisClass::HandleSurvivalSnapshotChanged);
        HandleSurvivalSnapshotChanged(State->SurvivalSnapshot);
    }
    if (ARPGDemoPlayerState* State = GetPlayerState<ARPGDemoPlayerState>())
    {
        State->OnLifeStateChanged.AddUniqueDynamic(this, &ThisClass::HandleLocalLifeStateChanged);
        HandleLocalLifeStateChanged(State->LifeState, State->RespawnEndServerTime);
    }
}

void ARPGDemoHeroController::HandleSurvivalSnapshotChanged(const FRPGDemoSurvivalSnapshot& Snapshot)
{
    if (!IsLocalController() || Snapshot.Revision == LastPresentedSnapshotRevision) return;
    LastPresentedSnapshotRevision = Snapshot.Revision;
    if (Snapshot.Revision > 0)
    {
        EnsureSurvivalMusic();
    }
    BP_OnSurvivalSnapshotChanged(Snapshot);

    switch (Snapshot.State)
    {
    case ERPGDemoSurvivalGameModeState::WaitSpawnNewWave:
        ShowCountdownMessage(NSLOCTEXT("RPGDemoSurvival", "NewWaveCountdown", "New Wave Coming In"),
            GetSnapshotSecondsRemaining(Snapshot));
        break;
    case ERPGDemoSurvivalGameModeState::SpawningNewWave:
        ShowTransientMessage(FText::Format(
            NSLOCTEXT("RPGDemoSurvival", "WaveStarted", "Wave {0} Started"),
            FText::AsNumber(Snapshot.CurrentWave)));
        break;
    case ERPGDemoSurvivalGameModeState::WaveCompleted:
        ShowCountdownMessage(NSLOCTEXT("RPGDemoSurvival", "WaveCompleted", "Wave Completed"),
            GetSnapshotSecondsRemaining(Snapshot));
        break;
    case ERPGDemoSurvivalGameModeState::AllWavesDone:
        ShowResultScreen(true);
        break;
    case ERPGDemoSurvivalGameModeState::TeamDefeated:
        ShowResultScreen(false);
        break;
    default:
        break;
    }
}

void ARPGDemoHeroController::HandleLocalLifeStateChanged(ERPGDemoPlayerLifeState LifeState, double RespawnEndServerTime)
{
    if (!IsLocalController()) return;
    if (bHasPresentedLifeState && LastPresentedLifeState == LifeState &&
        FMath::IsNearlyEqual(LastPresentedRespawnEndServerTime, RespawnEndServerTime)) return;
    bHasPresentedLifeState = true;
    LastPresentedLifeState = LifeState;
    LastPresentedRespawnEndServerTime = RespawnEndServerTime;
    BP_OnLocalLifeStateChanged(LifeState, RespawnEndServerTime);
    if (LifeState == ERPGDemoPlayerLifeState::Dead)
    {
        ShowCountdownMessage(NSLOCTEXT("RPGDemoSurvival", "RespawnCountdown", "Respawning In"),
            GetLocalRespawnSecondsRemaining());
    }
}

float ARPGDemoHeroController::GetLocalRespawnSecondsRemaining() const
{
    const ARPGDemoPlayerState* State = GetPlayerState<ARPGDemoPlayerState>();
    const ARPGDemoGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr;
    if (!State || !GameState || State->LifeState != ERPGDemoPlayerLifeState::Dead) return 0.f;
    return FMath::Max(0.0, State->RespawnEndServerTime - GameState->GetServerWorldTimeSeconds());
}

float ARPGDemoHeroController::GetSnapshotSecondsRemaining(const FRPGDemoSurvivalSnapshot& Snapshot) const
{
    const ARPGDemoGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARPGDemoGameState>() : nullptr;
    return GameState ? FMath::Max(0.0, Snapshot.StateEndServerTime - GameState->GetServerWorldTimeSeconds()) : 0.f;
}

void ARPGDemoHeroController::ShowCountdownMessage(const FText& Message, float DurationSeconds)
{
    if (!IsLocalController()) return;
    const TSoftClassPtr<UUserWidget> WidgetClass(FSoftObjectPath(
        TEXT("/Game/Widgets/GameModeWidgets/WBP_WaveTextWithCountDown.WBP_WaveTextWithCountDown_C")));
    UClass* LoadedClass = WidgetClass.LoadSynchronous();
    UUserWidget* Widget = LoadedClass ? CreateWidget<UUserWidget>(this, LoadedClass) : nullptr;
    if (!Widget) return;
    if (UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("TextBlock_WaveText"))))
    {
        Text->SetText(Message);
    }
    Widget->AddToViewport();
    if (UFunction* Function = Widget->FindFunction(TEXT("StartCountDown")))
    {
        FStructOnScope Parameters(Function);
        if (FFloatProperty* DurationProperty = FindFProperty<FFloatProperty>(Function, TEXT("InTotalCountDownTime")))
        {
            DurationProperty->SetPropertyValue_InContainer(Parameters.GetStructMemory(), FMath::Max(0.f, DurationSeconds));
            Widget->ProcessEvent(Function, Parameters.GetStructMemory());
        }
    }
}

void ARPGDemoHeroController::ShowTransientMessage(const FText& Message)
{
    if (!IsLocalController()) return;
    const TSoftClassPtr<UUserWidget> WidgetClass(FSoftObjectPath(
        TEXT("/Game/Widgets/GameModeWidgets/WBP_WaveTextNoCountDown.WBP_WaveTextNoCountDown_C")));
    UClass* LoadedClass = WidgetClass.LoadSynchronous();
    UUserWidget* Widget = LoadedClass ? CreateWidget<UUserWidget>(this, LoadedClass) : nullptr;
    if (!Widget) return;
    if (UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("TextBlock_WaveText"))))
    {
        Text->SetText(Message);
    }
    Widget->AddToViewport();
}

void ARPGDemoHeroController::ShowResultScreen(bool bVictory)
{
    if (!IsLocalController() || !GetWorld()) return;
    UWidgetLayoutLibrary::RemoveAllWidgets(this);
    if (PlayerCameraManager)
    {
        PlayerCameraManager->StartCameraFade(0.f, 1.f, 2.f, FLinearColor::Black, false, true);
    }
    GetWorldTimerManager().ClearTimer(ResultPresentationTimer);
    GetWorldTimerManager().SetTimer(ResultPresentationTimer,
        FTimerDelegate::CreateWeakLambda(this, [this, bVictory]()
        {
            const TCHAR* Path = bVictory
                ? TEXT("/Game/Widgets/GameModeWidgets/WBP_WinScreen.WBP_WinScreen_C")
                : TEXT("/Game/Widgets/GameModeWidgets/WBP_LoseScreen.WBP_LoseScreen_C");
            const TSoftClassPtr<UUserWidget> WidgetClass{FSoftObjectPath(Path)};
            if (UClass* LoadedClass = WidgetClass.LoadSynchronous())
            {
                if (UUserWidget* Widget = CreateWidget<UUserWidget>(this, LoadedClass)) Widget->AddToViewport();
            }
        }), 2.f, false);
}

void ARPGDemoHeroController::EnsureSurvivalMusic()
{
    if (!IsLocalController() || SurvivalMusicComponent)
    {
        return;
    }

    const TSoftObjectPtr<USoundBase> MusicAsset(FSoftObjectPath(
        TEXT("/Game/Assets/Sounds/Music/Dungeons_EndBossBattle.Dungeons_EndBossBattle")));
    if (USoundBase* Music = MusicAsset.LoadSynchronous())
    {
        SurvivalMusicComponent = UGameplayStatics::SpawnSound2D(
            this, Music, 0.5f, 1.f, 0.f, nullptr, false, false);
    }
}

void ARPGDemoHeroController::RestoreGameplayInput()
{
    if (!IsLocalController()) return;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
    bShowMouseCursor = false;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    FlushPressedKeys();
    if (APawn* ControlledPawn = GetPawn()) ControlledPawn->EnableInput(this);
}

FGenericTeamId ARPGDemoHeroController::GetGenericTeamId() const { return HeroTeamId; }
