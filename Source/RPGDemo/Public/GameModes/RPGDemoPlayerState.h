// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RPGDemoPlayerState.generated.h"

UCLASS()
class RPGDEMO_API ARPGDemoPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ARPGDemoPlayerState();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "RPGDemo|Player")
	bool bIsReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsLobbyHost, BlueprintReadOnly, Category = "RPGDemo|Player")
	bool bIsLobbyHost = false;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsLobbyHost();
};
