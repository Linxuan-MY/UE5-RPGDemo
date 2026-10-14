// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/RPGDemoPlayerState.h"

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoGameInstance.h"
#include "RPGDemoGameplayTags.h"

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

void ARPGDemoPlayerState::ResetForNewMatch()
{
    if (!HasAuthority()) return;
    AbilitySystemComponent->CancelAllAbilities();
    AbilitySystemComponent->ClearAllAbilities();
    AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery());
    FGameplayTagContainer OwnedTags;
    AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
    for (const FGameplayTag& Tag : OwnedTags)
    {
        AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::None);
        AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagAndCountToAll);
    }
    AbilitySystemComponent->SetLooseGameplayTagCount(RPGDemoGameplayTags::Shared_Status_Dead, 0, EGameplayTagReplicationState::TagAndCountToAll);
    bHeroStartUpDataGranted = false;
    SetIsOnlyASpectator(false);
    SetIsSpectator(false);
    SetLifeState(ERPGDemoPlayerLifeState::Alive);
}

void ARPGDemoPlayerState::OnRep_PlayerName()
{
    Super::OnRep_PlayerName();
    if (URPGDemoGameInstance* GI = GetGameInstance<URPGDemoGameInstance>()) GI->RefreshLobbyWaitingPlayers();
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
