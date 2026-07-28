// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "RPGDemoProjectileBase.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UProjectileMovementComponent;
struct FGameplayEventData;

UENUM( BlueprintType )
enum class EProjectileDamagePolicy : uint8
{
	OnHit,
	OnBeginOverlap
};

UCLASS()
class RPGDEMO_API ARPGDemoProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	ARPGDemoProjectileBase();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	UBoxComponent* ProjectileCollisionBox;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	UNiagaraComponent* ProjectileNiagaraComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* ProjectileMovementComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	EProjectileDamagePolicy ProjectileDamagePolicy = EProjectileDamagePolicy::OnHit;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile", meta = (ExposeOnSpawn = true))
	FGameplayEffectSpecHandle ProjectileDamageEffectSpecHandle;

	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	virtual void OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Spawn Projectile Hit FX"))
	void BP_OnSpawnProjectileHitFX(const FVector& HitLocation);

private:
	bool CanDamageHitPawn(APawn* InHitPawn) const;
	void HandleProjectileImpact(APawn* InHitPawn, const FVector& InImpactPoint);
	void HandleApplyProjectileDamage(APawn* InHitPawn, const FGameplayEventData& InPayload);

	TArray<AActor*> OverlappedActors;
};
