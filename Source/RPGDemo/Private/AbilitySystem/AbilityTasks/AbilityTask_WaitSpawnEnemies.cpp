// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/AbilityTask_WaitSpawnEnemies.h"
#include "AbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "NavigationSystem.h"
#include "Characters/RPGDemoEnemyCharacter.h"

#include "RPGDemoDebugHelper.h"

UAbilityTask_WaitSpawnEnemies* UAbilityTask_WaitSpawnEnemies::WaitSpawnEnemies(UGameplayAbility* OwningAbility,
                                                                               FGameplayTag EventTag, TSoftClassPtr<ARPGDemoEnemyCharacter> SoftEnemyClassToSpawn, int32 NumToSpawn,
                                                                               const FVector& SpawnOrigin, float RandomSpawnRadius)
{
	UAbilityTask_WaitSpawnEnemies* Node = NewAbilityTask<UAbilityTask_WaitSpawnEnemies>(OwningAbility);

	Node->CachedEventTag = EventTag;
	Node->CachedSoftEnemyClassToSpawn = SoftEnemyClassToSpawn;
	Node->CachedNumToSpawn = NumToSpawn;
	Node->CachedSpawnOrigin = SpawnOrigin;
	Node->CachedRandomSpawnRadius = RandomSpawnRadius;

	return Node;
}

bool UAbilityTask_WaitSpawnEnemies::HasSpawnAuthority() const
{
	const AActor* Avatar = AbilitySystemComponent.IsValid() ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	return IsValid(Avatar) && Avatar->HasAuthority() && !Avatar->IsActorBeingDestroyed() &&
		Avatar->GetWorld() && !Avatar->GetWorld()->bIsTearingDown;
}

void UAbilityTask_WaitSpawnEnemies::FailAndEndTask()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		DidNotSpawn.Broadcast(TArray<ARPGDemoEnemyCharacter*>());
	}
	EndTask();
}

void UAbilityTask_WaitSpawnEnemies::Activate()
{
	if (!HasSpawnAuthority())
	{
		FailAndEndTask();
		return;
	}
	FGameplayEventMulticastDelegate& Delegate = AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(CachedEventTag);

	DelegateHandle = Delegate.AddUObject(this, &ThisClass::OnGameplayEventRecieved);
}

void UAbilityTask_WaitSpawnEnemies::OnDestroy(bool bInOwnerFinished)
{
	bTaskEnded = true;
	if (AbilitySystemComponent.IsValid())
	{
		if (FGameplayEventMulticastDelegate* Delegate = AbilitySystemComponent->GenericGameplayEventCallbacks.Find(CachedEventTag))
		{
			Delegate->Remove(DelegateHandle);
		}
	}
	if (SpawnClassHandle.IsValid())
	{
		SpawnClassHandle->CancelHandle();
		SpawnClassHandle.Reset();
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_WaitSpawnEnemies::OnGameplayEventRecieved(const FGameplayEventData* InPayload)
{
	if (bTaskEnded || bSpawnRequested)
	{
		return;
	}
	if (!HasSpawnAuthority())
	{
		FailAndEndTask();
		return;
	}
	bSpawnRequested = true;
	if (ensure(!CachedSoftEnemyClassToSpawn.IsNull()))
	{
		SpawnClassHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			CachedSoftEnemyClassToSpawn.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::OnEnemyClassLoaded)
		);
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			DidNotSpawn.Broadcast(TArray<ARPGDemoEnemyCharacter*>());
		}

		EndTask();
	}
}

void UAbilityTask_WaitSpawnEnemies::OnEnemyClassLoaded()
{
	if (bTaskEnded)
	{
		return;
	}
	if (!HasSpawnAuthority())
	{
		FailAndEndTask();
		return;
	}
	UClass* LoadedClass = CachedSoftEnemyClassToSpawn.Get();
	UWorld* World = GetWorld();

	if (!LoadedClass || !World)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			DidNotSpawn.Broadcast(TArray<ARPGDemoEnemyCharacter*>());
		}

		EndTask();
		return;
	}

	TArray<ARPGDemoEnemyCharacter*> SpawnedEnemies;

	FActorSpawnParameters SpawnParam;
	SpawnParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 i=0; i<CachedNumToSpawn; i++)
	{
		if (bTaskEnded || !HasSpawnAuthority())
		{
			break;
		}
		FVector RandomLocation = CachedSpawnOrigin;
		if (!UNavigationSystemV1::K2_GetRandomReachablePointInRadius(this, CachedSpawnOrigin, RandomLocation, CachedRandomSpawnRadius))
		{
			continue;
		}

		RandomLocation += FVector(0.f, 0.f, 150.f);

		const FRotator SpawnFacingRotation = AbilitySystemComponent->GetAvatarActor()->GetActorForwardVector().ToOrientationRotator();

		ARPGDemoEnemyCharacter* SpawnedEnemy = World->SpawnActor<ARPGDemoEnemyCharacter>(LoadedClass, RandomLocation, SpawnFacingRotation, SpawnParam);

		if (SpawnedEnemy)
		{
			SpawnedEnemies.Add(SpawnedEnemy);
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (!SpawnedEnemies.IsEmpty())
		{
			OnSpawnFinished.Broadcast(SpawnedEnemies);
		}
		else
		{
			DidNotSpawn.Broadcast(TArray<ARPGDemoEnemyCharacter*>());
		}
	}

	EndTask();
}
