// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoPlayerState.h"

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoGameInstance.h"

ARPGDemoPlayerState::ARPGDemoPlayerState()
{
    bReplicates = true;
    SetNetUpdateFrequency(100.f);
    AbilitySystemComponent = CreateDefaultSubobject<URPGDemoAbilitySystemComponent>(TEXT("RPGDemoAbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    AttributeSet = CreateDefaultSubobject<URPGDemoAttributeSet>(TEXT("RPGDemoAttributeSet"));
}

UAbilitySystemComponent* ARPGDemoPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ARPGDemoPlayerState::SetLobbyHost(bool bNewHost)
{
    if (!HasAuthority() || bIsLobbyHost == bNewHost) return;
    bIsLobbyHost = bNewHost;
    OnRep_IsLobbyHost();
    ForceNetUpdate();
}

void ARPGDemoPlayerState::SetLifeState(ERPGDemoPlayerLifeState NewState, double NewRespawnEndServerTime)
{
    if (!HasAuthority()) return;
    LifeState = NewState;
    RespawnEndServerTime = NewRespawnEndServerTime;
    OnRep_LifeState();
    ForceNetUpdate();
}

bool ARPGDemoPlayerState::TryMarkHeroStartUpDataGranted()
{
    if (!HasAuthority() || bHeroStartUpDataGranted) return false;
    bHeroStartUpDataGranted = true;
    return true;
}

void ARPGDemoPlayerState::OnRep_IsLobbyHost()
{
    if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
}

void ARPGDemoPlayerState::OnRep_LifeState()
{
    OnLifeStateChanged.Broadcast(LifeState, RespawnEndServerTime);
}

void ARPGDemoPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARPGDemoPlayerState, bIsReady);
    DOREPLIFETIME(ARPGDemoPlayerState, bIsLobbyHost);
    DOREPLIFETIME(ARPGDemoPlayerState, LifeState);
    DOREPLIFETIME(ARPGDemoPlayerState, RespawnEndServerTime);
}
