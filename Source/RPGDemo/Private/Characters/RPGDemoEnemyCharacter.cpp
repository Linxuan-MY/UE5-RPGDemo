// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/RPGDemoEnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Components/Combat/EnemyCombatComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "DataAssets/StartUpData/DataAsset_EnemyStartUpData.h"
#include "Components/UI/EnemyUIComponent.h"
#include "Components/WidgetComponent.h"
#include "Widgets/RPGDemoWidgetBase.h"
#include "Components/BoxComponent.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameplayTags.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

#include "RPGDemoDebugHelper.h"

ARPGDemoEnemyCharacter::ARPGDemoEnemyCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;

	EnemyCombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("EnemyCombatComponent"));

	EnemyUIComponent = CreateDefaultSubobject<UEnemyUIComponent>(TEXT("EnemyUIComponent"));

	EnemyHealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHealthWidgetComponent"));
	EnemyHealthWidgetComponent->SetupAttachment(GetMesh());

	LeftHandCollisionBox = CreateDefaultSubobject<UBoxComponent>("LeftHandCollisionBox");
	LeftHandCollisionBox->SetupAttachment(GetMesh());
	LeftHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);

	RightHandCollisionBox = CreateDefaultSubobject<UBoxComponent>("RightHandCollisionBox");
	RightHandCollisionBox->SetupAttachment(GetMesh());
	RightHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandCollisionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBodyCollisionBoxBeginOverlap);
}

UPawnCombatComponent* ARPGDemoEnemyCharacter::GetPawnCombatComponent() const
{
	return EnemyCombatComponent;
}

UPawnUIComponent* ARPGDemoEnemyCharacter::GetPawnUIComponent() const
{
	return EnemyUIComponent;
}

UEnemyUIComponent* ARPGDemoEnemyCharacter::GetEnemyUIComponent() const
{
	return EnemyUIComponent;
}

void ARPGDemoEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (URPGDemoWidgetBase* HealthWidget = Cast<URPGDemoWidgetBase>(EnemyHealthWidgetComponent->GetUserWidgetObject()))
	{
		HealthWidget->InitEnemyCreatedWidget(this);
	}

	if (!HasAuthority())
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
		return;
	}

	// Spawned AI pawns may enter BeginPlay before AutoPossessAI calls PossessedBy.
	// Startup data must be requested from either path, but only once.
	if (!RPGDemoAbilitySystemComponent->GetAvatarActor())
	{
		RPGDemoAbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	InitEnemyStartUpData();

}

void ARPGDemoEnemyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority())
	{
		InitEnemyStartUpData();
	}
}

#if WITH_EDITOR
void ARPGDemoEnemyCharacter::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, LeftHandCollisionBoxAttachBoneName))
	{
		LeftHandCollisionBox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, LeftHandCollisionBoxAttachBoneName);
	}
	else if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, RightHandCollisionBoxAttachBoneName))
	{
		RightHandCollisionBox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RightHandCollisionBoxAttachBoneName);
	}
}
#endif

void ARPGDemoEnemyCharacter::OnBodyCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                            AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                                            const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (APawn* HitPawn = Cast<APawn>(OtherActor))
	{
		if (URPGDemoFunctionLibrary::TargetPawnHostile(this, HitPawn))
		{
			EnemyCombatComponent->OnHitTargetActor(HitPawn);
		}
	}
}

void ARPGDemoEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoEnemyCharacter, ReplicatedDeathPresentation);
}

void ARPGDemoEnemyCharacter::BeginReplicatedDeathPresentation()
{
	if (!HasAuthority() || bDeathPresentationRequested || ReplicatedDeathPresentation.bStarted)
	{
		return;
	}

	bDeathPresentationRequested = true;

	// Adding the death tag immediately after this call activates the existing death
	// ability synchronously. Resolve on the next tick so we replicate the montage that
	// the server actually selected instead of performing a second random selection.
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]() { FinalizeReplicatedDeathPresentation(); }));
}

