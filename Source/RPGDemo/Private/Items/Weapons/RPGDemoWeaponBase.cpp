// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "RPGDemoFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

#include "RPGDemoDebugHelper.h"


ARPGDemoWeaponBase::ARPGDemoWeaponBase()
{
	bReplicates = true;

 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	WeaponCollisionBox->SetupAttachment(GetRootComponent());
	WeaponCollisionBox->SetBoxExtent(FVector(20.f));
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnCollisionBoxBeginOverlap);
	WeaponCollisionBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnCollisionBoxEndOverlap);
}

void ARPGDemoWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	TryRegisterWithOwningPawn();
}

void ARPGDemoWeaponBase::OnRep_Owner()
{
	Super::OnRep_Owner();
	TryRegisterWithOwningPawn();
}

void ARPGDemoWeaponBase::OnRep_WeaponRegistrationData()
{
	TryRegisterWithOwningPawn();
}

void ARPGDemoWeaponBase::TryRegisterWithOwningPawn()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!OwningPawn || !ReplicatedWeaponTag.IsValid())
	{
		return;
	}

	if (UPawnCombatComponent* CombatComponent = OwningPawn->FindComponentByClass<UPawnCombatComponent>())
	{
		CombatComponent->RegisterSpawnedWeapon(ReplicatedWeaponTag, this, bReplicatedAsEquippedWeapon);
	}
}

void ARPGDemoWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoWeaponBase, ReplicatedWeaponTag);
	DOREPLIFETIME(ARPGDemoWeaponBase, bReplicatedAsEquippedWeapon);
}

void ARPGDemoWeaponBase::SetWeaponRegistrationData(FGameplayTag InWeaponTag, bool bInRegisterAsEquippedWeapon)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedWeaponTag = InWeaponTag;
	bReplicatedAsEquippedWeapon = bInRegisterAsEquippedWeapon;
	ForceNetUpdate();
}

void ARPGDemoWeaponBase::OnCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* WeaponOwningPawn = GetInstigator<APawn>();

	checkf(WeaponOwningPawn, TEXT("Weapon %s does not have a valid instigator pawn!"), *GetName());

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (URPGDemoFunctionLibrary::TargetPawnHostile(WeaponOwningPawn, HitPawn))
		{
			OnWeaponHitTarget.ExecuteIfBound(OtherActor);
		}
	}
}

void ARPGDemoWeaponBase::OnCollisionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* WeaponOwningPawn = GetInstigator<APawn>();

	checkf(WeaponOwningPawn, TEXT("Weapon %s does not have a valid instigator pawn!"), *GetName());

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (URPGDemoFunctionLibrary::TargetPawnHostile(WeaponOwningPawn, HitPawn))
		{
			OnWeaponPulledFromTarget.ExecuteIfBound(OtherActor);
		}
	}
}



