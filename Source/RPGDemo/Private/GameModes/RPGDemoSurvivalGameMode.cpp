// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/RPGDemoSurvivalGameMode.h"
#include "Engine/AssetManager.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TargetPoint.h"
#include "NavigationSystem.h"
#include "RPGDemoFunctionLibrary.h"
#include "GameModes/RPGDemoGameState.h"

void ARPGDemoSurvivalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (UGameplayStatics::HasOption(Options, TEXT("RPGDemoMultiplayer")))
	{
		const FString DifficultyOption = UGameplayStatics::ParseOption(Options, TEXT("RPGDemoDifficulty"));
		if (DifficultyOption == TEXT("Easy")) CurrentGameDifficulty = ERPGDemoGameDifficulty::Easy;
		else if (DifficultyOption == TEXT("Normal")) CurrentGameDifficulty = ERPGDemoGameDifficulty::Normal;
		else if (DifficultyOption == TEXT("Hard")) CurrentGameDifficulty = ERPGDemoGameDifficulty::Hard;
		else if (DifficultyOption == TEXT("ExtremelyHard")) CurrentGameDifficulty = ERPGDemoGameDifficulty::ExtremelyHard;
		else
		{
			CurrentGameDifficulty = ERPGDemoGameDifficulty::Normal;
			UE_LOG(LogTemp, Warning, TEXT("Invalid multiplayer difficulty '%s'; falling back to Normal."), *DifficultyOption);
		}
	}
	else
	{
		ERPGDemoGameDifficulty SavedGameDifficulty;
		if (URPGDemoFunctionLibrary::TryLoadSavedGameDifficulty(SavedGameDifficulty)) CurrentGameDifficulty = SavedGameDifficulty;
	}
}

void ARPGDemoSurvivalGameMode::BeginPlay()
{
	Super::BeginPlay();

	checkf(EnemyWaveSpawnerDataTable, TEXT("Forgot to assign a valid data table"));

	SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::WaitSpawnNewWave);

	TotalWavesToSpawn = EnemyWaveSpawnerDataTable->GetRowNames().Num();
	if (ARPGDemoGameState* RPGDemoGameState = GetGameState<ARPGDemoGameState>())
	{
		RPGDemoGameState->SetWaveProgress(CurrentWaveCount, TotalWavesToSpawn);
	}

	PreLoadNextWaveEnemies();
}

void ARPGDemoSurvivalGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::WaitSpawnNewWave)
	{
		TimePassedSinceStart += DeltaTime;

		if (TimePassedSinceStart >= SpawnNewWaveWaitTime)
		{
			TimePassedSinceStart = 0.f;

			SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::SpawningNewWave);
		}
	}

	if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::SpawningNewWave)
	{
		TimePassedSinceStart += DeltaTime;

		if (TimePassedSinceStart >= SpawnEnemiesDelayTime)
		{
			CurrentSpawnedEnemiesCounter += TrySpawnWaveEnemies();

			TimePassedSinceStart = 0.f;

			SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::InProgress);
		}
	}

	if (CurrentSurvivalGameModeState == ERPGDemoSurvivalGameModeState::WaveCompleted)
	{
		TimePassedSinceStart += DeltaTime;

		if (TimePassedSinceStart >= WaveCompletedWaitTime)
		{
			TimePassedSinceStart = 0.f;

			CurrentWaveCount++;

			if (ARPGDemoGameState* RPGDemoGameState = GetGameState<ARPGDemoGameState>())
			{
				RPGDemoGameState->SetWaveProgress(CurrentWaveCount, TotalWavesToSpawn);
			}

			if (HasFinishedAllWaves())
			{
				SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::AllWavesDone);
			}
			else
			{
				SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::WaitSpawnNewWave);
				PreLoadNextWaveEnemies();
			}
		}
	}

}

void ARPGDemoSurvivalGameMode::SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState NewState)
{
	CurrentSurvivalGameModeState = NewState;

	OnSurvivalGameModeStateChanged.Broadcast(CurrentSurvivalGameModeState);

	if (ARPGDemoGameState* RPGDemoGameState = GetGameState<ARPGDemoGameState>())
	{
		RPGDemoGameState->SetSurvivalState(CurrentSurvivalGameModeState);
	}
}

bool ARPGDemoSurvivalGameMode::HasFinishedAllWaves() const
{
	return CurrentWaveCount > TotalWavesToSpawn;
}

void ARPGDemoSurvivalGameMode::PreLoadNextWaveEnemies()
{
	if (HasFinishedAllWaves())
	{
		return;
	}

	PreloadedEnemyClassMap.Empty();

	for (const FRPGDemoEnemyWaveSpawnerInfo& SpawnerInfo : GetCurrentWaveSpawnerTableRow()->EnemyWaveSpawnerDefinitions)
	{
		if (SpawnerInfo.SoftEnemyClassToSpawn.IsNull())
		{
			continue;
		}

		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			SpawnerInfo.SoftEnemyClassToSpawn.ToSoftObjectPath(),
			FStreamableDelegate::CreateLambda(
				[SpawnerInfo, this]()
				{
					if (UClass* LoadedEnemyClass = SpawnerInfo.SoftEnemyClassToSpawn.Get())
					{
						PreloadedEnemyClassMap.Emplace(SpawnerInfo.SoftEnemyClassToSpawn, LoadedEnemyClass);
					}
				}
			)
		);
	}
}

