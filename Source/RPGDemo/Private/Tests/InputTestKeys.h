#pragma once

#include "Characters/RPGDemoHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "DataAssets/Input/DataAsset_InputConfig.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"
#include "RPGDemoGameplayTags.h"

namespace RPGDemoInputTest
{
    inline void SendKey(APlayerController* PC, FKey Key, EInputEvent Event)
    {
        UGameViewportClient* Viewport = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->ViewportClient.Get() : nullptr;
        if (Viewport && Viewport->Viewport)
            Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport, FInputDeviceId::CreateFromInternalId(0), Key, Event, FPlatformTime::Cycles64()));
    }

    // Resolve both the physical key and its chord dependencies from the real
    // assets. Special attacks require their modifier action before the mouse key.
    inline TArray<FKey> ResolveKeys(const ARPGDemoHeroCharacter* Hero, FGameplayTag InputTag)
    {
        TArray<FKey> Keys;
        const UDataAsset_InputConfig* Config = Hero ? Hero->GetInputConfig() : nullptr;
        if (!Config) return Keys;
        auto* Weapon = Hero->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe);
        const UInputMappingContext* Contexts[] = { Weapon ? Weapon->HeroWeaponData.WeaponInputMappingContext : nullptr, Config->DefaultMappingContext };
        TSet<const UInputAction*> Visited;
        TFunction<bool(const UInputAction*)> Resolve = [&](const UInputAction* Action)
        {
            if (!Action || Visited.Contains(Action)) return false;
            Visited.Add(Action);
            for (const auto* Context : Contexts)
            {
                if (!Context) continue;
                for (const auto& Mapping : Context->GetMappings())
                {
                    if (Mapping.Action != Action) continue;
                    const auto ResolveChords = [&](const TArray<TObjectPtr<UInputTrigger>>& Triggers)
                    {
                        for (const auto& Trigger : Triggers)
                            if (const auto* Chord = Cast<UInputTriggerChordAction>(Trigger.Get()))
                                if (!Resolve(Chord->ChordAction)) return false;
                        return true;
                    };
                    if (!ResolveChords(Action->Triggers) || !ResolveChords(Mapping.Triggers)) return false;
                    Keys.AddUnique(Mapping.Key);
                    return true;
                }
            }
            return false;
        };
        for (const auto& Binding : Config->AbilityInputActions)
            if (Binding.InputTag == InputTag && Resolve(Binding.InputAction)) return Keys;
        return {};
    }
}
