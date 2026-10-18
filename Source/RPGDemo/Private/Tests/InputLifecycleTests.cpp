#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Settings/RPGDemoNetworkSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.h"
#include "GameplayEffect.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Controllers/RPGDemoHeroController.h"
#include "Controllers/RPGDemoLobbyPlayerController.h"
#include "DataAssets/Input/DataAsset_InputConfig.h"
#include "GameModes/RPGDemoSurvivalGameMode.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"
#include "RPGDemoGameInstance.h"
#include "RPGDemoGameplayTags.h"
#include "RPGDemoFunctionLibrary.h"
#include "Tests/InputTestKeys.h"

// Owns a disposable DS + two offline PIE windows. Connects through the menu's
// GameInstance entry point; weapon assertions use viewport FKey events only.
class FRPGDemoInputLifecycleCommand : public IAutomationLatentCommand
{
public:
    explicit FRPGDemoInputLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FRPGDemoInputLifecycleCommand() override
    {
        if (!Settings.IsValid()) return;
        ReleaseKeys();
        for (int32 Index = 0; Index < 2; ++Index)
        {
            if (auto* Input = Subsystems[Index].Get()) Input->RemoveMappingContext(Sentinels[Index].Get());
            if (auto* ASC = ObservedASCs[Index].Get()) ASC->AbilityActivatedCallbacks.Remove(ActivationHandles[Index]);
        }
        if (GEditor) GEditor->RequestEndPlayMap();
        auto* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
        Defaults->SetPlayNetMode(PreviousNetMode);
        Defaults->SetRunUnderOneProcess(bPreviousOneProcess);
        Defaults->SetPlayNumberOfClients(PreviousClientCount);
        Defaults->bLaunchSeparateServer = bPreviousSeparateServer;
        Defaults->SetServerPort(PreviousPort);
        SetServerMap(Defaults, PreviousServerMap);
        Defaults->GameGetsMouseControl = bPreviousMouseControl;
        Defaults->SaveConfig();
        GetMutableDefault<URPGDemoNetworkSettings>()->DedicatedServerEndpoint = PreviousEndpoint;
        IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel"))->Set(PreviousSeamlessTravel, ECVF_SetByCode);
    }

