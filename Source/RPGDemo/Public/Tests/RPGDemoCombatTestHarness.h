#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPGDemoCombatTestHarness.generated.h"

class ARPGDemoEnemyCharacter;
class ARPGDemoHeroCharacter;

// Created only by the explicit Development test command-line flag. Uses real
// abilities, montage notifies, collision and replication, never simulated hits.
UCLASS()
class RPGDEMO_API ARPGDemoCombatTestHarness : public AActor
{
    GENERATED_BODY()
public:
    ARPGDemoCombatTestHarness();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    void TickServer();
    void TickClient();
    void BeginCase();
    void FinishCase(bool bPassed, const TCHAR* Reason);
    void DestroyFixture();
    ARPGDemoHeroCharacter* FindHero(int32 PlayerRole) const;
    void PrepareHero(ARPGDemoHeroCharacter* Hero, FVector Location, bool bRestoreHealth = true);

    UPROPERTY(Replicated) int32 CaseIndex = -1;
    UPROPERTY(Replicated) TObjectPtr<ARPGDemoEnemyCharacter> Fixture;
    UPROPERTY() TObjectPtr<UClass> AttackClass;
    TWeakObjectPtr<ARPGDemoHeroCharacter> OldPawn;
    FVector Origin = FVector::ZeroVector;
    float HealthBefore = 0.f;
    double CaseStarted = 0.0;
    double NextCaseAt = 0.0;
    double NextWaitingAuditAt = 0.0;
    bool bActivated = false;
    bool bFixtureReady = false;
    bool bSinglePlayer = false;
    bool bDisconnectOnly = false;
    bool bAbruptDisconnect = false;
    bool bCaseFinished = false;
    int32 Failures = 0;
    int32 LastClientCase = -1;
    double ClientCaseStarted = 0.0;
    bool bClientEquipped = false;
    bool bClientAttacked = false;
    bool bClientSawFlash = false;
    bool bClientSawShake = false;
    bool bClientSawHitStop = false;
    bool bClientSawHitReact = false;
    bool bClientCheckedPose = false;
    bool bClientSawFlashEnd = false;
    bool bClientSawMovement = false;
    double PoseSampleAt = 0.0;
    float PoseSampleMontagePosition = 0.f;
    FTransform PoseSampleTransform;
    FVector ClientStartLocation;
    bool bClientRequestedDisconnect = false;
};

UCLASS()
class RPGDEMO_API URPGDemoCombatTestSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
};
