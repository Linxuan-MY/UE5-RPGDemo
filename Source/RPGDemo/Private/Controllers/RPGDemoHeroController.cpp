// Fill out your copyright notice in the Description page of Project Settings.

#include "Controllers/RPGDemoHeroController.h"
#include "Components/Combat/HeroCombatComponent.h"

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
#include "Characters/RPGDemoHeroCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Components/UI/HeroUIComponent.h"
#include "Widgets/RPGDemoWidgetBase.h"

ARPGDemoHeroController::ARPGDemoHeroController()
{
    HeroTeamId = FGenericTeamId(0);
}

void ARPGDemoHeroController::BeginPlay()
{
    Super::BeginPlay();
    RestoreGameplayInput();
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { BindReplicatedPresentation(); }));
    if (IsLocalController()) GetWorldTimerManager().SetTimer(PresentationReadinessTimer, this,
        &ThisClass::UpdatePresentationReadiness, 0.1f, true);
}

void ARPGDemoHeroController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(PresentationReadinessTimer);
    GetWorldTimerManager().ClearTimer(WaveMessageTimer);
    GetWorldTimerManager().ClearTimer(ResultPresentationTimer);
    if (ARPGDemoGameState* State = GetWorld()->GetGameState<ARPGDemoGameState>()) State->OnSurvivalSnapshotChanged.RemoveAll(this);
    if (ARPGDemoPlayerState* State = GetPlayerState<ARPGDemoPlayerState>()) State->OnLifeStateChanged.RemoveAll(this);
    if (HeroHUD) HeroHUD->RemoveFromParent();
    if (WaveMessage) WaveMessage->RemoveFromParent();
    if (RespawnMessage) RespawnMessage->RemoveFromParent();
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
    EnsureHeroHUD();
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

void ARPGDemoHeroController::UpdatePresentationReadiness()
{
    BindReplicatedPresentation();
    UpdateCountdowns();
}

void ARPGDemoHeroController::EnsureHeroHUD()
{
    if (!IsLocalController() || bResultPresented) return;
    ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(GetPawn());
    URPGDemoAbilitySystemComponent* ASC = Hero ? Hero->GetRPGDemoAbilitySystemComponent() : nullptr;
    if (!Hero || !ASC || ASC->GetAvatarActor() != Hero || !HeroHUDClass) return;
    Hero->GetHeroCombatComponent()->RefreshEquippedWeaponPresentation();
    if (HUDPawn.Get() != Hero || !HeroHUD)
    {
        if (HeroHUD) HeroHUD->RemoveFromParent();
        HeroHUD = CreateWidget<URPGDemoWidgetBase>(this, HeroHUDClass);
        HUDPawn = Hero;
        if (HeroHUD)
        {
            HeroHUD->AddToPlayerScreen();
            HeroHUD->InitHeroCreatedWidget(Hero);
            Hero->GetHeroUIComponent()->RefreshPresentation();
            Hero->RefreshStoneInteraction();
        }
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
        RespawnMessageEndTime = RespawnEndServerTime;
        CreateCountdown(NSLOCTEXT("RPGDemoSurvival", "RespawnCountdown", "Respawning In"), RespawnEndServerTime, true);
    }
    else if (RespawnMessage) { RespawnMessage->RemoveFromParent(); RespawnMessage = nullptr; }
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
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    WaveMessageEndTime = (State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + DurationSeconds;
    CreateCountdown(Message, WaveMessageEndTime, false);
}

UUserWidget* ARPGDemoHeroController::CreateCountdown(const FText& Message, double EndServerTime, bool bRespawn)
{
    if (!IsLocalController() || bResultPresented) return nullptr;
    TObjectPtr<UUserWidget>& Slot = bRespawn ? RespawnMessage : WaveMessage;
    if (Slot) Slot->RemoveFromParent();
    if (!bRespawn) GetWorldTimerManager().ClearTimer(WaveMessageTimer);
    UUserWidget* Widget = CountdownWidgetClass ? CreateWidget<UUserWidget>(this, CountdownWidgetClass) : nullptr;
    Slot = Widget;
    if (!Widget) return nullptr;
    if (UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("TextBlock_WaveText"))))
    {
        Text->SetText(Message);
    }
    Widget->AddToPlayerScreen(bRespawn ? 20 : 10);
    if (bRespawn)
    {
        // Position/size setters reset viewport anchors. Apply anchors last.
        Widget->SetPositionInViewport(FVector2D::ZeroVector);
        Widget->SetAnchorsInViewport(FAnchors(0.5f, 0.85f));
        Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
    }
    UpdateCountdowns();
    return Widget;
}

void ARPGDemoHeroController::UpdateCountdowns()
{
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!State) return;
    auto Update = [State](TObjectPtr<UUserWidget>& Widget, double EndTime)
    {
        if (!Widget || EndTime <= 0.0) return;
        const double Remaining = FMath::Max(0.0, EndTime - State->GetServerWorldTimeSeconds());
        if (UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("TextBlock_CountDownText"))))
            Text->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
        if (Remaining <= 0.0) { Widget->RemoveFromParent(); Widget = nullptr; }
    };
    Update(WaveMessage, WaveMessageEndTime);
    Update(RespawnMessage, RespawnMessageEndTime);
}

void ARPGDemoHeroController::ShowTransientMessage(const FText& Message)
{
    if (!IsLocalController()) return;
    if (bResultPresented) return;
    if (WaveMessage) WaveMessage->RemoveFromParent();
    WaveMessageEndTime = 0.0;
    GetWorldTimerManager().ClearTimer(WaveMessageTimer);
    UUserWidget* Widget = WaveMessageWidgetClass ? CreateWidget<UUserWidget>(this, WaveMessageWidgetClass) : nullptr;
    WaveMessage = Widget;
    if (!Widget) return;
    if (UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("TextBlock_WaveText"))))
    {
        Text->SetText(Message);
    }
    Widget->AddToPlayerScreen(10);
    GetWorldTimerManager().SetTimer(WaveMessageTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if (WaveMessage) { WaveMessage->RemoveFromParent(); WaveMessage = nullptr; }
    }), 3.f, false);
}

void ARPGDemoHeroController::ShowResultScreen(bool bVictory)
{
    if (!IsLocalController() || !GetWorld()) return;
    if (bResultPresented) return;
    bResultPresented = true;
    GetWorldTimerManager().ClearTimer(WaveMessageTimer);
    HeroHUD = nullptr;
    WaveMessage = nullptr;
    RespawnMessage = nullptr;
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
