// Fill out your copyright notice in the Description page of Project Settings.


#include "Controllers/RPGDemoAIController.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "RPGDemoGameplayTags.h"
#include "TimerManager.h"

#include "RPGDemoDebugHelper.h"

ARPGDemoAIController::ARPGDemoAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>("PathFollowingComponent"))
{
	AISenseConfig_Sight = CreateDefaultSubobject<UAISenseConfig_Sight>("EnemySenseConfig_Sight");
	AISenseConfig_Sight->DetectionByAffiliation.bDetectEnemies = true;
	AISenseConfig_Sight->DetectionByAffiliation.bDetectFriendlies = false;
	AISenseConfig_Sight->DetectionByAffiliation.bDetectNeutrals = false;
	AISenseConfig_Sight->SightRadius = 5000.f;
	// Sight uses this radius after acquisition. A zero radius loses the target
	// on the next perception update and repeatedly restarts the chase task.
	AISenseConfig_Sight->LoseSightRadius = 5500.f;
	AISenseConfig_Sight->PeripheralVisionAngleDegrees = 360.f;

	EnemyPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("EnemyPerceptionComponent");
	EnemyPerceptionComponent->ConfigureSense(*AISenseConfig_Sight);
	EnemyPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
	EnemyPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::OnEnemyPerceptionUpdated);

	SetGenericTeamId(FGenericTeamId(1));
}

ETeamAttitude::Type ARPGDemoAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const APawn* PawnToCheck = Cast<const APawn>(&Other);
	if (!PawnToCheck)
	{
		return ETeamAttitude::Neutral;
	}

	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(PawnToCheck->GetController());

	if(OtherTeamAgent && OtherTeamAgent->GetGenericTeamId() < GetGenericTeamId())
	{
		return ETeamAttitude::Hostile;
	}

	return ETeamAttitude::Friendly;
}

void ARPGDemoAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		CrowdComp->SetCrowdSimulationState(bEnableDetourCrowdAvoidance ? ECrowdSimulationState::Enabled : ECrowdSimulationState::Disabled);

		switch (DetourCrowdAvoidanceQuality)
		{
			case 1:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
				break;
			case 2:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
				break;
			case 3:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);
				break;
			case 4:
				CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
				break;
			default:
				break;
		}

		CrowdComp->SetAvoidanceGroup(1);
		CrowdComp->SetGroupsToAvoid(1);
		CrowdComp->SetCrowdCollisionQueryRange(CollisionQueryRange);

	}

}

void ARPGDemoAIController::OnEnemyPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// Evaluate the entire current sight set, rather than letting callback order
	// decide which player wins. Sight loss observes the same lock as acquisition.
	RefreshTargetSelection();
}

void ARPGDemoAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	GetWorldTimerManager().SetTimer(TargetSelectionTimer, this, &ThisClass::RefreshTargetSelection, 0.2f, true);
}

void ARPGDemoAIController::OnUnPossess()
{
	StopTargetSelection();
	Super::OnUnPossess();
}

void ARPGDemoAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TargetSelectionTimer);
	Super::EndPlay(EndPlayReason);
}

void ARPGDemoAIController::StopTargetSelection()
{
	GetWorldTimerManager().ClearTimer(TargetSelectionTimer);
	TargetLockEndTime = 0.0;
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
		BlackboardComp->ClearValue(TEXT("TargetActor"));
	ClearFocus(EAIFocusPriority::Gameplay);
}

bool ARPGDemoAIController::IsLiveHostile(AActor* Actor) const
{
	const APawn* TargetPawn = Cast<APawn>(Actor);
	if (!IsValid(TargetPawn) || TargetPawn->IsActorBeingDestroyed() || !IsValid(TargetPawn->GetController()) ||
		GetTeamAttitudeTowards(*TargetPawn) != ETeamAttitude::Hostile) return false;
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	return !ASC || !ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead);
}

void ARPGDemoAIController::RefreshTargetSelection()
{
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	const UAbilitySystemComponent* PawnASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
	if (!GetPawn() || (PawnASC && PawnASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead)))
	{
		StopTargetSelection();
		return;
	}
	if (!BlackboardComponent || !EnemyPerceptionComponent)
	{
		return;
	}

	static const FName TargetActorKey(TEXT("TargetActor"));
	AActor* CurrentTarget = Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetActorKey));

	AActor* Candidate = FindNearestPerceivedHostile();
	if (!IsLiveHostile(CurrentTarget))
	{
		SetTargetActor(*BlackboardComponent, Candidate, TEXT("NoLiveTarget"));
		return;
	}
	if (IsTargetSwitchLocked()) return;

	TArray<AActor*> PerceivedActors;
	EnemyPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	if (!PerceivedActors.Contains(CurrentTarget))
	{
		SetTargetActor(*BlackboardComponent, Candidate, TEXT("SightLostAfterLock"));
		return;
	}
	if (!Candidate || Candidate == CurrentTarget) return;
	const float CurrentDistance = GetPawn()->GetDistanceTo(CurrentTarget);
	const float CandidateDistance = GetPawn()->GetDistanceTo(Candidate);
	if (CandidateDistance < CurrentDistance &&
		(CandidateDistance <= CurrentDistance * TargetSwitchDistanceRatio ||
		 CurrentDistance - CandidateDistance >= TargetSwitchMinDistanceAdvantage))
		SetTargetActor(*BlackboardComponent, Candidate, TEXT("DistanceAdvantageAfterLock"));
}

AActor* ARPGDemoAIController::FindNearestPerceivedHostile(const AActor* ExcludedActor) const
{
	if (!EnemyPerceptionComponent || !GetPawn())
	{
		return nullptr;
	}

	TArray<AActor*> PerceivedActors;
	EnemyPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (AActor* Candidate : PerceivedActors)
	{
		if (!IsLiveHostile(Candidate) || Candidate == ExcludedActor)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}

	return NearestTarget;
}

void ARPGDemoAIController::SetTargetActor(UBlackboardComponent& BlackboardComponent, AActor* NewTarget, const TCHAR* SwitchReason)
{
	static const FName TargetActorKey(TEXT("TargetActor"));
	AActor* PreviousTarget = Cast<AActor>(BlackboardComponent.GetValueAsObject(TargetActorKey));
	if (PreviousTarget == NewTarget)
	{
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	const float PreviousDistance = ControlledPawn && IsValid(PreviousTarget)
		? FVector::Dist(ControlledPawn->GetActorLocation(), PreviousTarget->GetActorLocation()) : -1.0f;
	const float NewDistance = ControlledPawn && IsValid(NewTarget)
		? FVector::Dist(ControlledPawn->GetActorLocation(), NewTarget->GetActorLocation()) : -1.0f;

	BlackboardComponent.SetValueAsObject(TargetActorKey, NewTarget);
	TargetLockEndTime = NewTarget && GetWorld()
		? GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, MinimumTargetLockDuration) : 0.0;

	UE_LOG(LogTemp, Log,
		TEXT("AI target changed. Controller=%s Pawn=%s Previous=%s New=%s Reason=%s PreviousDistance=%.1f NewDistance=%.1f LockDuration=%.2f"),
		*GetName(), *GetNameSafe(ControlledPawn), *GetNameSafe(PreviousTarget), *GetNameSafe(NewTarget), SwitchReason,
		PreviousDistance, NewDistance, NewTarget ? MinimumTargetLockDuration : 0.0f);
}

bool ARPGDemoAIController::IsTargetSwitchLocked() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < TargetLockEndTime;
}
