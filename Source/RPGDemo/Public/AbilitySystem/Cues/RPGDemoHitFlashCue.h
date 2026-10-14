#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "RPGDemoHitFlashCue.generated.h"

UCLASS()
class RPGDEMO_API ARPGDemoHitFlashCue : public AGameplayCueNotify_Actor
{
    GENERATED_BODY()
public:
    ARPGDemoHitFlashCue();
    virtual bool OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
    virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
    virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
private:
    void SetFlash(AActor* Target, float Value);
};
