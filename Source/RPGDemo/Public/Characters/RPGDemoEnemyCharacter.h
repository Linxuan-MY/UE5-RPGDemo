// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/RPGDemoBaseCharacter.h"
#include "RPGDemoEnemyCharacter.generated.h"

class UEnemyCombatComponent;



/**
 * 
 */
UCLASS()
class RPGDEMO_API ARPGDemoEnemyCharacter : public ARPGDemoBaseCharacter
{
	GENERATED_BODY()

public:
	ARPGDemoEnemyCharacter();

	//~ Begin PawnCombatInterface interface
	virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
	//~ End PawnCombatInterface interface

protected:

	//~ Begin APawn interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UEnemyCombatComponent* EnemyCombatComponent;

public:
	FORCEINLINE UEnemyCombatComponent* GetEnemyCombatComponent() const { return EnemyCombatComponent; }

private:
	void InitEnemyStartUpData();


	
};
