// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/PawnCombatInterface.h"
#include "Interfaces/PawnUIInterface.h"
#include "RPGDemoBaseCharacter.generated.h"

class URPGDemoAbilitySystemComponent;
class URPGDemoAttributeSet;
class UDataAsset_StartUpDataBase;
class UMotionWarpingComponent;
class UAnimMontage;

USTRUCT()
struct FRPGDemoCharacterDeathSnapshot
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UAnimMontage> Montage;
    UPROPERTY() double StartServerTime = 0.0;
    UPROPERTY() float Duration = 0.f;
    UPROPERTY() bool bStarted = false;
};

UCLASS()
class RPGDEMO_API ARPGDemoBaseCharacter : public ACharacter, public IAbilitySystemInterface, public IPawnCombatInterface, public IPawnUIInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ARPGDemoBaseCharacter();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void BeginCharacterDeath();
    bool IsDeathPresentationActive() const { return DeathSnapshot.bStarted; }
    bool IsDeathPoseHeld() const { return bDeathPoseHeld; }
    const FRPGDemoCharacterDeathSnapshot& GetDeathSnapshot() const { return DeathSnapshot; }
    double GetPresentationServerTime() const;

	//~ Begin IAbilitySystemInterface interface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface interface

	//~ Begin PawnCombatInterface interface
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface interface

	//~ Begin PawnUIInterface interface
	virtual UPawnUIComponent* GetPawnUIComponent() const override;
	//~ End PawnUIInterface interface

protected:
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeathPoseHeld() {}
    UPROPERTY(EditDefaultsOnly, Category = "CharacterData|Death") TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	//~ Begin APawn interface
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	//~ End APawn interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	URPGDemoAbilitySystemComponent*	RPGDemoAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	URPGDemoAttributeSet* RPGDemoAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motionwarping")
	UMotionWarpingComponent* MotionWarpingComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterData")
	TSoftObjectPtr<UDataAsset_StartUpDataBase> CharacterStartUpData;
private:
    UFUNCTION() void OnRep_DeathSnapshot();
    void UpdateDeathPresentation();
    UAnimMontage* SelectDeathMontage() const;
    UPROPERTY(ReplicatedUsing = OnRep_DeathSnapshot) FRPGDemoCharacterDeathSnapshot DeathSnapshot;
    FTimerHandle DeathPoseTimer;
    bool bLocalDeathStarted = false;
    bool bDeathPoseHeld = false;

public:
	FORCEINLINE URPGDemoAbilitySystemComponent* GetRPGDemoAbilitySystemComponent() const { return RPGDemoAbilitySystemComponent; }
	
	FORCEINLINE URPGDemoAttributeSet* GetRPGDemoAttributeSet() const { return RPGDemoAttributeSet; }
};
