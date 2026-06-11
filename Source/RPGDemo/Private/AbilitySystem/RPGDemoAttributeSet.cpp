// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"

#include "RPGDemoDebugHelper.h"

URPGDemoAttributeSet::URPGDemoAttributeSet()
{
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitCurrentRage(1.f);
	InitMaxRage(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);

}

void URPGDemoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		const float NewCurrentHealth = FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);
	}

	if (Data.EvaluatedData.Attribute == GetCurrentRageAttribute())
	{
		const float NewCurrentRage = FMath::Clamp(GetCurrentRage(), 0.f, GetMaxRage());

		SetCurrentRage(NewCurrentRage);
	}

	if(Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
	{
		const float OldHealth = GetCurrentHealth();
		const float DamageDone = GetDamageTaken();

		const float NewCurrentHealth = FMath::Clamp(OldHealth - DamageDone, 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		const FString DebugString = FString::Printf(
			TEXT("Damage Taken: %f, Old Health: %f, New Health: %f"),
			DamageDone,
			OldHealth,
			NewCurrentHealth
		);

		Debug::Print(DebugString, FColor::Green);

		// TODO: Notify the UI

		if (NewCurrentHealth == 0.f)
		{
			URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(), RPGDemoGameplayTags::Shared_Status_Dead);
		}

	}

}
