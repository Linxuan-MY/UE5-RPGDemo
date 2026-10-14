// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_RotateToFaceTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"

UBTTask_RotateToFaceTarget::UBTTask_RotateToFaceTarget()
{
	NodeName = TEXT("Rotate to Face Target Actor");
	AnglePrecision = 10.f;
	RotationInterpSpeed = 5.f;

	bNotifyTick = true;
	bNotifyTaskFinished = true;
	bCreateNodeInstance = false;

	INIT_TASK_NODE_NOTIFY_FLAGS();

	InTargetToFaceKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, InTargetToFaceKey), AActor::StaticClass());
}

void UBTTask_RotateToFaceTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InTargetToFaceKey.ResolveSelectedKey(*BBAsset);
	}
}

uint16 UBTTask_RotateToFaceTarget::GetInstanceMemorySize() const
{
	return sizeof(FRotateToFaceTargetTaskMemory);
}

FString UBTTask_RotateToFaceTarget::GetStaticDescription() const
{
	const FString KeyDescription = InTargetToFaceKey.SelectedKeyName.ToString();
	return FString::Printf(TEXT("Rotate to Face Target: %s Until the Angle Precision: %s"), *KeyDescription, *FString::SanitizeFloat(AnglePrecision));
}

EBTNodeResult::Type UBTTask_RotateToFaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (!OwnerComp.GetBlackboardComponent() || !OwnerComp.GetAIOwner()) return EBTNodeResult::Failed;
	UObject* ActorObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(InTargetToFaceKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(ActorObject);

	APawn* OwningPawn = OwnerComp.GetAIOwner()->GetPawn();

	FRotateToFaceTargetTaskMemory* Memory = CastInstanceNodeMemory<FRotateToFaceTargetTaskMemory>(NodeMemory);
	check(Memory);

	Memory->OwningPawn = OwningPawn;
	Memory->TargetActor = TargetActor;

	if (!Memory->IsValid())
	{
		return EBTNodeResult::Failed;
	}

	if (HasReachedAnglePrecision(OwningPawn, TargetActor))
	{
		Memory->Reset();
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_RotateToFaceTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FRotateToFaceTargetTaskMemory* Memory = CastInstanceNodeMemory<FRotateToFaceTargetTaskMemory>(NodeMemory);
	if (!Memory->IsValid())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
	}

	if (HasReachedAnglePrecision(Memory->OwningPawn.Get(), Memory->TargetActor.Get()))
	{
		Memory->Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
	else
	{
		// Ground characters turn around their upright capsule, so both interpolation
		// and completion must measure yaw. Movement can otherwise erase the pitch
		// that this task is waiting to reach, leaving it running indefinitely.
		const FVector Direction = Memory->TargetActor->GetActorLocation() - Memory->OwningPawn->GetActorLocation();
		const FRotator LookAtRot(0.f, Direction.Rotation().Yaw, 0.f);
		const FRotator CurrentRot(0.f, Memory->OwningPawn->GetActorRotation().Yaw, 0.f);
		const FRotator TargetRot = FMath::RInterpTo(CurrentRot, LookAtRot, DeltaSeconds, RotationInterpSpeed);

		Memory->OwningPawn->SetActorRotation(TargetRot);
	}
}

bool UBTTask_RotateToFaceTarget::HasReachedAnglePrecision(APawn* QueryPawn, AActor* TargetActor) const
{
	const FVector OwnerForward = QueryPawn->GetActorForwardVector().GetSafeNormal2D();
	const FVector OwnerToTargetNormalized = (TargetActor->GetActorLocation() - QueryPawn->GetActorLocation()).GetSafeNormal2D();
	if (OwnerToTargetNormalized.IsNearlyZero()) return true;

	const float DotResult = FMath::Clamp(FVector::DotProduct(OwnerForward, OwnerToTargetNormalized), -1.f, 1.f);
	const float AngleDiff = UKismetMathLibrary::DegAcos(DotResult);

	return AngleDiff <= AnglePrecision;
}
