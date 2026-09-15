// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RPGDemoBaseCharacter.h"
#include "RPGDemoHeroCharacter.generated.h"

class UCameraComponent;
class UDataAsset_InputConfig;
class UHeroCombatComponent;
class UHeroUIComponent;
class USpringArmComponent;
struct FInputActionValue;

UCLASS()
class RPGDEMO_API ARPGDemoHeroCharacter : public ARPGDemoBaseCharacter
{
    GENERATED_BODY()
    friend class FRPGDemoNetworkRegressionCommand;

public:
    ARPGDemoHeroCharacter();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UPawnCombatComponent* GetPawnCombatComponent() const override;
    virtual UPawnUIComponent* GetPawnUIComponent() const override;
    virtual UHeroUIComponent* GetHeroUIComponent() const override;
    FORCEINLINE UHeroCombatComponent* GetHeroCombatComponent() const { return HeroCombatComponent; }

    UFUNCTION(BlueprintPure, Category = "RPGDemo|Movement")
    FVector GetNetworkMovementInputDirection() const;

protected:
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void BeginPlay() override;

private:
    void InitializeAbilitySystemFromPlayerState();
    void GrantPersistentHeroStartUpData();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UHeroCombatComponent> HeroCombatComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UHeroUIComponent> HeroUIComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterData", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDataAsset_InputConfig> InputConfigDataAsset;
    UPROPERTY() FVector2D SwitchDirection = FVector2D::ZeroVector;

    void Input_Move(const FInputActionValue& InputActionValue);
    void Input_Look(const FInputActionValue& InputActionValue);
    void Input_SwitchTargetTriggered(const FInputActionValue& InputActionValue);
    void Input_SwitchTargetCompleted(const FInputActionValue& InputActionValue);
    void Input_PickUpStoneStarted(const FInputActionValue& InputActionValue);
    void TryConsumeNearbyStones();
    UFUNCTION(Server, Reliable) void ServerTryConsumeNearbyStones();
    void Input_AbilityInputPressed(FGameplayTag InInputTag);
    void Input_AbilityInputReleased(FGameplayTag InInputTag);
    UFUNCTION(Server, Unreliable) void ServerUpdateMovementInputDirection(FVector_NetQuantizeNormal NewDirection);

    UPROPERTY(Replicated) FVector_NetQuantizeNormal ReplicatedMovementInputDirection = FVector::ForwardVector;
    UPROPERTY(EditDefaultsOnly, Category = "Pick Up Interaction") float StonePickUpRequestRadius = 175.f;
};