FRPGDemoEnemyWaveSpawnerTableRow* ARPGDemoSurvivalGameMode::GetCurrentWaveSpawnerTableRow() const
{
	const FName RowName = FName(TEXT("Wave") + FString::FromInt(CurrentWaveCount));

	FRPGDemoEnemyWaveSpawnerTableRow* FoundRow =  EnemyWaveSpawnerDataTable->FindRow<FRPGDemoEnemyWaveSpawnerTableRow>(RowName, FString());

	checkf(FoundRow, TEXT("Could not find a valid row under the name %s in the data table"), *RowName.ToString());

	return FoundRow;
}

int32 ARPGDemoSurvivalGameMode::TrySpawnWaveEnemies()
{
	if (TargetPointsArray.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(this, ATargetPoint::StaticClass(), TargetPointsArray);
	}

	checkf(!TargetPointsArray.IsEmpty(), TEXT("No valid target points found in level %s"), *GetWorld()->GetName());

	uint32 EnemiesSpawnedThisTime = 0;

	FActorSpawnParameters SpawnParam;
	SpawnParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (const FRPGDemoEnemyWaveSpawnerInfo& SpawnerInfo : GetCurrentWaveSpawnerTableRow()->EnemyWaveSpawnerDefinitions)
	{
		if (SpawnerInfo.SoftEnemyClassToSpawn.IsNull())
		{
			continue;
		}

		const int32 NumToSpawn = FMath::RandRange(SpawnerInfo.MinPerSpawnCount, SpawnerInfo.MaxPerSpawnCount);

		UClass* LoadedEnemyClass = PreloadedEnemyClassMap.FindChecked(SpawnerInfo.SoftEnemyClassToSpawn);

		for (int32 i = 0; i < NumToSpawn; i++)
		{
			const int32 RandomTargetPointIndex = FMath::RandRange(0, TargetPointsArray.Num() - 1);
			const FVector SpawnOrigin = TargetPointsArray[RandomTargetPointIndex]->GetActorLocation();
			const FRotator SpawnRotation = TargetPointsArray[RandomTargetPointIndex]->GetActorForwardVector().ToOrientationRotator();

			FVector RandomLocation;
			UNavigationSystemV1::K2_GetRandomLocationInNavigableRadius(this, SpawnOrigin, RandomLocation, 400.f);

			RandomLocation += FVector(0.f, 0.f, 150.f); // Offset to avoid spawning inside the ground

			ARPGDemoEnemyCharacter* SpawnedEnemy = GetWorld()->SpawnActor<ARPGDemoEnemyCharacter>(LoadedEnemyClass, RandomLocation, SpawnRotation, SpawnParam);

			if (SpawnedEnemy)
			{
				SpawnedEnemy->OnDestroyed.AddUniqueDynamic(this, &ARPGDemoSurvivalGameMode::OnEnemyDestroyed);

				EnemiesSpawnedThisTime++;
				TotalSpawnedEnemiesThisWaveCounter++;
			}

			if (!ShouldKeepSpawnEnemies())
			{
				return EnemiesSpawnedThisTime;
			}
		}
	}

	return EnemiesSpawnedThisTime;
}

bool ARPGDemoSurvivalGameMode::ShouldKeepSpawnEnemies() const
{
	return TotalSpawnedEnemiesThisWaveCounter < GetCurrentWaveSpawnerTableRow()->TotalEnemyToSpawnThisWave;
}

void ARPGDemoSurvivalGameMode::OnEnemyDestroyed(AActor* DestroyedActor)
{
	CurrentSpawnedEnemiesCounter--;

	if (ShouldKeepSpawnEnemies())
	{
		CurrentSpawnedEnemiesCounter += TrySpawnWaveEnemies();
	}
	else if (CurrentSpawnedEnemiesCounter == 0)
	{
		TotalSpawnedEnemiesThisWaveCounter = 0;
		CurrentSpawnedEnemiesCounter = 0;

		SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::WaveCompleted);
	}
}

void ARPGDemoSurvivalGameMode::RegisterSpawnedEnemies(const TArray<ARPGDemoEnemyCharacter*>& InEnemiesToRegister)
{
	for (ARPGDemoEnemyCharacter* Enemy : InEnemiesToRegister)
	{
		if (Enemy)
		{
			CurrentSpawnedEnemiesCounter++;

			Enemy->OnDestroyed.AddUniqueDynamic(this, &ThisClass::OnEnemyDestroyed);
		}
	}
}

void ARPGDemoSurvivalGameMode::NotifyPlayerDied()
{
	if (HasAuthority() && CurrentSurvivalGameModeState != ERPGDemoSurvivalGameModeState::PlayerDied &&
		CurrentSurvivalGameModeState != ERPGDemoSurvivalGameModeState::AllWavesDone)
	{
		SetCurrentSurvivalGameModeState(ERPGDemoSurvivalGameModeState::PlayerDied);
	}
}
