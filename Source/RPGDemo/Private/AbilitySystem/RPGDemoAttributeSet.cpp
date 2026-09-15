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
        if (Avatar)
        {
            if (GetCurrentRage() >= GetMaxRage())
            {
                URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Avatar, RPGDemoGameplayTags::Player_Status_Rage_None);
                URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Avatar, RPGDemoGameplayTags::Player_Status_Rage_Full);
            }
            else if (GetCurrentRage() <= 0.f)
            {
                URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Avatar, RPGDemoGameplayTags::Player_Status_Rage_Full);
                URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Avatar, RPGDemoGameplayTags::Player_Status_Rage_None);
            }
            else
            {
                URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Avatar, RPGDemoGameplayTags::Player_Status_Rage_Full);
                URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Avatar, RPGDemoGameplayTags::Player_Status_Rage_None);
            }
        }
        BroadcastRageToUI();
    }
    else if (Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
    {
        SetCurrentHealth(FMath::Clamp(GetCurrentHealth() - GetDamageTaken(), 0.f, GetMaxHealth()));
        if (PawnUI) BroadcastHealthToUI();
        UAbilitySystemComponent* ASC = &Data.Target;
        if (GetCurrentHealth() <= 0.f && Avatar && ASC &&
            !ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead))
        {
            if (ARPGDemoEnemyCharacter* Enemy = Cast<ARPGDemoEnemyCharacter>(Avatar)) Enemy->BeginReplicatedDeathPresentation();
            if (ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(Avatar))
            {
                if (ARPGDemoSurvivalGameMode* Mode = Hero->GetWorld()->GetAuthGameMode<ARPGDemoSurvivalGameMode>())
                {
                    Mode->NotifyPlayerDied(Hero->GetController());
                }
            }
            URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Avatar, RPGDemoGameplayTags::Shared_Status_Dead);
        }
    }
}
