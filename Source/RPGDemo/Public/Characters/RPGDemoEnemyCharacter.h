// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/RPGDemoBaseCharacter.h"
#include "RPGDemoEnemyCharacter.generated.h"

class UEnemyCombatComponent;
class UEnemyUIComponent;
class UWidgetComponent;
class UBoxComponent;
class UAnimMontage;
class UNiagaraSystem;
class UGameplayAbility;
class UCurveFloat;
class URPGDemoWidgetBase;
struct FStreamableHandle;


/**
 *
 */
USTRUCT()
struct FRPGDemoEnemyDeathPresentation
{
	GENERATED_BODY()


	UPROPERTY()
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> DissolveSystem = nullptr;

	UPROPERTY()
	bool bStarted = false;

	UPROPERTY()
	bool bDissolving = false;
    UPROPERTY() double DissolveStartServerTime = 0.0;
    UPROPERTY() float DissolveDuration = 2.f;
};

UCLASS()
class RPGDEMO_API ARPGDemoEnemyCharacter : public ARPGDemoBaseCharacter
{
	GENERATED_BODY()
	friend class FRPGDemoNetworkRegressionCommand;

public:
	ARPGDemoEnemyCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Resolves the granted death ability on the server and replicates one deterministic presentation. */
	void BeginReplicatedDeathPresentation();

	/** One terminal path for natural completion, cancellation and interruption. */
	UFUNCTION(BlueprintCallable, Category = "RPGDemo|Death")
	void FinishEnemyDeathPresentation();

	//~ Begin PawnCombatInterface interface
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface interface

	//~ Begin PawnUIInterface interface
	virtual UPawnUIComponent* GetPawnUIComponent() const override;
	virtual UEnemyUIComponent* GetEnemyUIComponent() const override;
	//~ End PawnUIInterface interface

protected:
    UPROPERTY(EditDefaultsOnly, Category = "UI") TSubclassOf<URPGDemoWidgetBase> BossHealthWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category = "UI") FText BossDisplayName;
    virtual void OnDeathPoseHeld() override;
    UPROPERTY(EditDefaultsOnly, Category = "CharacterData|Death") TObjectPtr<UNiagaraSystem> DeathDissolveSystem;
    UPROPERTY(EditDefaultsOnly, Category = "CharacterData|Death", meta = (ClampMin = "0.01")) float DeathDissolveDuration = 2.f;
    UPROPERTY(EditDefaultsOnly, Category = "CharacterData|Death") TSubclassOf<UGameplayAbility> DeathDropAbility;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~ Begin APawn interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn interface

#if WITH_EDITOR
	//~ Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UEnemyCombatComponent* EnemyCombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UBoxComponent* LeftHandCollisionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName LeftHandCollisionBoxAttachBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UBoxComponent* RightHandCollisionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName RightHandCollisionBoxAttachBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UEnemyUIComponent* EnemyUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* EnemyHealthWidgetComponent;

	UFUNCTION()
	virtual void OnBodyCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


public:
	FORCEINLINE UEnemyCombatComponent* GetEnemyCombatComponent() const { return EnemyCombatComponent; }
	FORCEINLINE UBoxComponent* GetLeftHandCollisionBox() const { return LeftHandCollisionBox; }
	FORCEINLINE UBoxComponent* GetRightHandCollisionBox() const { return RightHandCollisionBox; }

private:
	void InitEnemyStartUpData();
	void FinalizeReplicatedDeathPresentation();
	void ApplyReplicatedDeathPresentation();
	void BeginReplicatedDissolvePresentation();
	void ResolveDeathPresentationAssets(UAnimMontage*& OutMontage, UNiagaraSystem*& OutDissolveSystem) const;
    void UpdateDissolvePresentation();
    void CompleteDeathCleanup();
    void SpawnDeathDrop();
    bool bDeathDropRequested = false;
    FTimerHandle DeathCleanupTimer;
    FTimerHandle DissolveUpdateTimer;
    TWeakObjectPtr<UCurveFloat> DissolveCurve;

	UFUNCTION()
	void OnRep_DeathPresentation();

	UPROPERTY(ReplicatedUsing = OnRep_DeathPresentation)
	FRPGDemoEnemyDeathPresentation ReplicatedDeathPresentation;

	FTimerHandle DeathPresentationTimerHandle;
	bool bDeathPresentationRequested = false;
	bool bLocalDeathPresentationApplied = false;
	bool bLocalDissolvePresentationApplied = false;

	TSharedPtr<FStreamableHandle> EnemyStartUpDataHandle;
	bool bEnemyStartUpDataLoading = false;
	bool bEnemyStartUpDataInitialized = false;



};