void ARPGDemoEnemyCharacter::FinalizeReplicatedDeathPresentation()
{
	if (!HasAuthority() || ReplicatedDeathPresentation.bStarted)
	{
		return;
	}

	UAnimMontage* SelectedMontage = nullptr;
	UNiagaraSystem* DissolveSystem = nullptr;
	ResolveDeathPresentationAssets(SelectedMontage, DissolveSystem);

	ReplicatedDeathPresentation.DeathMontage = SelectedMontage;
	ReplicatedDeathPresentation.DissolveSystem = DissolveSystem;
	ReplicatedDeathPresentation.bStarted = true;
	ForceNetUpdate();
	ApplyReplicatedDeathPresentation();
}

void ARPGDemoEnemyCharacter::OnRep_DeathPresentation()
{
	ApplyReplicatedDeathPresentation();
}

void ARPGDemoEnemyCharacter::ApplyReplicatedDeathPresentation()
{
	if (!ReplicatedDeathPresentation.bStarted || bLocalDeathPresentationApplied)
	{
		return;
	}

	bLocalDeathPresentationApplied = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EnemyHealthWidgetComponent->SetVisibility(false, true);

	float PresentationDuration = 0.f;
	if (ReplicatedDeathPresentation.DeathMontage)
	{
		if (!HasAuthority())
		{
			PresentationDuration = PlayAnimMontage(ReplicatedDeathPresentation.DeathMontage);
		}
		else
		{
			PresentationDuration = ReplicatedDeathPresentation.DeathMontage->GetPlayLength();
		}
	}

	if (PresentationDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DeathPresentationTimerHandle,
			this,
			&ThisClass::BeginReplicatedDissolvePresentation,
			PresentationDuration,
			false);
	}
	else
	{
		BeginReplicatedDissolvePresentation();
	}
}

void ARPGDemoEnemyCharacter::BeginReplicatedDissolvePresentation()
{
	if (bLocalDissolvePresentationApplied)
	{
		return;
	}

	bLocalDissolvePresentationApplied = true;

	// The authority already executes the existing ability OnEnd path. Simulated proxies
	// need this explicit bridge because they do not own the AI's ability spec.
	if (HasAuthority())
	{
		return;
	}

	// Preserve the existing Blueprint material timeline and Niagara setup, but invoke it
	// from replicated state so simulated proxies do not need an owning ability spec.
	if (UFunction* EnemyDiedFunction = FindFunction(TEXT("OnEnemyDied")))
	{
		FProperty* Parameter = FindFProperty<FProperty>(EnemyDiedFunction, TEXT("DissolveNiagaraSystem"));
		if (!Parameter || !Parameter->HasAnyPropertyFlags(CPF_Parm) ||
			Parameter->HasAnyPropertyFlags(CPF_ReturnParm) || Parameter->ArrayDim != 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid OnEnemyDied parameter signature on %s"), *GetClass()->GetPathName());
			return;
		}

		// Blueprint uses a soft object reference, whose layout is larger than a UObject*.
		// Allocate and initialize the actual reflected function frame; never guess its layout.
		FStructOnScope Parameters(EnemyDiedFunction);
		if (FSoftObjectProperty* SoftParameter = CastField<FSoftObjectProperty>(Parameter))
		{
			SoftParameter->SetPropertyValue_InContainer(Parameters.GetStructMemory(),
				FSoftObjectPtr(ReplicatedDeathPresentation.DissolveSystem.Get()));
		}
		else if (FObjectProperty* ObjectParameter = CastField<FObjectProperty>(Parameter))
		{
			ObjectParameter->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(),
				ReplicatedDeathPresentation.DissolveSystem.Get());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Unsupported OnEnemyDied parameter type %s on %s"),
				*Parameter->GetCPPType(), *GetClass()->GetPathName());
			return;
		}
		ProcessEvent(EnemyDiedFunction, Parameters.GetStructMemory());
	}
}

