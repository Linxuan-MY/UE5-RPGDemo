// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGDemoFunctionLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Interfaces/PawnCombatInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "RPGDemoTypes/RPGDemoCountDownAction.h"

URPGDemoAbilitySystemComponent* URPGDemoFunctionLibrary::NativeGetRPGDemoASCFromActor(AActor* InActor)
{
	check(InActor);

	return CastChecked<URPGDemoAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor));
}

void URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	if(!ASC->HasMatchingGameplayTag(TagToAdd))
	{
		ASC->AddLooseGameplayTag(TagToAdd);
	}
}

void URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	if(ASC->HasMatchingGameplayTag(TagToRemove))
	{
		ASC->RemoveLooseGameplayTag(TagToRemove);
	}
}

bool URPGDemoFunctionLibrary::NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	return ASC->HasMatchingGameplayTag(TagToCheck);
}

void URPGDemoFunctionLibrary::BP_DoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck, ERPGDemoConfirmType& OutConfirmType)
{
	NativeDoesActorHaveTag(InActor, TagToCheck) ? OutConfirmType = ERPGDemoConfirmType::Yes : OutConfirmType = ERPGDemoConfirmType::No;
}

UPawnCombatComponent* URPGDemoFunctionLibrary::NativeGetPawnCombatComponentFromActor(AActor* InActor)
{
	check(InActor);

	if (IPawnCombatInterface* PawnCombatInterface = Cast<IPawnCombatInterface>(InActor))
	{
		return PawnCombatInterface->GetPawnCombatComponent();
	}

	return nullptr;
}

UPawnCombatComponent* URPGDemoFunctionLibrary::BP_GetPawnCombatComponentFromActor(AActor* InActor, ERPGDemoValidType& OutValidType)
{
	UPawnCombatComponent* CombatComponent = NativeGetPawnCombatComponentFromActor(InActor);

	OutValidType = CombatComponent ? ERPGDemoValidType::Valid : ERPGDemoValidType::Invalid;

	return CombatComponent;
}

bool URPGDemoFunctionLibrary::TargetPawnHostile(APawn* QueryPawn, APawn* TargetPawn)
{
	check(QueryPawn);
	check(TargetPawn);

	IGenericTeamAgentInterface* QueryTeamAgent = Cast<IGenericTeamAgentInterface>(QueryPawn->GetController());
	IGenericTeamAgentInterface* TargetTeamAgent = Cast<IGenericTeamAgentInterface>(TargetPawn->GetController());

	if (QueryTeamAgent && TargetTeamAgent)
	{
		return QueryTeamAgent->GetGenericTeamId() != TargetTeamAgent->GetGenericTeamId();
	}

	return false;
}

float URPGDemoFunctionLibrary::GetScalableFloatValueAtLevel(const FScalableFloat& InScalableFloat, float InLevel)
{
	return InScalableFloat.GetValueAtLevel(InLevel);
}

FGameplayTag URPGDemoFunctionLibrary::ComputeHitReactDirectionTag(AActor* InAttacker, AActor* InVictim,
	float& OutAngleDiff)
{
	check(InAttacker);
	check(InVictim);

	const FVector AttackerLocation = InAttacker->GetActorLocation();
	const FVector VictimLocation = InVictim->GetActorLocation();

	const FVector VictimForward = InVictim->GetActorForwardVector();
	const FVector VictimToAttackerNormalized = (AttackerLocation - VictimLocation).GetSafeNormal();

	const float DotResult = FVector::DotProduct(VictimForward, VictimToAttackerNormalized);
	OutAngleDiff = UKismetMathLibrary::DegAcos(DotResult);

	const FVector CrossResult = FVector::CrossProduct(VictimForward, VictimToAttackerNormalized);

	if (CrossResult.Z < 0.0f)
	{
		OutAngleDiff = -OutAngleDiff;
	}

	if (OutAngleDiff <= 45.0f && OutAngleDiff >= -45.0f)
	{
		return RPGDemoGameplayTags::Shared_Status_HitReact_Front;
	}
	else if (OutAngleDiff < -45.f && OutAngleDiff >= -135.f)
	{
		return RPGDemoGameplayTags::Shared_Status_HitReact_Left;
	}
	else if (OutAngleDiff > 135.f || OutAngleDiff < -135.f)
	{
		return RPGDemoGameplayTags::Shared_Status_HitReact_Back;
	}
	else if (OutAngleDiff > 45.f && OutAngleDiff <= 135.f)
	{
		return RPGDemoGameplayTags::Shared_Status_HitReact_Right;
	}

	return RPGDemoGameplayTags::Shared_Status_HitReact_Front;
}

bool URPGDemoFunctionLibrary::IsValidBlock(AActor* InAttacker, AActor* InVictim)
{
	check(InAttacker);
	check(InVictim);

	const float DotResult = FVector::DotProduct(InAttacker->GetActorForwardVector(), InVictim->GetActorForwardVector());

	return DotResult < -0.1f;
}

bool URPGDemoFunctionLibrary::ApplyGameplayEffectSpecHandleToTargetActor(AActor* InInstigator, AActor* InTargetActor,
	const FGameplayEffectSpecHandle& InSpecHandle)
{
	URPGDemoAbilitySystemComponent* SourceASC = NativeGetRPGDemoASCFromActor(InInstigator);
	URPGDemoAbilitySystemComponent* TargetASC = NativeGetRPGDemoASCFromActor(InTargetActor);

	FActiveGameplayEffectHandle ActiveGameplayEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*InSpecHandle.Data, TargetASC);

	return ActiveGameplayEffectHandle.WasSuccessfullyApplied();
}

void URPGDemoFunctionLibrary::CountDown(const UObject* WorldContextObject, float TotalTime, float UpdateInterval,
	float& OutRemainingTime, ERPGDemoCountDownActionInput CountDownInput,
	UPARAM(DisplayName = "Output")
	ERPGDemoCountDownActionOutput& CountDownOutput, FLatentActionInfo LatentInfo)
{
	UWorld* World = nullptr;
	if (GEngine)
	{
		World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	}

	if (!World)
	{
		return;
	}

	FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

	FRPGDemoCountDownAction* FoundAction = LatentActionManager.FindExistingAction<FRPGDemoCountDownAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);

	if (CountDownInput == ERPGDemoCountDownActionInput::Start)
	{
		if (!FoundAction)
		{
			LatentActionManager.AddNewAction(
				LatentInfo.CallbackTarget,
				LatentInfo.UUID,
				new FRPGDemoCountDownAction(TotalTime, UpdateInterval, OutRemainingTime, CountDownOutput, LatentInfo)
			);
		}
	}

	if (CountDownInput == ERPGDemoCountDownActionInput::Cancel)
	{
		if (FoundAction)
		{
			FoundAction->CancelAction();
		}
	}
}
