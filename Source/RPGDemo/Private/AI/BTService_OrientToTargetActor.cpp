// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTService_OrientToTargetActor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"

UBTService_OrientToTargetActor::UBTService_OrientToTargetActor()
{
	NodeName = TEXT("Native Orient Rotation to Target Actor");

	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	RotationInterpSpeed = 5.f;
	Interval = 0.f;
	RandomDeviation = 0.f;

	InTargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, InTargetActorKey), AActor::StaticClass());
}

void UBTService_OrientToTargetActor::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		InTargetActorKey.ResolveSelectedKey(*BBAsset);
	}
}

FString UBTService_OrientToTargetActor::GetStaticDescription() const
{
	const FString KeyDescription = InTargetActorKey.SelectedKeyName.ToString();

	return FString::Printf(TEXT("Orient rotation to %s key %s"), *KeyDescription, *GetStaticServiceDescription());
}

void UBTService_OrientToTargetActor::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	if (!OwnerComp.GetBlackboardComponent() || !OwnerComp.GetAIOwner()) return;
	UObject* ActorObject = OwnerComp.GetBlackboardComponent()->GetValueAsObject(InTargetActorKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(ActorObject);

	APawn* OwningPawn = OwnerComp.GetAIOwner()->GetPawn();

	if (IsValid(OwningPawn) && IsValid(TargetActor))
	{
		const FVector Direction = TargetActor->GetActorLocation() - OwningPawn->GetActorLocation();
		if (Direction.IsNearlyZero()) return;
		// Use the same upright yaw contract as RotateToFaceTarget. This service
		// runs during attacks too, so pitch must not tilt socket damage boxes.
		const FRotator LookAtRot(0.f, Direction.Rotation().Yaw, 0.f);
		const FRotator CurrentRot(0.f, OwningPawn->GetActorRotation().Yaw, 0.f);
		const FRotator TargetRot = FMath::RInterpTo(CurrentRot, LookAtRot, DeltaSeconds, RotationInterpSpeed);

		OwningPawn->SetActorRotation(TargetRot);
	}
}