    bool Update() override
    {
        if (!Settings.IsValid()) { Start(); return false; }
        if (FPlatformTime::Seconds() - Started > 420.0)
        {
            Test->AddError(FString::Printf(TEXT("Input lifecycle timeout: stage=%d cycle=%d"), Stage, Cycle));
            return true;
        }
        const double Now = FPlatformTime::Seconds();
        if (ReleaseAt > 0.0 && Now >= ReleaseAt) ReleaseKeys();
        UWorld* Server = nullptr;
        TArray<UWorld*> Clients;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (Context.WorldType != EWorldType::PIE || !World) continue;
            if (World->GetNetMode() == NM_DedicatedServer) Server = World;
            else if (World->GetGameViewport()) Clients.Add(World);
        }
        if (!Server || Clients.Num() != 2) return false;
        if (Stage == 0)
        {
            if (!Server->GetMapName().Contains(TEXT("MultiplayerLobbyMap")))
            {
                if (!bServerTravelRequested)
                {
                    Server->GetAuthGameMode()->bUseSeamlessTravel = true;
                    Server->ServerTravel(TEXT("/Game/Maps/MultiplayerLobbyMap?game=/Script/RPGDemo.RPGDemoLobbyGameMode"), false);
                    bServerTravelRequested = true;
                }
                return false;
            }
            for (UWorld* Client : Clients)
                if (!Client->GetMapName().Contains(TEXT("MainMenuMap")) || !Client->GetFirstPlayerController()) return false;
            for (UWorld* Client : Clients)
            {
                Test->TestEqual(TEXT("Offline menu before connecting"), Client->GetNetMode(), NM_Standalone);
                Client->GetGameInstance<URPGDemoGameInstance>()->ConnectToDedicatedServer();
            }
            SetStage(1);
            return false;
        }
        if (Stage == 1)
        {
            ARPGDemoLobbyPlayerController* Host = nullptr;
            for (UWorld* Client : Clients)
            {
                auto* PC = Cast<ARPGDemoLobbyPlayerController>(Client->GetFirstPlayerController());
                auto* PS = PC ? PC->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
                if (!PS) return false;
                if (PS->bIsLobbyHost) Host = PC;
            }
            if (!Host) return false;
            Host->ServerStartGame();
            SetStage(2);
            return false;
        }
        auto* Mode = Server->GetAuthGameMode<ARPGDemoSurvivalGameMode>();
        if (!Mode) return false;
        Mode->SetActorTickEnabled(false);
        for (TActorIterator<ARPGDemoEnemyCharacter> It(Server); It; ++It)
        {
            It->OnDestroyed.RemoveAll(Mode);
            It->Destroy();
        }
        ARPGDemoHeroController* Controllers[2] = {};
        ARPGDemoHeroCharacter* Heroes[2] = {};
        for (int32 Index = 0; Index < 2; ++Index)
        {
            Controllers[Index] = Cast<ARPGDemoHeroController>(Clients[Index]->GetFirstPlayerController());
            Heroes[Index] = Controllers[Index] ? Cast<ARPGDemoHeroCharacter>(Controllers[Index]->GetPawn()) : nullptr;
        }
        if (Stage == 2)
        {
            for (int32 Index = 0; Index < 2; ++Index)
            {
                if (!Heroes[Index] || !Heroes[Index]->InputComponent || !Carried(Heroes[Index])) return false;
                auto* PC = Controllers[Index];
                auto* Input = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
                Subsystems[Index] = Input;
                Sentinels[Index].Reset(NewObject<UInputMappingContext>());
                Input->AddMappingContext(Sentinels[Index].Get(), 12);
                auto* ASC = Heroes[Index]->GetRPGDemoAbilitySystemComponent();
                ObservedASCs[Index] = ASC;
                ActivationHandles[Index] = ASC->AbilityActivatedCallbacks.AddLambda([this, Index](UGameplayAbility* Ability)
                { Activations[Index].Add(Ability->GetClass()); });
                Test->AddInfo(FString::Printf(TEXT("Player%d LocalPlayer=%s PC=%s Pawn=%s ViewTarget=%s"), Index,
                    *PC->GetLocalPlayer()->GetPathName(), *PC->GetPathName(), *Heroes[Index]->GetPathName(), *GetPathNameSafe(PC->GetViewTarget())));
            }
            SetStage(3);
        }
        const int32 Victim = Cycle % 2;
        const int32 Survivor = 1 - Victim;
        auto* PC = Controllers[Victim];
        auto* Hero = Heroes[Victim];
        auto* Input = Subsystems[Victim].Get();
        auto* SurvivorPC = Controllers[Survivor];
        const double Elapsed = Now - StageStarted;
        if (Stage == 3)
        {
            if (!Hero || !Heroes[Survivor] || !Carried(Hero)) return false;
            OldPawn.Reset(Hero);
            WeaponMapping = Carried(Hero)->HeroWeaponData.WeaponInputMappingContext;
            SurvivorMappings = MappingPriorities(SurvivorPC, Heroes[Survivor], Sentinels[Survivor].Get());
            auto Widget = SurvivorPC->GetLocalPlayer()->ViewportClient->GetGameViewportWidget();
            FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
            FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
            Key(SurvivorPC, EKeys::W, IE_Pressed);
            bStableFocus = true;
            bStableMovement = true;
            // Cycle pairs cover death carried, fully equipped and during equip.
            if (Cycle / 2 != 0) Press(PC, EKeys::One);
            SetStage(4);
        }
        else if (Stage == 4)
        {
            if (!Hero) return false;
            auto* AuthorityHero = ServerHero(Server, Hero);
            if (!AuthorityHero) return false;
            const bool bMidEquip = Cycle / 2 == 2;
            if (bMidEquip)
            {
                bool bEquipping = false;
                for (const auto& Spec : AuthorityHero->GetRPGDemoAbilitySystemComponent()->GetActivatableAbilities())
                    bEquipping |= Spec.IsActive() && Spec.Ability->GetName().Contains(TEXT("EquipAxe"));
                if (!bEquipping && Elapsed < 3.0) return false;
                Test->TestTrue(TEXT("Death occurs during real key-triggered equip"), bEquipping);
            }
            else if (Elapsed < 3.0) return false;
            if (Cycle / 2 == 1) Test->TestNotNull(TEXT("Equipped before death"), AuthorityHero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon());
            auto* ASC = AuthorityHero->GetRPGDemoAbilitySystemComponent();
            UGameplayEffect* Damage = NewObject<UGameplayEffect>();
            auto& Mod = Damage->Modifiers.AddDefaulted_GetRef();
            Mod.Attribute = URPGDemoAttributeSet::GetDamageTakenAttribute();
            Mod.ModifierOp = EGameplayModOp::Additive;
            Mod.ModifierMagnitude = FScalableFloat(100000.f);
            ASC->ApplyGameplayEffectToSelf(Damage, 1.f, ASC->MakeEffectContext());
            SetStage(5);
        }
        else if (Stage == 5)
        {
            if (Elapsed > 0.5 && Heroes[Survivor])
            {
                bStableFocus &= FSlateApplication::Get().GetKeyboardFocusedWidget() == FocusedWidget.Pin();
                bStableMovement &= SurvivorPC->IsInputKeyDown(EKeys::W) && !SurvivorPC->IsMoveInputIgnored() &&
                    !Heroes[Survivor]->GetLastMovementInputVector().IsNearlyZero();
            }
            if (!Hero || Hero == OldPawn.Get() || !Carried(Hero) || !Hero->InputComponent) return false;
            Test->TestFalse(TEXT("Respawn removes old weapon mapping"), Input->HasMappingContext(WeaponMapping.Get()));
            Test->TestNull(TEXT("Respawn starts carried"), Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon());
            SetStage(6);
        }
        else if (Stage == 6)
        {
            // The respawned player sends no keys for three seconds. The survivor
            // must keep focus and its original held W without another key-down.
            bStableFocus &= FSlateApplication::Get().GetKeyboardFocusedWidget() == FocusedWidget.Pin();
            bStableMovement &= SurvivorPC->IsInputKeyDown(EKeys::W) && !SurvivorPC->IsMoveInputIgnored() &&
                !Heroes[Survivor]->GetLastMovementInputVector().IsNearlyZero();
            if (Elapsed < 3.0) return false;
            Test->TestTrue(TEXT("Background respawn never steals keyboard focus"), bStableFocus);
            Test->TestTrue(TEXT("Survivor's held movement stays active"), bStableMovement);
            Test->TestTrue(TEXT("Survivor mapping contexts unchanged"),
                SurvivorMappings.OrderIndependentCompareEqual(MappingPriorities(SurvivorPC, Heroes[Survivor], Sentinels[Survivor].Get())));
            Key(SurvivorPC, EKeys::W, IE_Released);
            Test->TestTrue(TEXT("Other system's mapping preserved"), Input->HasMappingContext(Sentinels[Victim].Get()));
            Activations[Victim].Reset();
            Press(PC, EKeys::One);
            SetStage(7);
        }
        else if (Stage == 7 && Elapsed > 2.5)
        {
            Test->TestNotNull(TEXT("First new key 1 equips after respawn"), Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon());
            Test->TestTrue(TEXT("Weapon mapping installed after equip"), Input->HasMappingContext(WeaponMapping.Get()));
            PC->ReleasePawnInputContexts(OldPawn.Get());
            Test->TestTrue(TEXT("Old Pawn cannot release successor's mapping"), Input->HasMappingContext(WeaponMapping.Get()));
            Press(PC, EKeys::One);
            SetStage(8);
        }
        else if (Stage == 8 && Elapsed > 2.5)
        {
            Test->TestNull(TEXT("Key 1 unequips"), Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon());
            Test->TestFalse(TEXT("Unequip removes weapon mapping"), Input->HasMappingContext(WeaponMapping.Get()));
            Press(PC, EKeys::One);
            ActionIndex = 0;
            SetStage(9);
        }
        else if (Stage == 9 && Elapsed > 2.5)
        {
            auto* Weapon = Hero ? Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon() : nullptr;
            if (!Test->TestNotNull(TEXT("Re-equip via key"), Weapon)) return true;
            Actions.Reset();
            for (const auto& Ability : Weapon->HeroWeaponData.DefaultWeaponAbilities)
                if (Ability.AbilityToGrant && (Ability.AbilityToGrant->GetName().Contains(TEXT("LightAttack")) || Ability.AbilityToGrant->GetName().Contains(TEXT("HeavyAttack"))))
                    Actions.Add(Ability);
            for (const auto& Ability : Weapon->HeroWeaponData.SpecialWeaponAbilities) Actions.Add(Ability);
            SetStage(10);
        }
        else if (Stage == 10)
        {
            if (!Actions.IsValidIndex(ActionIndex))
            {
                Test->AddInfo(FString::Printf(TEXT("Completed real-key death/respawn cycle %d player=%d deathState=%d"), Cycle + 1, Victim, Cycle / 2));
                bCleanupKeyPressed = false;
                SetStage(11);
                return false;
            }
            const auto& Action = Actions[ActionIndex];
            if (!bActionPressed)
            {
                const TArray<FKey> PhysicalKeys = RPGDemoInputTest::ResolveKeys(Hero, Action.InputTag);
                if (!Test->TestFalse(TEXT("Ability has real physical key mapping"), PhysicalKeys.IsEmpty())) return true;
                Activations[Victim].Remove(Action.AbilityToGrant);
                Press(PC, PhysicalKeys);
                bActionPressed = true;
                StageStarted = Now;
            }
            else if (Elapsed > 3.0)
            {
                Test->TestTrue(*FString::Printf(TEXT("Physical key activates %s"), *Action.AbilityToGrant->GetName()), Activations[Victim].Contains(Action.AbilityToGrant));
                ++ActionIndex;
                bActionPressed = false;
            }
        }
        else if (Stage == 11)
        {
            // The heavy special montage can outlive the activation assertion.
            // Respect its normal ability blocking before preparing the next case.
            if (!bCleanupKeyPressed)
            {
                bool bBusy = false;
                for (const auto& Spec : Hero->GetRPGDemoAbilitySystemComponent()->GetActivatableAbilities()) bBusy |= Spec.IsActive();
                if (bBusy && Elapsed < 16.0) return false;
                Test->TestFalse(TEXT("Weapon ability finishes before cleanup unequip"), bBusy);
                Press(PC, EKeys::One);
                bCleanupKeyPressed = true;
                StageStarted = Now;
                return false;
            }
            if (Elapsed < 3.0) return false;
            Test->TestNull(TEXT("Unequip leaves next cycle carried"), Hero->GetHeroCombatComponent()->GetHeroCurrentEquippedWeapon());
            ++Cycle;
            if (Cycle >= 6) return true;
            SetStage(3);
        }
        return false;
    }
