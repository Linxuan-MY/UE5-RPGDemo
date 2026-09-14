// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "Interfaces/PawnUIInterface.h"
#include "Components/UI/PawnUIComponent.h"
#include "Components/UI/HeroUIComponent.h"

#include "RPGDemoDebugHelper.h"
#include "Net/UnrealNetwork.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "GameModes/RPGDemoSurvivalGameMode.h"

URPGDemoAttributeSet::URPGDemoAttributeSet()
{
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitCurrentRage(1.f);
	InitMaxRage(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);

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

void URPGDemoAttributeSet::OnRep_CurrentHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, CurrentHealth, OldValue);
	BroadcastHealthToUI();
}

void URPGDemoAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, MaxHealth, OldValue);
	BroadcastHealthToUI();
}

void URPGDemoAttributeSet::OnRep_CurrentRage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, CurrentRage, OldValue);
	BroadcastRageToUI();
}

void URPGDemoAttributeSet::OnRep_MaxRage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, MaxRage, OldValue);
	BroadcastRageToUI();
}

void URPGDemoAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, AttackPower, OldValue);
}

void URPGDemoAttributeSet::OnRep_DefensePower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URPGDemoAttributeSet, DefensePower, OldValue);
}

void URPGDemoAttributeSet::BroadcastHealthToUI() const
{
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	IPawnUIInterface* PawnUIInterface = ASC ? Cast<IPawnUIInterface>(ASC->GetAvatarActor()) : nullptr;
	UPawnUIComponent* PawnUIComponent = PawnUIInterface ? PawnUIInterface->GetPawnUIComponent() : nullptr;
	if (PawnUIComponent)
	{
		PawnUIComponent->OnCurrentHealthChanged.Broadcast(
			GetMaxHealth() > 0.f ? GetCurrentHealth() / GetMaxHealth() : 0.f);
	}
}

void URPGDemoAttributeSet::BroadcastRageToUI() const
{
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	IPawnUIInterface* PawnUIInterface = ASC ? Cast<IPawnUIInterface>(ASC->GetAvatarActor()) : nullptr;
	UHeroUIComponent* HeroUIComponent = PawnUIInterface ? PawnUIInterface->GetHeroUIComponent() : nullptr;
	if (HeroUIComponent)
	{
		HeroUIComponent->OnCurrentRageChanged.Broadcast(
			GetMaxRage() > 0.f ? GetCurrentRage() / GetMaxRage() : 0.f);
	}
}

void URPGDemoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if(!CachedPawnUIInterface.IsValid())
	{
		CachedPawnUIInterface = TWeakInterfacePtr<IPawnUIInterface>(Data.Target.GetAvatarActor());
	}

	checkf(CachedPawnUIInterface.IsValid(), TEXT("%s didn't implement IPawnUIInterface"), *Data.Target.GetAvatarActor()->GetActorNameOrLabel());

	UPawnUIComponent* PawnUIComponent = CachedPawnUIInterface->GetPawnUIComponent();

	checkf(PawnUIComponent, TEXT("Couldn't get PawnUIComponent in %s"), *Data.Target.GetAvatarActor()->GetActorNameOrLabel());

	if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		const float NewCurrentHealth = FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		PawnUIComponent->OnCurrentHealthChanged.Broadcast(NewCurrentHealth / GetMaxHealth());
	}

	if (Data.EvaluatedData.Attribute == GetCurrentRageAttribute())
	{
		const float NewCurrentRage = FMath::Clamp(GetCurrentRage(), 0.f, GetMaxRage());

		SetCurrentRage(NewCurrentRage);

		if (GetCurrentRage() == GetMaxRage())
		{
			URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Player_Status_Rage_None);
			URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Player_Status_Rage_Full);
		}
		else if (GetCurrentRage() == 0.f)
		{
			URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Player_Status_Rage_Full);
			URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Player_Status_Rage_None);
		}
		else
		{
			URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Player_Status_Rage_Full);
			URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Player_Status_Rage_None);
		}

		if (UHeroUIComponent* HeroUIComponent = CachedPawnUIInterface->GetHeroUIComponent())
		{
			HeroUIComponent->OnCurrentRageChanged.Broadcast(NewCurrentRage / GetMaxRage());
		}
	}

	if(Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
	{
		const float OldHealth = GetCurrentHealth();
		const float DamageDone = GetDamageTaken();

		const float NewCurrentHealth = FMath::Clamp(OldHealth - DamageDone, 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		PawnUIComponent->OnCurrentHealthChanged.Broadcast(NewCurrentHealth / GetMaxHealth());

		/*
		const FString DebugString = FString::Printf(
			TEXT("Damage Taken: %f, Old Health: %f, New Health: %f"),
			DamageDone,
			OldHealth,
			NewCurrentHealth
		);


		Debug::Print(DebugString, FColor::Green);
		*/

		if (GetCurrentHealth() == 0.f)
		{
			if (ARPGDemoEnemyCharacter* Enemy = Cast<ARPGDemoEnemyCharacter>(Data.Target.GetAvatarActor()))
			{
				Enemy->BeginReplicatedDeathPresentation();
			}

			URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Shared_Status_Dead);

			if (Data.Target.GetAvatarActor()->IsA<ARPGDemoHeroCharacter>())
			{
				if (UWorld* World = Data.Target.GetAvatarActor()->GetWorld())
				{
					if (ARPGDemoSurvivalGameMode* SurvivalGameMode = World->GetAuthGameMode<ARPGDemoSurvivalGameMode>())
					{
						SurvivalGameMode->NotifyPlayerDied();
					}
				}
			}
		}

	}

}
