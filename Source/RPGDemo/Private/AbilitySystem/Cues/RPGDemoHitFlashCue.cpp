#include "AbilitySystem/Cues/RPGDemoHitFlashCue.h"
#include "Characters/RPGDemoBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/RPGDemoCombatLog.h"
#include "RPGDemoGameplayTags.h"

ARPGDemoHitFlashCue::ARPGDemoHitFlashCue()
{
    GameplayCueTag = RPGDemoGameplayTags::GameplayCue_FX_HitFlash;
    bAutoDestroyOnRemove = true;
    bAllowMultipleOnActiveEvents = true;
    bAllowMultipleWhileActiveEvents = true;
}

void ARPGDemoHitFlashCue::SetFlash(AActor* Target, float Value)
{
    if (ARPGDemoBaseCharacter* Character = Cast<ARPGDemoBaseCharacter>(Target))
    {
        Character->GetMesh()->SetScalarParameterValueOnMaterials(TEXT("HitFxSwitch"), Value);
        if (RPGDemoCombat::IsDebugEnabled())
            UE_LOG(LogRPGDemoCombat, Log, TEXT("HitFlash target=%s enabled=%d netMode=%d"), *Target->GetName(), Value > 0.f, Target->GetNetMode());
    }
}

bool ARPGDemoHitFlashCue::OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
    SetFlash(Target, 1.f);
    return true;
}

bool ARPGDemoHitFlashCue::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
    SetFlash(Target, 1.f);
    return true;
}

bool ARPGDemoHitFlashCue::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
    SetFlash(Target, 0.f);
    return true;
}
