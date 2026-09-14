// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/RPGDemoHeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DataAssets/Input/DataAsset_InputConfig.h"
#include "Components/Input/RPGDemoInputComponent.h"
#include "RPGDemoGameplayTags.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "DataAssets/StartUpData/DataAsset_HeroStartUpData.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/UI/HeroUIComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Items/PickUps/RPGDemoStoneBase.h"

#include "RPGDemoDebugHelper.h"


ARPGDemoHeroCharacter::ARPGDemoHeroCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 200.f;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 65.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	HeroCombatComponent = CreateDefaultSubobject<UHeroCombatComponent>(TEXT("HeroCombatComponent"));

	HeroUIComponent = CreateDefaultSubobject<UHeroUIComponent>(TEXT("HeroUIComponent"));

}

void ARPGDemoHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGDemoHeroCharacter, ReplicatedMovementInputDirection);
}

UPawnCombatComponent* ARPGDemoHeroCharacter::GetPawnCombatComponent() const
{
	return HeroCombatComponent;
}

UPawnUIComponent* ARPGDemoHeroCharacter::GetPawnUIComponent() const
{
	return HeroUIComponent;
}

UHeroUIComponent* ARPGDemoHeroCharacter::GetHeroUIComponent() const
{
	return HeroUIComponent;
}

void ARPGDemoHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if(!CharacterStartUpData.IsNull())
	{
		if (UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous())
		{
			int32 AbilityApplyLevel = 1;

			if (ARPGDemoBaseGameMode* BaseGameMode = GetWorld()->GetAuthGameMode<ARPGDemoBaseGameMode>())
			{
				switch (BaseGameMode->GetCurrentGameDifficulty())
				{
				case ERPGDemoGameDifficulty::Easy:
					AbilityApplyLevel = 4;
					break;

					case ERPGDemoGameDifficulty::Normal:
					AbilityApplyLevel = 3;
					break;

					case ERPGDemoGameDifficulty::Hard:
					AbilityApplyLevel = 2;
					break;

					case ERPGDemoGameDifficulty::ExtremelyHard:
					AbilityApplyLevel = 1;
					break;

					default:
					break;
				}
			}

			LoadedData->GiveToAbilitySystemComponent(RPGDemoAbilitySystemComponent, AbilityApplyLevel);
		}
	}
}

void ARPGDemoHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	checkf(InputConfigDataAsset, TEXT("Forgot to assign a valid data asset as input config"));

	ULocalPlayer* LocalPlayer = GetController<APlayerController>()->GetLocalPlayer();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	check(Subsystem);

	Subsystem->AddMappingContext(InputConfigDataAsset->DefaultMappingContext, 0);

	URPGDemoInputComponent* RPGDemoInputComponent = CastChecked<URPGDemoInputComponent>(PlayerInputComponent);

	RPGDemoInputComponent->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	RPGDemoInputComponent->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);

	RPGDemoInputComponent->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Triggered, this, &ThisClass::Input_SwitchTargetTriggered);
	RPGDemoInputComponent->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Completed, this, &ThisClass::Input_SwitchTargetCompleted);

	RPGDemoInputComponent->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_PickUp_Stones, ETriggerEvent::Started, this, &ThisClass::Input_PickUpStoneStarted);
	RPGDemoInputComponent->BindAbilityInputAction(InputConfigDataAsset, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

// Called when the game starts or when spawned
void ARPGDemoHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

}

void ARPGDemoHeroCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	const FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	const FRotator MovementRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = MovementRotation.RotateVector(FVector::ForwardVector);
	const FVector RightDirection = MovementRotation.RotateVector(FVector::RightVector);
	const FVector DesiredDirection =
		(ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal2D();

	if (!DesiredDirection.IsNearlyZero() &&
		!DesiredDirection.Equals(ReplicatedMovementInputDirection, 0.01f))
	{
		ReplicatedMovementInputDirection = DesiredDirection;
		if (!HasAuthority())
		{
			ServerUpdateMovementInputDirection(DesiredDirection);
		}
	}

	if (MovementVector.Y != 0.f)
	{
		AddMovementInput(ForwardDirection, MovementVector.Y);

	}

	if(MovementVector.X != 0.f)
	{
		AddMovementInput(RightDirection, MovementVector.X);
	}

}

void ARPGDemoHeroCharacter::ServerUpdateMovementInputDirection_Implementation(
	FVector_NetQuantizeNormal NewDirection)
{
	const FVector SafeDirection = FVector(NewDirection).GetSafeNormal2D();
	if (!SafeDirection.IsNearlyZero())
	{
		ReplicatedMovementInputDirection = SafeDirection;
	}
}

FVector ARPGDemoHeroCharacter::GetNetworkMovementInputDirection() const
{
	const FVector Direction = FVector(ReplicatedMovementInputDirection).GetSafeNormal2D();
	return Direction.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Direction;
}

void ARPGDemoHeroCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();

	if(LookAxisVector.X != 0.f)
	{
		AddControllerYawInput(LookAxisVector.X);
	}

	if(LookAxisVector.Y != 0.f)
	{
		AddControllerPitchInput(LookAxisVector.Y);
	}

}

void ARPGDemoHeroCharacter::Input_SwitchTargetTriggered(const FInputActionValue& InputActionValue)
{
	SwitchDirection = InputActionValue.Get<FVector2D>();
}

void ARPGDemoHeroCharacter::Input_SwitchTargetCompleted(const FInputActionValue& InputActionValue)
{
	FGameplayEventData Data;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		this,
		SwitchDirection.X > 0.f ? RPGDemoGameplayTags::Player_Event_SwitchTarget_Right : RPGDemoGameplayTags::Player_Event_SwitchTarget_Left,
		Data
		);
}

void ARPGDemoHeroCharacter::Input_PickUpStoneStarted(const FInputActionValue& InputActionValue)
{
	if (HasAuthority())
	{
		TryConsumeNearbyStones();
	}
	else
	{
		ServerTryConsumeNearbyStones();
	}
}

void ARPGDemoHeroCharacter::ServerTryConsumeNearbyStones_Implementation()
{
	TryConsumeNearbyStones();
}

void ARPGDemoHeroCharacter::TryConsumeNearbyStones()
{
	if (!HasAuthority() || !RPGDemoAbilitySystemComponent ||
		RPGDemoAbilitySystemComponent->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead))
	{
		return;
	}

	const float MaxDistanceSquared = FMath::Square(FMath::Max(StonePickUpRequestRadius, 0.f));
	for (TActorIterator<ARPGDemoStoneBase> It(GetWorld()); It; ++It)
	{
		ARPGDemoStoneBase* Stone = *It;
		if (!IsValid(Stone) || Stone->IsConsumed() ||
			FVector::DistSquared(GetActorLocation(), Stone->GetActorLocation()) > MaxDistanceSquared)
		{
			continue;
		}

		Stone->Consume(RPGDemoAbilitySystemComponent, 1);
	}
}

void ARPGDemoHeroCharacter::Input_AbilityInputPressed(FGameplayTag InInputTag)
{
	RPGDemoAbilitySystemComponent->OnAbilityInputPressed(InInputTag);
}

void ARPGDemoHeroCharacter::Input_AbilityInputReleased(FGameplayTag InInputTag)
{
	RPGDemoAbilitySystemComponent->OnAbilityInputReleased(InInputTag);
}
