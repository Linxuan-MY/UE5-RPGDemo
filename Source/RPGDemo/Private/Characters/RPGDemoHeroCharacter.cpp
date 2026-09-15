// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/RPGDemoHeroCharacter.h"

#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Input/RPGDemoInputComponent.h"
#include "Components/UI/HeroUIComponent.h"
#include "DataAssets/Input/DataAsset_InputConfig.h"
#include "DataAssets/StartUpData/DataAsset_HeroStartUpData.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameModes/RPGDemoBaseGameMode.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "Items/PickUps/RPGDemoStoneBase.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoGameplayTags.h"

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

UPawnCombatComponent* ARPGDemoHeroCharacter::GetPawnCombatComponent() const { return HeroCombatComponent; }
UPawnUIComponent* ARPGDemoHeroCharacter::GetPawnUIComponent() const { return HeroUIComponent; }
UHeroUIComponent* ARPGDemoHeroCharacter::GetHeroUIComponent() const { return HeroUIComponent; }

void ARPGDemoHeroCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilitySystemFromPlayerState();
    GrantPersistentHeroStartUpData();
}

void ARPGDemoHeroCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilitySystemFromPlayerState();
}

void ARPGDemoHeroCharacter::InitializeAbilitySystemFromPlayerState()
{
    ARPGDemoPlayerState* RPGDemoPlayerState = GetPlayerState<ARPGDemoPlayerState>();
    if (!RPGDemoPlayerState) return;
    RPGDemoAbilitySystemComponent = RPGDemoPlayerState->GetRPGDemoAbilitySystemComponent();
    RPGDemoAttributeSet = RPGDemoPlayerState->GetRPGDemoAttributeSet();
    if (RPGDemoAbilitySystemComponent)
    {
        RPGDemoAbilitySystemComponent->InitAbilityActorInfo(RPGDemoPlayerState, this);
    }
}

void ARPGDemoHeroCharacter::GrantPersistentHeroStartUpData()
{
    ARPGDemoPlayerState* RPGDemoPlayerState = GetPlayerState<ARPGDemoPlayerState>();
    if (!HasAuthority() || !RPGDemoPlayerState || !RPGDemoAbilitySystemComponent || CharacterStartUpData.IsNull())
    {
        return;
    }

    UDataAsset_StartUpDataBase* LoadedData = CharacterStartUpData.LoadSynchronous();
    if (!LoadedData || !RPGDemoPlayerState->TryMarkHeroStartUpDataGranted()) return;
    int32 AbilityApplyLevel = 1;
    if (const ARPGDemoBaseGameMode* BaseGameMode = GetWorld()->GetAuthGameMode<ARPGDemoBaseGameMode>())
    {
        switch (BaseGameMode->GetCurrentGameDifficulty())
        {
        case ERPGDemoGameDifficulty::Easy: AbilityApplyLevel = 4; break;
        case ERPGDemoGameDifficulty::Normal: AbilityApplyLevel = 3; break;
        case ERPGDemoGameDifficulty::Hard: AbilityApplyLevel = 2; break;
        case ERPGDemoGameDifficulty::ExtremelyHard: AbilityApplyLevel = 1; break;
        default: break;
        }
    }
    LoadedData->GiveToAbilitySystemComponent(RPGDemoAbilitySystemComponent, AbilityApplyLevel);
}

void ARPGDemoHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    checkf(InputConfigDataAsset, TEXT("Forgot to assign a valid data asset as input config"));
    APlayerController* PlayerController = GetController<APlayerController>();
    ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer
        ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
    check(Subsystem);
    Subsystem->AddMappingContext(InputConfigDataAsset->DefaultMappingContext, 0);
    URPGDemoInputComponent* Input = CastChecked<URPGDemoInputComponent>(PlayerInputComponent);
    Input->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
    Input->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
    Input->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Triggered, this, &ThisClass::Input_SwitchTargetTriggered);
    Input->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_SwitchTarget, ETriggerEvent::Completed, this, &ThisClass::Input_SwitchTargetCompleted);
    Input->BindNativeInputAction(InputConfigDataAsset, RPGDemoGameplayTags::InputTag_PickUp_Stones, ETriggerEvent::Started, this, &ThisClass::Input_PickUpStoneStarted);
    Input->BindAbilityInputAction(InputConfigDataAsset, this, &ThisClass::Input_AbilityInputPressed, &ThisClass::Input_AbilityInputReleased);
}

void ARPGDemoHeroCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeAbilitySystemFromPlayerState();
}

void ARPGDemoHeroCharacter::Input_Move(const FInputActionValue& Value)
{
    const FVector2D Movement = Value.Get<FVector2D>();
    if (!Controller) return;
    const FRotator Rotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector Forward = Rotation.RotateVector(FVector::ForwardVector);
    const FVector Right = Rotation.RotateVector(FVector::RightVector);
    const FVector Desired = (Forward * Movement.Y + Right * Movement.X).GetSafeNormal2D();
    if (!Desired.IsNearlyZero() && !Desired.Equals(ReplicatedMovementInputDirection, 0.01f))
    {
        ReplicatedMovementInputDirection = Desired;
        if (!HasAuthority()) ServerUpdateMovementInputDirection(Desired);
    }
    if (Movement.Y != 0.f) AddMovementInput(Forward, Movement.Y);
    if (Movement.X != 0.f) AddMovementInput(Right, Movement.X);
}

void ARPGDemoHeroCharacter::ServerUpdateMovementInputDirection_Implementation(FVector_NetQuantizeNormal NewDirection)
{
    const FVector SafeDirection = FVector(NewDirection).GetSafeNormal2D();
    if (!SafeDirection.IsNearlyZero()) ReplicatedMovementInputDirection = SafeDirection;
}

FVector ARPGDemoHeroCharacter::GetNetworkMovementInputDirection() const
{
    const FVector Direction = FVector(ReplicatedMovementInputDirection).GetSafeNormal2D();
    return Direction.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Direction;
}

void ARPGDemoHeroCharacter::Input_Look(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>();
    if (Look.X != 0.f) AddControllerYawInput(Look.X);
    if (Look.Y != 0.f) AddControllerPitchInput(Look.Y);
}

void ARPGDemoHeroCharacter::Input_SwitchTargetTriggered(const FInputActionValue& Value) { SwitchDirection = Value.Get<FVector2D>(); }

void ARPGDemoHeroCharacter::Input_SwitchTargetCompleted(const FInputActionValue&)
{
    FGameplayEventData Data;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this,
        SwitchDirection.X > 0.f ? RPGDemoGameplayTags::Player_Event_SwitchTarget_Right : RPGDemoGameplayTags::Player_Event_SwitchTarget_Left,
        Data);
}

void ARPGDemoHeroCharacter::Input_PickUpStoneStarted(const FInputActionValue&)
{
    if (HasAuthority()) TryConsumeNearbyStones(); else ServerTryConsumeNearbyStones();
}

void ARPGDemoHeroCharacter::ServerTryConsumeNearbyStones_Implementation() { TryConsumeNearbyStones(); }

void ARPGDemoHeroCharacter::TryConsumeNearbyStones()
{
    if (!HasAuthority() || !RPGDemoAbilitySystemComponent ||
        RPGDemoAbilitySystemComponent->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead)) return;
    const float MaxDistanceSquared = FMath::Square(FMath::Max(StonePickUpRequestRadius, 0.f));
    for (TActorIterator<ARPGDemoStoneBase> It(GetWorld()); It; ++It)
    {
        ARPGDemoStoneBase* Stone = *It;
        if (IsValid(Stone) && !Stone->IsConsumed() &&
            FVector::DistSquared(GetActorLocation(), Stone->GetActorLocation()) <= MaxDistanceSquared)
        {
            Stone->Consume(RPGDemoAbilitySystemComponent, 1);
        }
    }
}

void ARPGDemoHeroCharacter::Input_AbilityInputPressed(FGameplayTag Tag)
{
    if (RPGDemoAbilitySystemComponent) RPGDemoAbilitySystemComponent->OnAbilityInputPressed(Tag);
}

void ARPGDemoHeroCharacter::Input_AbilityInputReleased(FGameplayTag Tag)
{
    if (RPGDemoAbilitySystemComponent) RPGDemoAbilitySystemComponent->OnAbilityInputReleased(Tag);
}