private:
    static void SetServerMap(ULevelEditorPlaySettings* Settings, const FString& Value)
    {
        FStrProperty* Property = FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("ServerMapNameOverride"));
        check(Property);
        Property->SetPropertyValue_InContainer(Settings, Value);
    }
    static TMap<const UInputMappingContext*, int32> MappingPriorities(APlayerController* PC, ARPGDemoHeroCharacter* Hero, UInputMappingContext* Sentinel)
    {
        TMap<const UInputMappingContext*, int32> Result;
        auto* Input = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        const UInputMappingContext* Contexts[] = { Hero->GetInputConfig()->DefaultMappingContext,
            Carried(Hero)->HeroWeaponData.WeaponInputMappingContext, Sentinel };
        for (const auto* Context : Contexts)
        {
            int32 Priority = INDEX_NONE;
            Input->HasMappingContext(Context, Priority);
            Result.Add(Context, Priority);
        }
        return Result;
    }
    static ARPGDemoHeroWeapon* Carried(ARPGDemoHeroCharacter* Hero)
    { return Hero ? Hero->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe) : nullptr; }
    static ARPGDemoHeroCharacter* ServerHero(UWorld* Server, ARPGDemoHeroCharacter* ClientHero)
    {
        for (TActorIterator<ARPGDemoHeroCharacter> It(Server); It; ++It)
            if (It->GetPlayerState() && ClientHero->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == ClientHero->GetPlayerState()->GetPlayerId()) return *It;
        return nullptr;
    }
    static void Key(APlayerController* PC, FKey Key, EInputEvent Event)
    {
        if (!PC || !PC->GetLocalPlayer() || !PC->GetLocalPlayer()->ViewportClient) return;
        UGameViewportClient* Viewport = PC->GetLocalPlayer()->ViewportClient;
        Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport, FInputDeviceId::CreateFromInternalId(0), Key, Event, FPlatformTime::Cycles64()));
    }
    void Press(APlayerController* PC, FKey InKey)
    {
        Press(PC, TArray<FKey>{InKey});
    }
    void Press(APlayerController* PC, const TArray<FKey>& Keys)
    {
        ReleaseKeys();
        PendingPC = PC; PendingKeys = Keys;
        for (FKey InKey : Keys) Key(PC, InKey, IE_Pressed);
        ReleaseAt = FPlatformTime::Seconds() + 0.15;
    }
    void ReleaseKeys()
    {
        if (PendingPC.IsValid())
            for (int32 Index = PendingKeys.Num() - 1; Index >= 0; --Index) Key(PendingPC.Get(), PendingKeys[Index], IE_Released);
        PendingPC.Reset(); PendingKeys.Reset(); ReleaseAt = 0.0;
    }
    void SetStage(int32 Value)
    {
        Stage = Value; StageStarted = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Log, TEXT("INPUT_REGRESSION stage=%d cycle=%d"), Stage, Cycle);
    }
    void Start()
    {
        IConsoleVariable* SeamlessTravel = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel"));
        PreviousSeamlessTravel = SeamlessTravel->GetInt();
        SeamlessTravel->Set(1, ECVF_SetByCode);
        const auto* Defaults = GetDefault<ULevelEditorPlaySettings>();
        Defaults->GetPlayNetMode(PreviousNetMode);
        Defaults->GetRunUnderOneProcess(bPreviousOneProcess);
        Defaults->GetPlayNumberOfClients(PreviousClientCount);
        bPreviousSeparateServer = Defaults->bLaunchSeparateServer;
        bPreviousMouseControl = Defaults->GameGetsMouseControl;
        Defaults->GetServerPort(PreviousPort);
        Defaults->GetServerMapNameOverride(PreviousServerMap);
        PreviousEndpoint = GetDefault<URPGDemoNetworkSettings>()->DedicatedServerEndpoint;
        Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
        Settings->SetPlayNetMode(PIE_Standalone);
        Settings->SetRunUnderOneProcess(true);
        Settings->SetPlayNumberOfClients(2);
        Settings->bLaunchSeparateServer = true;
        Settings->SetServerPort(19777);
        SetServerMap(Settings.Get(), TEXT("/Game/Maps/MultiplayerLobbyMap"));
        Settings->GameGetsMouseControl = false;
        GetMutableDefault<URPGDemoNetworkSettings>()->DedicatedServerEndpoint = TEXT("127.0.0.1:19777");
        FRequestPlaySessionParams Params;
        Params.EditorPlaySettings = Settings.Get();
        Params.GlobalMapOverride = TEXT("/Game/Maps/MainMenuMap");
        GEditor->RequestPlaySession(Params);
        Started = FPlatformTime::Seconds();
        StageStarted = Started;
    }
    FAutomationTestBase* Test;
    TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    EPlayNetMode PreviousNetMode = PIE_Standalone;
    bool bPreviousOneProcess = true, bPreviousSeparateServer = false, bPreviousMouseControl = false;
    int32 PreviousClientCount = 2;
    uint16 PreviousPort = 0;
    FString PreviousServerMap, PreviousEndpoint;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> Subsystems[2];
    TStrongObjectPtr<UInputMappingContext> Sentinels[2];
    TWeakObjectPtr<URPGDemoAbilitySystemComponent> ObservedASCs[2];
    FDelegateHandle ActivationHandles[2];
    TSet<UClass*> Activations[2];
    TMap<const UInputMappingContext*, int32> SurvivorMappings;
    TStrongObjectPtr<ARPGDemoHeroCharacter> OldPawn;
    TWeakObjectPtr<UInputMappingContext> WeaponMapping;
    TWeakPtr<SWidget> FocusedWidget;
    TWeakObjectPtr<APlayerController> PendingPC;
    TArray<FKey> PendingKeys;
    double Started = 0.0, StageStarted = 0.0, ReleaseAt = 0.0;
    int32 Stage = 0, Cycle = 0, ActionIndex = 0;
    int32 PreviousSeamlessTravel = 0;
    bool bStableFocus = true, bStableMovement = true, bActionPressed = false, bServerTravelRequested = false;
    bool bCleanupKeyPressed = false;
    TArray<FRPGDemoHeroAbilitySet> Actions;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoInputLifecycleTest, "RPGDemo.Network.InputLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoInputLifecycleTest::RunTest(const FString& Parameters)
{
    if (GEditor->IsPlayingSessionInEditor()) { AddError(TEXT("Stop the current PIE session before starting this disposable test.")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoInputLifecycleCommand(this));
    return true;
}
#endif
