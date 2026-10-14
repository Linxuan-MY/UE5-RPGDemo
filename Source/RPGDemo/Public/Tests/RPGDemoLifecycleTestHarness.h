#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPGDemoLifecycleTestHarness.generated.h"

class ARPGDemoHeroCharacter;
class ARPGDemoEnemyCharacter;
class ARPGDemoStoneBase;
class ARPGDemoPlayerState;
class APawn;

// Created only in an explicitly requested disposable Development test process.
UCLASS()
class RPGDEMO_API ARPGDemoLifecycleTestHarness : public AActor
{
    GENERATED_BODY()
public:
    ARPGDemoLifecycleTestHarness();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    static void InitializeTestWorld(UWorld& World);
private:
    ARPGDemoHeroCharacter* FindHero(int32 PlayerRole) const;
    ARPGDemoPlayerState* FindRegressionPlayerState(int32 PlayerRole) const;
    void TickServer();
    void TickClient();
    void TickRespawnRegressionServer();
    void TickRespawnRegressionClient();
    void Check(bool bPassed, const TCHAR* Case, int32 PlayerRole = 0);
    void SetPhase(int32 NewPhase);
    void PrepareCycle();
    void DamageToDeath(ARPGDemoHeroCharacter* Hero);
    UPROPERTY(Replicated) int32 Phase = 0;
    UPROPERTY(Replicated) int32 Cycle = 0;
    UPROPERTY(Replicated) int32 Match = 0;
    UPROPERTY(Replicated) double PhaseStartServerTime = 0.0;
    UPROPERTY(Replicated) FVector TestOrigin;
    UPROPERTY(Replicated) TObjectPtr<ARPGDemoEnemyCharacter> Fixture;
    UPROPERTY(Replicated) TObjectPtr<ARPGDemoHeroCharacter> Corpse;
    UPROPERTY(Replicated) TObjectPtr<ARPGDemoStoneBase> HealthStone;
    UPROPERTY(Replicated) TObjectPtr<ARPGDemoStoneBase> RageStone;
    double Started = 0.0;
    int32 LastClientPhase = -1, LastClientCycle = -1, Failures = 0;
    bool bClientEquipped = false, bClientLight = false, bClientHeavy = false, bClientSpecial = false;
    bool bClientHUDChecked = false, bClientPoseChecked = false, bClientPromptChecked = false, bClientPickupRequested = false;
    bool bClientStoneCleanupChecked = false, bClientSawDeathMontage = false, bServerSawConsumption = false;
    TArray<FTransform> HeldPose;
    double PoseSampleTime = 0.0;
    int32 ClientSpecialIndex = 0;
    FVector FixtureStart;
    bool bClientRequestedDisconnect = false;
    float LowestHeroHealth = 1000.f, LowestEnemyHealth = 10000.f;
    bool bDisconnectVictimDied = false;
    double NextCombatRetry = 12.0;
    double NextAIDiagnostic = 14.0;
    float MaxEnemyTilt = 0.f;
    int32 GroundedEnemySamples = 0;
    bool bClientSpecialPresentationChecked = false;
    double NextEquipAttempt = 0.0, NextLightAttempt = 0.0, NextHeavyAttempt = 0.0, NextSpecialAttempt = 0.0;
    TArray<TWeakObjectPtr<AActor>> SpawnBlockers;
    TSubclassOf<APawn> RegressionPawnClass;
};
