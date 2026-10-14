// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/Hero/RPGDemoHeroAnimInstance.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "Combat/RPGDemoCombatLog.h"
#include "RPGDemoGameplayTags.h"
#include "Animation/PoseSnapshot.h"

void URPGDemoHeroAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if(OwningCharacter)
	{
		OwningHeroCharacter = Cast<ARPGDemoHeroCharacter>(OwningCharacter);
	}
}

void URPGDemoHeroAnimInstance::BeginLocalHitStop(float Duration)
{
    if (!OwningHeroCharacter || !OwningHeroCharacter->IsLocallyControlled() || !GetWorld()) return;
    // Snapshot only the rendered pose. The live graph remains the base pose of
    // a layered blend, updated at full weight with its original root motion.
    const FName SnapshotName(TEXT("RPGDemoLocalHitStop"));
    if (LocalHitStopAlpha <= 0.f) SavePoseSnapshot(SnapshotName);
    const FPoseSnapshot* Pose = GetPoseSnapshot(SnapshotName);
    LocalHitStopAlpha = Pose && Pose->bIsValid ? 1.f : 0.f;
    LocalHitStopEndRealTime = GetWorld()->GetRealTimeSeconds() + FMath::Clamp(Duration, 0.f, 0.5f);
    if (RPGDemoCombat::IsDebugEnabled())
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LocalHitStop begin pawn=%s duration=%.3f alpha=%.1f"), *OwningHeroCharacter->GetName(), Duration, LocalHitStopAlpha);
}

void URPGDemoHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    if (LocalHitStopAlpha <= 0.f) return;
    const float Remaining = GetWorld() ? static_cast<float>(LocalHitStopEndRealTime - GetWorld()->GetRealTimeSeconds()) : 0.f;
    const bool bCanPresent = OwningHeroCharacter && OwningHeroCharacter->IsLocallyControlled()
        && OwningHeroCharacter->GetRPGDemoAbilitySystemComponent()
        && !OwningHeroCharacter->GetRPGDemoAbilitySystemComponent()->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead);
    LocalHitStopAlpha = bCanPresent ? FMath::Clamp(Remaining / 0.03f, 0.f, 1.f) : 0.f;
    if (LocalHitStopAlpha <= 0.f && RPGDemoCombat::IsDebugEnabled())
        UE_LOG(LogRPGDemoCombat, Log, TEXT("LocalHitStop end pawn=%s"), *GetNameSafe(OwningHeroCharacter));
}

void URPGDemoHeroAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if(bHasAcceleration)
	{
		IdleElapsedTime = 0.f;
		bShouldEnterRelaxState = false;
	}
	else
	{
		IdleElapsedTime += DeltaSeconds;
		bShouldEnterRelaxState = (IdleElapsedTime >= EnterRelaxStateThreshold);
	}
}
