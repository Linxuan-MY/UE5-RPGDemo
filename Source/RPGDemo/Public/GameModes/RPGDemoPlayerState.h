// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RPGDemoPlayerState.generated.h"

class URPGDemoAbilitySystemComponent;
class URPGDemoAttributeSet;

UENUM(BlueprintType)
enum class ERPGDemoPlayerLifeState : uint8
{
    Alive,
    Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnRPGDemoPlayerLifeStateChanged,
    ERPGDemoPlayerLifeState, LifeState,
    double, RespawnEndServerTime);

UCLASS()
class RPGDEMO_API ARPGDemoPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ARPGDemoPlayerState();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category = "RPGDemo|AbilitySystem")
    URPGDemoAbilitySystemComponent* GetRPGDemoAbilitySystemComponent() const { return AbilitySystemComponent; }

    UFUNCTION(BlueprintPure, Category = "RPGDemo|AbilitySystem")
    URPGDemoAttributeSet* GetRPGDemoAttributeSet() const { return AttributeSet; }

    UPROPERTY(BlueprintAssignable, Category = "RPGDemo|Player")
    FOnRPGDemoPlayerLifeStateChanged OnLifeStateChanged;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "RPGDemo|Player")
    bool bIsReady = false;

    UPROPERTY(ReplicatedUsing = OnRep_IsLobbyHost, BlueprintReadOnly, Category = "RPGDemo|Player")
    bool bIsLobbyHost = false;

    UPROPERTY(ReplicatedUsing = OnRep_LifeState, BlueprintReadOnly, Category = "RPGDemo|Player")
    ERPGDemoPlayerLifeState LifeState = ERPGDemoPlayerLifeState::Alive;

    UPROPERTY(ReplicatedUsing = OnRep_LifeState, BlueprintReadOnly, Category = "RPGDemo|Player")
    double RespawnEndServerTime = 0.0;

    void SetLobbyHost(bool bNewHost);
    void SetLifeState(ERPGDemoPlayerLifeState NewState, double NewRespawnEndServerTime = 0.0);
    bool TryMarkHeroStartUpDataGranted();

    uint32 ServerJoinOrder = 0;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION() void OnRep_IsLobbyHost();
    UFUNCTION() void OnRep_LifeState();

private:
    UPROPERTY(VisibleAnywhere, Category = "RPGDemo|AbilitySystem")
    TObjectPtr<URPGDemoAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<URPGDemoAttributeSet> AttributeSet;

    bool bHeroStartUpDataGranted = false;
};
