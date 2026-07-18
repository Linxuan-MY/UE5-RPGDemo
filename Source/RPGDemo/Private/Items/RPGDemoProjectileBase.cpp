// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/RPGDemoProjectileBase.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"

ARPGDemoProjectileBase::ARPGDemoProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;

	ProjectileCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ProjectileCollisionBox"));
	SetRootComponent(ProjectileCollisionBox);
	ProjectileCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Block);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECollisionResponse::ECR_Block);
	ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECollisionResponse::ECR_Block);
	ProjectileCollisionBox->OnComponentHit.AddUniqueDynamic(this, &ThisClass::OnProjectileHit);
	ProjectileCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnProjectileBeginOverlap);

	ProjectileNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileNiagaraComponent"));
	ProjectileNiagaraComponent->SetupAttachment(GetRootComponent());

	ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
	ProjectileMovementComp->InitialSpeed = 700.f;
	ProjectileMovementComp->MaxSpeed = 900.f;
	ProjectileMovementComp->Velocity = FVector(1.f, 0.f, 0.f);
	ProjectileMovementComp->ProjectileGravityScale = 0.f;

	InitialLifeSpan = 4.f;
}

void ARPGDemoProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	if (ProjectileDamagePolicy == EProjectileDamagePolicy::OnBeginOverlap)
	{
		ProjectileCollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
	}

}

void ARPGDemoProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	APawn* HitPawn = Cast<APawn>(OtherActor);

	if (!HitPawn || !CanDamageHitPawn(HitPawn))
	{
		BP_OnSpawnProjectileHitFX(Hit.ImpactPoint);
		Destroy();
		return;
	}

	HandleProjectileImpact(HitPawn, Hit.ImpactPoint);
}

void ARPGDemoProjectileBase::OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* HitPawn = Cast<APawn>(OtherActor);

	if (!HitPawn || !CanDamageHitPawn(HitPawn))
	{
		return;
	}

	FVector ImpactPoint = HitPawn->GetActorLocation();
	if (bFromSweep)
	{
		ImpactPoint = SweepResult.ImpactPoint;
	}

	HandleProjectileImpact(HitPawn, ImpactPoint);
}

bool ARPGDemoProjectileBase::CanDamageHitPawn(APawn* InHitPawn) const
{
	APawn* ProjectileOwningPawn = GetInstigator<APawn>();

	checkf(ProjectileOwningPawn, TEXT("Projectile %s does not have a valid instigator pawn!"), *GetName());

	return InHitPawn && URPGDemoFunctionLibrary::TargetPawnHostile(ProjectileOwningPawn, InHitPawn);
}

void ARPGDemoProjectileBase::HandleProjectileImpact(APawn* InHitPawn, const FVector& InImpactPoint)
{
	BP_OnSpawnProjectileHitFX(InImpactPoint);

	bool bIsValidBlock = false;
	const bool bIsPlayerBlocking = URPGDemoFunctionLibrary::NativeDoesActorHaveTag(InHitPawn, RPGDemoGameplayTags::Player_Status_Block);

	if (bIsPlayerBlocking)
	{
		bIsValidBlock = URPGDemoFunctionLibrary::IsValidBlock(this, InHitPawn);
	}

	FGameplayEventData Data;
	Data.Instigator = this;
	Data.Target = InHitPawn;

	if (bIsValidBlock)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			InHitPawn,
			RPGDemoGameplayTags::Player_Event_SuccessfulBlock,
			Data
			);
	}
	else
	{
		HandleApplyProjectileDamage(InHitPawn, Data);
	}

	Destroy();
}

void ARPGDemoProjectileBase::HandleApplyProjectileDamage(APawn* InHitPawn, const FGameplayEventData& InPayload)
{
	checkf(ProjectileDamageEffectSpecHandle.IsValid(), TEXT("Forgot to assign a valid spec handle to the projectile: %s"), *GetActorNameOrLabel());

	const bool bWasApplied = URPGDemoFunctionLibrary::ApplyGameplayEffectSpecHandleToTargetActor(GetInstigator(), InHitPawn, ProjectileDamageEffectSpecHandle);

	if (bWasApplied)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			InHitPawn,
			RPGDemoGameplayTags::Shared_Event_HitReact,
			InPayload
			);
	}
}
