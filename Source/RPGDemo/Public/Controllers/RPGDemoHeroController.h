// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GameModes/RPGDemoGameState.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "RPGDemoHeroController.generated.h"

class UAudioComponent;
class URPGDemoWidgetBase;
class UUserWidget;

UCLASS()
class RPGDEMO_API ARPGDemoHeroController : public APlayerController, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    ARPGDemoHeroController();
    virtual void AcknowledgePossession(APawn* PossessedPawn) override;
    virtual FGenericTeamId GetGenericTeamId() const override;
    UFUNCTION(BlueprintCallable, Category = "RPGDemo|UI") void EnsureHeroHUD();

    UFUNCTION(BlueprintPure, Category = "RPGDemo|Survival")
    float GetLocalRespawnSecondsRemaining() const;

    UFUNCTION(BlueprintPure, Category = "RPGDemo|Survival")
    float GetSnapshotSecondsRemaining(const FRPGDemoSurvivalSnapshot& Snapshot) const;

    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Survival")
    void ShowCountdownMessage(const FText& Message, float DurationSeconds);

    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Survival")
    void ShowTransientMessage(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "RPGDemo|Survival")
    void ShowResultScreen(bool bVictory);

    UFUNCTION(BlueprintImplementableEvent, Category = "RPGDemo|Survival", meta = (DisplayName = "On Survival Snapshot Changed"))
    void BP_OnSurvivalSnapshotChanged(const FRPGDemoSurvivalSnapshot& Snapshot);

    UFUNCTION(BlueprintImplementableEvent, Category = "RPGDemo|Survival", meta = (DisplayName = "On Local Life State Changed"))
    void BP_OnLocalLifeStateChanged(ERPGDemoPlayerLifeState LifeState, double RespawnEndServerTime);

protected:
    virtual void BeginPlay() override;
    virtual void OnRep_PlayerState() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditDefaultsOnly, Category = "RPGDemo|UI") TSubclassOf<URPGDemoWidgetBase> HeroHUDClass;
    UPROPERTY(EditDefaultsOnly, Category = "RPGDemo|UI") TSubclassOf<UUserWidget> CountdownWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category = "RPGDemo|UI") TSubclassOf<UUserWidget> WaveMessageWidgetClass;

private:
    void RestoreGameplayInput();
    void BindReplicatedPresentation();
    void EnsureSurvivalMusic();
    void UpdatePresentationReadiness();
    void UpdateCountdowns();
    UUserWidget* CreateCountdown(const FText& Message, double EndServerTime, bool bRespawn);
    UFUNCTION() void HandleSurvivalSnapshotChanged(const FRPGDemoSurvivalSnapshot& Snapshot);
    UFUNCTION() void HandleLocalLifeStateChanged(ERPGDemoPlayerLifeState LifeState, double RespawnEndServerTime);
    FGenericTeamId HeroTeamId;
    FTimerHandle ResultPresentationTimer;
    FTimerHandle PresentationReadinessTimer;
    FTimerHandle WaveMessageTimer;
    UPROPERTY(Transient) TObjectPtr<URPGDemoWidgetBase> HeroHUD;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> WaveMessage;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> RespawnMessage;
    TWeakObjectPtr<APawn> HUDPawn;
    double WaveMessageEndTime = 0.0;
    double RespawnMessageEndTime = 0.0;
    bool bResultPresented = false;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> SurvivalMusicComponent;
    int32 LastPresentedSnapshotRevision = INDEX_NONE;
    bool bHasPresentedLifeState = false;
    ERPGDemoPlayerLifeState LastPresentedLifeState = ERPGDemoPlayerLifeState::Alive;
    double LastPresentedRespawnEndServerTime = 0.0;
};
