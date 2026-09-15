// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "RPGDemoAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class RPGDEMO_API URPGDemoAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    URPGDemoAttributeSet();
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Health") FGameplayAttributeData CurrentHealth;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, CurrentHealth)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health") FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, MaxHealth)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentRage, Category = "Rage") FGameplayAttributeData CurrentRage;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, CurrentRage)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxRage, Category = "Rage") FGameplayAttributeData MaxRage;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, MaxRage)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Damage") FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, AttackPower)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DefensePower, Category = "Damage") FGameplayAttributeData DefensePower;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, DefensePower)
    UPROPERTY(BlueprintReadOnly, Category = "Damage") FGameplayAttributeData DamageTaken;
    ATTRIBUTE_ACCESSORS(URPGDemoAttributeSet, DamageTaken)

private:
    void BroadcastHealthToUI() const;
    void BroadcastRageToUI() const;
    UFUNCTION() void OnRep_CurrentHealth(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_CurrentRage(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxRage(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_DefensePower(const FGameplayAttributeData& OldValue);
};
