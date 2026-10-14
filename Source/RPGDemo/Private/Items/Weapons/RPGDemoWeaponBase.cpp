// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "RPGDemoFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

#include "RPGDemoDebugHelper.h"
#include "Combat/RPGDemoCombatLog.h"


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
    BindOwnerLifetime();
	TryRegisterWithOwningPawn();
}

void ARPGDemoWeaponBase::SetOwner(AActor* NewOwner)
{
    if (AActor* Previous = LifetimeOwner.Get()) Previous->OnDestroyed.RemoveDynamic(this, &ThisClass::OnLifetimeOwnerDestroyed);
    LifetimeOwner.Reset();
    Super::SetOwner(NewOwner);
    BindOwnerLifetime();
}

void ARPGDemoWeaponBase::BindOwnerLifetime()
{
    if (HasAuthority() && GetOwner())
    {
        LifetimeOwner = GetOwner();
        GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ThisClass::OnLifetimeOwnerDestroyed);
    }
}

void ARPGDemoWeaponBase::OnLifetimeOwnerDestroyed(AActor* DestroyedOwner)
{
    if (!HasAuthority()) return;
    OnWeaponHitTarget.Unbind();
    OnWeaponPulledFromTarget.Unbind();
    WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Destroy();
}

void ARPGDemoWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AActor* Previous = LifetimeOwner.Get()) Previous->OnDestroyed.RemoveDynamic(this, &ThisClass::OnLifetimeOwnerDestroyed);
    if (APawn* Pawn = Cast<APawn>(GetOwner()))
        if (UPawnCombatComponent* Combat = Pawn->FindComponentByClass<UPawnCombatComponent>()) Combat->UnregisterWeapon(this);
    OnWeaponHitTarget.Unbind();
    OnWeaponPulledFromTarget.Unbind();
    Super::EndPlay(EndPlayReason);
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

    if (!IsValid(WeaponOwningPawn)) return;

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (URPGDemoFunctionLibrary::TargetPawnHostile(WeaponOwningPawn, HitPawn))
		{
            if (RPGDemoCombat::IsDebugEnabled())
                UE_LOG(LogRPGDemoCombat, Log, TEXT("WeaponOverlap pawn=%s target=%s box=%s"), *WeaponOwningPawn->GetName(), *HitPawn->GetName(), *WeaponCollisionBox->GetComponentLocation().ToString());
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

    if (!IsValid(WeaponOwningPawn)) return;

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (URPGDemoFunctionLibrary::TargetPawnHostile(WeaponOwningPawn, HitPawn))
		{
			OnWeaponPulledFromTarget.ExecuteIfBound(OtherActor);
		}
	}
}



