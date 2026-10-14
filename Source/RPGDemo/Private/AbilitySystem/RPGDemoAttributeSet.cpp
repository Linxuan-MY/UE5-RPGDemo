// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/RPGDemoAttributeSet.h"

#include "Characters/RPGDemoEnemyCharacter.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Components/UI/HeroUIComponent.h"
#include "Components/UI/PawnUIComponent.h"
#include "GameModes/RPGDemoSurvivalGameMode.h"
#include "GameplayEffectExtension.h"
#include "Interfaces/PawnUIInterface.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "Combat/RPGDemoCombatLog.h"
#include "Components/Combat/PawnCombatComponent.h"

URPGDemoAttributeSet::URPGDemoAttributeSet()
{
    InitCurrentHealth(1.f); InitMaxHealth(1.f); InitCurrentRage(1.f); InitMaxRage(1.f);
    InitAttackPower(1.f); InitDefensePower(1.f);
}

void URPGDemoAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(URPGDemoAttributeSet, CurrentHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(URPGDemoAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(URPGDemoAttributeSet, CurrentRage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(URPGDemoAttributeSet, MaxRage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(URPGDemoAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(URPGDemoAttributeSet, DefensePower, COND_None, REPNOTIFY_Always);
}

void URPGDemoAttributeSet::OnRep_CurrentHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, CurrentHealth, OldValue); BroadcastHealthToUI(); }
void URPGDemoAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, MaxHealth, OldValue); BroadcastHealthToUI(); }
void URPGDemoAttributeSet::OnRep_CurrentRage(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, CurrentRage, OldValue); BroadcastRageToUI(); }
void URPGDemoAttributeSet::OnRep_MaxRage(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, MaxRage, OldValue); BroadcastRageToUI(); }
void URPGDemoAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, AttackPower, OldValue); }
void URPGDemoAttributeSet::OnRep_DefensePower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, DefensePower, OldValue); }

void URPGDemoAttributeSet::BroadcastHealthToUI() const
{
    const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    const IPawnUIInterface* UI = ASC ? Cast<IPawnUIInterface>(ASC->GetAvatarActor()) : nullptr;
    if (UPawnUIComponent* Component = UI ? UI->GetPawnUIComponent() : nullptr)
    {
        Component->OnCurrentHealthChanged.Broadcast(GetMaxHealth() > 0.f ? GetCurrentHealth() / GetMaxHealth() : 0.f);
    }
}

void URPGDemoAttributeSet::BroadcastRageToUI() const
{
    const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    const IPawnUIInterface* UI = ASC ? Cast<IPawnUIInterface>(ASC->GetAvatarActor()) : nullptr;
    if (UHeroUIComponent* Component = UI ? UI->GetHeroUIComponent() : nullptr)
    {
        Component->OnCurrentRageChanged.Broadcast(GetMaxRage() > 0.f ? GetCurrentRage() / GetMaxRage() : 0.f);
    }
}

void URPGDemoAttributeSet::SynchronizeRageStatus()
{
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (!ASC || !ASC->GetOwner() || !ASC->GetOwner()->HasAuthority()) return;
    ASC->SetLooseGameplayTagCount(RPGDemoGameplayTags::Player_Status_Rage_Full,
        GetMaxRage() > 0.f && GetCurrentRage() >= GetMaxRage() ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
    ASC->SetLooseGameplayTagCount(RPGDemoGameplayTags::Player_Status_Rage_None,
        GetCurrentRage() <= 0.f ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
    BroadcastRageToUI();
}

void URPGDemoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    AActor* Avatar = Data.Target.GetAvatarActor();
    IPawnUIInterface* UI = Cast<IPawnUIInterface>(Avatar);
    UPawnUIComponent* PawnUI = UI ? UI->GetPawnUIComponent() : nullptr;

    if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
    {
        SetCurrentHealth(FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth()));
        BroadcastHealthToUI();
    }
    else if (Data.EvaluatedData.Attribute == GetCurrentRageAttribute())
    {
        SetCurrentRage(FMath::Clamp(GetCurrentRage(), 0.f, GetMaxRage()));
        SynchronizeRageStatus();
        BroadcastRageToUI();
    }
    else if (Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
    {
        const float Damage = FMath::Max(0.f, GetDamageTaken());
        // DamageTaken is a per-effect meta attribute, including on a persistent PlayerState ASC.
        SetDamageTaken(0.f);
        const float HealthBefore = GetCurrentHealth();
        SetCurrentHealth(FMath::Clamp(HealthBefore - Damage, 0.f, GetMaxHealth()));
        AActor* Instigator = Data.EffectSpec.GetContext().GetOriginalInstigator();
        if (RPGDemoCombat::IsDebugEnabled())
            UE_LOG(LogRPGDemoCombat, Log, TEXT("Damage source=%s target=%s amount=%.3f healthBefore=%.3f healthAfter=%.3f"),
                *GetNameSafe(Instigator), *GetNameSafe(Avatar), Damage, HealthBefore, GetCurrentHealth());
        const bool bMeleeDamage = Data.EffectSpec.SetByCallerTagMagnitudes.Contains(RPGDemoGameplayTags::Player_SetByCaller_AttackType_Light)
            || Data.EffectSpec.SetByCallerTagMagnitudes.Contains(RPGDemoGameplayTags::Player_SetByCaller_AttackType_Heavy);
        if (Avatar && Avatar->HasAuthority() && HealthBefore > GetCurrentHealth() && bMeleeDamage)
            if (ARPGDemoHeroCharacter* Attacker = Cast<ARPGDemoHeroCharacter>(Instigator)) Attacker->NotifyConfirmedMeleeHit();
        if (PawnUI) BroadcastHealthToUI();
        UAbilitySystemComponent* ASC = &Data.Target;
        if (GetCurrentHealth() <= 0.f && Avatar && ASC &&
            !ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead))
        {
            if (UPawnCombatComponent* Combat = Avatar->FindComponentByClass<UPawnCombatComponent>()) Combat->ResetDamageCollision();
            if (ARPGDemoEnemyCharacter* Enemy = Cast<ARPGDemoEnemyCharacter>(Avatar)) Enemy->BeginReplicatedDeathPresentation();
            else if (ARPGDemoBaseCharacter* Character = Cast<ARPGDemoBaseCharacter>(Avatar)) Character->BeginCharacterDeath();
            URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Avatar, RPGDemoGameplayTags::Shared_Status_Dead);
            if (ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(Avatar))
            {
                if (ARPGDemoSurvivalGameMode* Mode = Hero->GetWorld()->GetAuthGameMode<ARPGDemoSurvivalGameMode>())
                {
                    Mode->NotifyPlayerDied(Hero->GetController());
                }
            }
        }
    }
}
