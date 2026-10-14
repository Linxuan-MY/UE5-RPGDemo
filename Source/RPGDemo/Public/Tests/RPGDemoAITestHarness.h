#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPGDemoAITestHarness.generated.h"

class ARPGDemoEnemyCharacter;
class ARPGDemoHeroCharacter;

// Disposable packaged regression, created only with -RPGDemoAITest.
UCLASS()
class RPGDEMO_API ARPGDemoAITestHarness : public AActor
{
    GENERATED_BODY()
public:
    ARPGDemoAITestHarness();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    ARPGDemoHeroCharacter* FindHero(int32 Role) const;
    void PlaceHero(int32 Role, const FVector& Offset);
    void SpawnEnemy(int32 Kind, bool bRunBrain);
    void TickServer();
    void ObserveCorpse(int32 Role);
    void DamageToDeath(AActor* Actor);
    void Check(bool bPassed, const TCHAR* Case);

    UPROPERTY(Replicated) TObjectPtr<ARPGDemoEnemyCharacter> Fixture;
    UPROPERTY(Replicated) int32 Phase = -1;
    UPROPERTY(Replicated) int32 EnemyKind = 0;
    UPROPERTY(Replicated) FVector Hero1Location;
    UPROPERTY(Replicated) FVector Hero2Location;
    UPROPERTY(Replicated) bool bPlacementReady = false;
    TWeakObjectPtr<AActor> Occluder;
    TWeakObjectPtr<ARPGDemoHeroCharacter> LostHero;
    FVector Origin;
    double Started = 0.0, PhaseStarted = 0.0;
    int32 Failures = 0, LastKind = -1;
    bool bSawSightLoss = false, bObservedFrozen = false, bCheckedFrozen = false;
    bool bRequestedDisconnect = false;
    double FrozenStarted = 0.0;
    FVector FrozenLocation, DeathStartLocation;
    FQuat FrozenRotation;
    TArray<FTransform> FrozenPose;
    TArray<FTransform> DeathFirstPose;
    bool bSawDeathMontage = false;
    double DeathPoseChange = 0.0;
    double MaxPoseError = 0.0, MaxPositionError = 0.0, MaxAngleError = 0.0;
};