void ARPGDemoEnemyCharacter::ResolveDeathPresentationAssets(
	UAnimMontage*& OutMontage,
	UNiagaraSystem*& OutDissolveSystem) const
{
	OutMontage = RPGDemoAbilitySystemComponent ? RPGDemoAbilitySystemComponent->GetCurrentMontage() : nullptr;
	OutDissolveSystem = nullptr;

	if (!RPGDemoAbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : RPGDemoAbilitySystemComponent->GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = AbilitySpec.Ability;
		if (!Ability || !Ability->GetAssetTags().HasTagExact(RPGDemoGameplayTags::Shared_Ability_Death))
		{
			continue;
		}

		TArray<UAnimMontage*> CandidateMontages;
		for (TFieldIterator<FArrayProperty> PropertyIt(Ability->GetClass()); PropertyIt; ++PropertyIt)
		{
			FArrayProperty* ArrayProperty = *PropertyIt;
			FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner);
			if (!InnerObjectProperty || !InnerObjectProperty->PropertyClass->IsChildOf(UAnimMontage::StaticClass()))
			{
				continue;
			}

			void* ArrayAddress = ArrayProperty->ContainerPtrToValuePtr<void>(const_cast<UGameplayAbility*>(Ability));
			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddress);
			for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			{
				if (UAnimMontage* Montage = Cast<UAnimMontage>(
					InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index))))
				{
					CandidateMontages.Add(Montage);
				}
			}
		}

		if (!OutMontage && !CandidateMontages.IsEmpty())
		{
			OutMontage = CandidateMontages[FMath::RandHelper(CandidateMontages.Num())];
		}

		for (TFieldIterator<FObjectPropertyBase> PropertyIt(Ability->GetClass()); PropertyIt; ++PropertyIt)
		{
			FObjectPropertyBase* ObjectProperty = *PropertyIt;
			if (ObjectProperty->PropertyClass->IsChildOf(UNiagaraSystem::StaticClass()) &&
				ObjectProperty->GetName().Contains(TEXT("Dissolve"), ESearchCase::IgnoreCase))
			{
				if (FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(ObjectProperty))
				{
					// Resolve on authority before replicating the hard reference; a cold soft asset's Get() is null.
					OutDissolveSystem = Cast<UNiagaraSystem>(SoftProperty->GetPropertyValue_InContainer(Ability).LoadSynchronous());
				}
				else
				{
					OutDissolveSystem = Cast<UNiagaraSystem>(ObjectProperty->GetObjectPropertyValue_InContainer(Ability));
				}
				break;
			}
		}

		break;
	}
}
void ARPGDemoEnemyCharacter::InitEnemyStartUpData()
{
	if (!HasAuthority() || !GetWorld() || IsActorBeingDestroyed() || bEnemyStartUpDataInitialized || bEnemyStartUpDataLoading || CharacterStartUpData.IsNull())
	{
		return;
	}

	bEnemyStartUpDataLoading = true;

	int32 AbilityApplyLevel = 1;

	if (ARPGDemoBaseGameMode* BaseGameMode = GetWorld()->GetAuthGameMode<ARPGDemoBaseGameMode>())
	{
		switch (BaseGameMode->GetCurrentGameDifficulty())
		{
		case ERPGDemoGameDifficulty::Easy:
			AbilityApplyLevel = 1;
			break;

		case ERPGDemoGameDifficulty::Normal:
			AbilityApplyLevel = 2;
			break;

		case ERPGDemoGameDifficulty::Hard:
			AbilityApplyLevel = 3;
			break;

		case ERPGDemoGameDifficulty::ExtremelyHard:
			AbilityApplyLevel = 4;
			break;

		default:
			break;
		}
	}

	EnemyStartUpDataHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		CharacterStartUpData.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this,
			[this, AbilityApplyLevel]()
			{
				bEnemyStartUpDataLoading = false;
				if (!HasAuthority() || IsActorBeingDestroyed() || !GetWorld() ||
					GetWorld()->bIsTearingDown || !IsValid(RPGDemoAbilitySystemComponent) ||
					RPGDemoAbilitySystemComponent->GetAvatarActor() != this)
				{
					return;
				}

				if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.Get())
				{
					LoadedData->GiveToAbilitySystemComponent(RPGDemoAbilitySystemComponent, AbilityApplyLevel);
					bEnemyStartUpDataInitialized = true;
				}
			}
		)
	);
}

void ARPGDemoEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EnemyStartUpDataHandle.IsValid())
	{
		EnemyStartUpDataHandle->CancelHandle();
		EnemyStartUpDataHandle.Reset();
	}
	bEnemyStartUpDataLoading = false;
	GetWorldTimerManager().ClearTimer(DeathPresentationTimerHandle);
	Super::EndPlay(EndPlayReason);
}
