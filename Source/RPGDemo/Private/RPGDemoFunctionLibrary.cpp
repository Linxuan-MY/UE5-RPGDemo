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
#include "RPGDemoGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/RPGDemoSaveGame.h"

URPGDemoAbilitySystemComponent* URPGDemoFunctionLibrary::NativeGetRPGDemoASCFromActor(AActor* InActor)
{
	check(InActor);

	return CastChecked<URPGDemoAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor));
}

void URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(AActor* InActor, FGameplayTag TagToAdd)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	// Attribute-owned state is never predicted; other Blueprint helper tags stay local.
	const bool bAuthoritativeState = TagToAdd == RPGDemoGameplayTags::Player_Status_Rage_Full ||
		TagToAdd == RPGDemoGameplayTags::Player_Status_Rage_None || TagToAdd == RPGDemoGameplayTags::Shared_Status_Dead;
	if (bAuthoritativeState && !InActor->HasAuthority())
	{
		return;
	}

	if(!ASC->HasMatchingGameplayTag(TagToAdd))
	{
		ASC->AddLooseGameplayTag(TagToAdd, 1, bAuthoritativeState ?
			EGameplayTagReplicationState::TagAndCountToAll : EGameplayTagReplicationState::None);
	}
}

void URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(AActor* InActor, FGameplayTag TagToRemove)
{
	URPGDemoAbilitySystemComponent* ASC = NativeGetRPGDemoASCFromActor(InActor);

	// Attribute-owned state is never predicted; other Blueprint helper tags stay local.
	const bool bAuthoritativeState = TagToRemove == RPGDemoGameplayTags::Player_Status_Rage_Full ||
		TagToRemove == RPGDemoGameplayTags::Player_Status_Rage_None || TagToRemove == RPGDemoGameplayTags::Shared_Status_Dead;
	if (bAuthoritativeState && !InActor->HasAuthority())
	{
		return;
	}

	if(ASC->HasMatchingGameplayTag(TagToRemove))
	{
		ASC->RemoveLooseGameplayTag(TagToRemove, 1, bAuthoritativeState ?
			EGameplayTagReplicationState::TagAndCountToAll : EGameplayTagReplicationState::None);
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

URPGDemoGameInstance* URPGDemoFunctionLibrary::GetRPGDemoGameInstance(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			return World->GetGameInstance<URPGDemoGameInstance>();
		}
	}

	return nullptr;
}

void URPGDemoFunctionLibrary::ToggleInputMode(const UObject* WorldContextObject, ERPGDemoInputMode InInputMode)
{
	APlayerController* PlayerController = nullptr;

	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			PlayerController = World->GetFirstPlayerController();
		}
	}

	if (!PlayerController)
	{
		return;
	}

	FInputModeGameOnly GameOnlyMode;
	FInputModeUIOnly UIOnlyMode;
	FInputModeGameAndUI GameAndUIMode;
	GameAndUIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	GameAndUIMode.SetHideCursorDuringCapture(false);

	const bool bNetworkedWorld = PlayerController->GetNetMode() != NM_Standalone;

	switch (InInputMode)
	{
		case ERPGDemoInputMode::GameOnly:
			PlayerController->SetInputMode(GameOnlyMode);
			PlayerController->bShowMouseCursor = false;
			PlayerController->ResetIgnoreMoveInput();
			PlayerController->ResetIgnoreLookInput();
			if (APawn* ControlledPawn = PlayerController->GetPawn())
			{
				ControlledPawn->EnableInput(PlayerController);
			}
			break;

		case ERPGDemoInputMode::UIOnly:
			if (bNetworkedWorld)
			{
				if (URPGDemoAbilitySystemComponent* ASC =
					Cast<URPGDemoAbilitySystemComponent>(
						UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerController->GetPawn())))
				{
					// Enhanced Input will not emit Completed after the pawn is disabled.
					// Release hold abilities (block, etc.) before consuming gameplay input.
					ASC->CancelInputHeldAbilities();
				}

				// Multiple PIE windows share Slate user 0. UIOnly gives the newest
				// pause menu exclusive routing and makes an already-open menu
				// unclickable. GameAndUI keeps both viewports interactive while the
				// controlled pawn is disabled locally.
				PlayerController->SetInputMode(GameAndUIMode);
				PlayerController->SetIgnoreMoveInput(true);
				PlayerController->SetIgnoreLookInput(true);
				if (APawn* ControlledPawn = PlayerController->GetPawn())
				{
					ControlledPawn->DisableInput(PlayerController);
				}
			}
			else
			{
				PlayerController->SetInputMode(UIOnlyMode);
			}
			PlayerController->bShowMouseCursor = true;
			break;

		default:
			break;
	}
}

void URPGDemoFunctionLibrary::SaveCurrentGameDifficulty(ERPGDemoGameDifficulty InGameDifficulty)
{
	USaveGame* SaveGameObject = UGameplayStatics::CreateSaveGameObject(URPGDemoSaveGame::StaticClass());

	if (URPGDemoSaveGame* RPGDemoSaveGameObject = Cast<URPGDemoSaveGame>(SaveGameObject))
	{
		RPGDemoSaveGameObject->SavedGameDifficulty = InGameDifficulty;
		const bool bWasSaved = UGameplayStatics::SaveGameToSlot(RPGDemoSaveGameObject, RPGDemoGameplayTags::GameData_SaveGame_Slot_1.GetTag().ToString(), 0);
	}
}

bool URPGDemoFunctionLibrary::TryLoadSavedGameDifficulty(ERPGDemoGameDifficulty& OutGameDifficulty)
{
	if (UGameplayStatics::DoesSaveGameExist(RPGDemoGameplayTags::GameData_SaveGame_Slot_1.GetTag().ToString(), 0))
	{
		USaveGame* LoadedSaveGame = UGameplayStatics::LoadGameFromSlot(RPGDemoGameplayTags::GameData_SaveGame_Slot_1.GetTag().ToString(), 0);

		if (URPGDemoSaveGame* RPGDemoSaveGame = Cast<URPGDemoSaveGame>(LoadedSaveGame))
		{
			OutGameDifficulty = RPGDemoSaveGame->SavedGameDifficulty;
			return true;
		}
	}
	return false;
}

FText URPGDemoFunctionLibrary::GetGameDifficultyDisplayText(ERPGDemoGameDifficulty GameDifficulty)
{
	switch (GameDifficulty)
	{
	case ERPGDemoGameDifficulty::Easy: return NSLOCTEXT("RPGDemoDifficulty", "Easy", "Easy");
	case ERPGDemoGameDifficulty::Normal: return NSLOCTEXT("RPGDemoDifficulty", "Normal", "Normal");
	case ERPGDemoGameDifficulty::Hard: return NSLOCTEXT("RPGDemoDifficulty", "Hard", "Hard");
	case ERPGDemoGameDifficulty::ExtremelyHard: return NSLOCTEXT("RPGDemoDifficulty", "ExtremelyHard", "Extremely Hard");
	default: return NSLOCTEXT("RPGDemoDifficulty", "Unknown", "Unknown");
	}
}
