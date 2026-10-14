#include "Tests/RPGDemoCombatTestHarness.h"
#include "Tests/RPGDemoAITestHarness.h"
#include "Tests/RPGDemoLifecycleTestHarness.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "AnimInstances/Hero/RPGDemoHeroAnimInstance.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Combat/RPGDemoCombatLog.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "GameModes/RPGDemoSurvivalGameMode.h"
#include "Items/Weapons/RPGDemoHeroWeapon.h"
#include "Items/Weapons/RPGDemoWeaponBase.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "Net/UnrealNetwork.h"
#include "RPGDemoGameplayTags.h"

namespace
{
    const TCHAR* CaseNames[] = {TEXT("GuardianNaturalAI_P1"), TEXT("GuardianMelee_P2"), TEXT("GlacerMelee_P1"),
        TEXT("GiantMelee1_P1"), TEXT("GiantMelee2_P2"), TEXT("GiantMelee3_P1"), TEXT("GlacerProjectile_P2"),
        TEXT("GuardianBlocked"), TEXT("GuardianUnblockable"), TEXT("GuardianCancelled"),
        TEXT("HeroLight_P1"), TEXT("HeroHeavy_P2"), TEXT("DeathRespawnWeapon"), TEXT("DisconnectWeapon")};
    const TCHAR* Guardian = TEXT("/Game/EnemyCharacter/Gruntling/Guardian/BP_Gruntling_Guardian.BP_Gruntling_Guardian_C");
    const TCHAR* Glacer = TEXT("/Game/EnemyCharacter/Gruntling/Glacer/BP_Gruntling_Glacer.BP_Gruntling_Glacer_C");
    const TCHAR* Giant = TEXT("/Game/EnemyCharacter/FrostGiant/BP_FrostGiant.BP_FrostGiant_C");
}

ARPGDemoCombatTestHarness::ARPGDemoCombatTestHarness()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ARPGDemoCombatTestHarness::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARPGDemoCombatTestHarness, CaseIndex);
    DOREPLIFETIME(ARPGDemoCombatTestHarness, Fixture);
}

bool URPGDemoCombatTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if !UE_BUILD_SHIPPING
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
        && (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoCombatTest")) || FParse::Param(FCommandLine::Get(), TEXT("RPGDemoAITest")) || FParse::Param(FCommandLine::Get(), TEXT("RPGDemoLifecycleTest"))) && Super::ShouldCreateSubsystem(Outer);
#else
    return false;
#endif
}

void URPGDemoCombatTestSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    // Keep disposable test processes bounded without a long recursive DEFER
    // command line. This ticker is installed only with the explicit test flag.
    static bool bTestExitInstalled = false;
    if (!bTestExitInstalled)
    {
        bTestExitInstalled = true;
        float Timeout = 180.f;
        FParse::Value(FCommandLine::Get(), TEXT("RPGDemoCombatTestTimeout="), Timeout);
        const double ExitAt = FPlatformTime::Seconds() + FMath::Clamp(Timeout, 30.f, 600.f);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ExitAt](float)
        {
            if (FPlatformTime::Seconds() < ExitAt) return true;
            UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST bounded process exit"));
            FPlatformMisc::RequestExit(false);
            return false;
        }));
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoLifecycleTest")))
    {
        ARPGDemoLifecycleTestHarness::InitializeTestWorld(World);
        return;
    }
    if (World.GetNetMode() != NM_Client && World.GetMapName().Contains(TEXT("SurvivalGameModeMap")))
    {
        if (AGameModeBase* Mode = World.GetAuthGameMode()) Mode->SetActorTickEnabled(false);
        if (FParse::Param(FCommandLine::Get(), TEXT("RPGDemoAITest"))) World.SpawnActor<ARPGDemoAITestHarness>();
        else World.SpawnActor<ARPGDemoCombatTestHarness>();
        UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST harnessCreated map=%s netMode=%d"), *World.GetMapName(), World.GetNetMode());
    }
}

ARPGDemoHeroCharacter* ARPGDemoCombatTestHarness::FindHero(int32 PlayerRole) const
{
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        const APlayerState* State = PC ? PC->PlayerState : nullptr;
        if (State && (GetNetMode() == NM_Standalone || State->GetPlayerName() == FString::Printf(TEXT("LocalClient%d"), PlayerRole)))
            return Cast<ARPGDemoHeroCharacter>(PC->GetPawn());
    }
    return nullptr;
}

void ARPGDemoCombatTestHarness::PrepareHero(ARPGDemoHeroCharacter* Hero, FVector Location, bool bRestoreHealth)
{
    if (!Hero) return;
    Hero->TeleportTo(Location, FRotator::ZeroRotator);
    Hero->GetCharacterMovement()->StopMovementImmediately();
    if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
    {
        PC->SetControlRotation(FRotator::ZeroRotator);
        PC->ClientSetRotation(FRotator::ZeroRotator, true);
    }
    if (bRestoreHealth)
    {
        auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 1000.f);
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 1000.f);
    }
}

void ARPGDemoCombatTestHarness::DestroyFixture()
{
    if (Fixture) Fixture->Destroy();
    Fixture = nullptr;
    AttackClass = nullptr;
}

void ARPGDemoCombatTestHarness::BeginCase()
{
    DestroyFixture();
    bCaseFinished = false;
    bActivated = false;
    bFixtureReady = false;
    CaseStarted = GetWorld()->GetRealTimeSeconds();
    const int32 PlayerRole = (CaseIndex == 1 || CaseIndex == 4 || CaseIndex == 6 || CaseIndex == 11) ? 2 : 1;
    ARPGDemoHeroCharacter* Hero = FindHero(PlayerRole);
    PrepareHero(FindHero(1), Origin + (PlayerRole == 1 ? FVector::ZeroVector : FVector(0, 1800, 0)));
    if (!bSinglePlayer) PrepareHero(FindHero(2), Origin + (PlayerRole == 2 ? FVector::ZeroVector : FVector(0, 1800, 0)));
    if (Hero) PrepareHero(Hero, Origin);
    if (CaseIndex == 13)
    {
        UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST awaitingDisconnect abrupt=%d"), bAbruptDisconnect);
        ForceNetUpdate();
        return;
    }
    const TCHAR* EnemyPath = (CaseIndex == 2 || CaseIndex == 6) ? Glacer : (CaseIndex >= 3 && CaseIndex <= 5) ? Giant : Guardian;
    UClass* EnemyClass = LoadClass<ARPGDemoEnemyCharacter>(nullptr, EnemyPath);
    if (!EnemyClass || !Hero) { FinishCase(false, TEXT("missing class or pawn")); return; }
    const auto* EnemyCDO = Cast<ARPGDemoEnemyCharacter>(EnemyClass->GetDefaultObject());
    const float Distance = CaseIndex == 6 ? 650.f : (CaseIndex >= 3 && CaseIndex <= 5) ? 270.f : 140.f;
    FVector SpawnLocation = Origin + FVector(Distance, 0, EnemyCDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Hero->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Fixture = GetWorld()->SpawnActor<ARPGDemoEnemyCharacter>(EnemyClass, SpawnLocation, FRotator(0, 180, 0), Params);
    if (!Fixture) { FinishCase(false, TEXT("spawn failed")); return; }
    if (CaseIndex != 0)
        if (AAIController* AI = Cast<AAIController>(Fixture->GetController()))
            if (AI->BrainComponent) AI->BrainComponent->StopLogic(TEXT("Explicit montage collision test"));
    if (AAIController* AI = Cast<AAIController>(Fixture->GetController()))
        if (UBlackboardComponent* BB = AI->GetBlackboardComponent()) BB->SetValueAsObject(TEXT("TargetActor"), Hero);
    auto* ASC = Fixture->GetRPGDemoAbilitySystemComponent();
    ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 10000.f);
    ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 10000.f);
    if (CaseIndex == 7) Hero->GetRPGDemoAbilitySystemComponent()->AddLooseGameplayTag(RPGDemoGameplayTags::Player_Status_Block);
    if (CaseIndex == 8)
    {
        Hero->GetRPGDemoAbilitySystemComponent()->AddLooseGameplayTag(RPGDemoGameplayTags::Player_Status_Block);
        ASC->AddLooseGameplayTag(RPGDemoGameplayTags::Enemy_Status_Unblockable);
    }
    FString AbilityPath;
    if (CaseIndex >= 3 && CaseIndex <= 5)
        AbilityPath = FString::Printf(TEXT("/Game/EnemyCharacter/FrostGiant/GameplayAbility/GA_Giant_MeleeAttack_%d.GA_Giant_MeleeAttack_%d_C"), CaseIndex - 2, CaseIndex - 2);
    else if (CaseIndex == 2) AbilityPath = TEXT("/Game/EnemyCharacter/Gruntling/GameplayAbility/GA_Glacer_Melee_1.GA_Glacer_Melee_1_C");
    else if (CaseIndex == 6) AbilityPath = TEXT("/Game/EnemyCharacter/Gruntling/GameplayAbility/GA_Glacer_Projectile.GA_Glacer_Projectile_C");
    else AbilityPath = TEXT("/Game/EnemyCharacter/Gruntling/GameplayAbility/GA_Guardian_Melee_1.GA_Guardian_Melee_1_C");
    AttackClass = LoadClass<UGameplayAbility>(nullptr, *AbilityPath);
    if (CaseIndex == 12)
    {
        OldPawn = Hero;
        Hero->GetRPGDemoAbilitySystemComponent()->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 1.f);
    }
    HealthBefore = CaseIndex >= 10 && CaseIndex <= 11 ? Fixture->GetRPGDemoAttributeSet()->GetCurrentHealth() : Hero->GetRPGDemoAttributeSet()->GetCurrentHealth();
    UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST begin case=%s index=%d health=%.3f"), CaseNames[CaseIndex], CaseIndex, HealthBefore);
    ForceNetUpdate();
}

void ARPGDemoCombatTestHarness::FinishCase(bool bPassed, const TCHAR* Reason)
{
    if (bCaseFinished) return;
    bCaseFinished = true;
    Failures += !bPassed;
    if (ARPGDemoHeroCharacter* Hero = FindHero(1)) Hero->GetRPGDemoAbilitySystemComponent()->RemoveLooseGameplayTag(RPGDemoGameplayTags::Player_Status_Block);
    UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST %s case=%s reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), CaseNames[CaseIndex], Reason);
    NextCaseAt = GetWorld()->GetRealTimeSeconds() + 1.2;
}

void ARPGDemoCombatTestHarness::TickServer()
{
    bSinglePlayer = GetNetMode() == NM_Standalone;
    bDisconnectOnly = FParse::Param(FCommandLine::Get(), TEXT("RPGDemoCombatDisconnectOnly"));
    bAbruptDisconnect = FParse::Param(FCommandLine::Get(), TEXT("RPGDemoCombatAbrupt"));
    const double Now = GetWorld()->GetRealTimeSeconds();
    if (CaseIndex < 0)
    {
        if (Now >= NextWaitingAuditAt)
        {
            NextWaitingAuditAt = Now + 2.0;
            for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PC = It->Get();
                ARPGDemoHeroCharacter* Candidate = PC ? Cast<ARPGDemoHeroCharacter>(PC->GetPawn()) : nullptr;
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST waiting name=%s pawn=%s carried=%d"),
                    PC && PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("None"), *GetNameSafe(Candidate),
                    Candidate && Candidate->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe) != nullptr);
            }
        }
        if (!FindHero(1) || (!bSinglePlayer && !FindHero(2))) return;
        if (!FindHero(1)->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe)) return;
        Origin = FindHero(1)->GetActorLocation();
        if (ARPGDemoHeroWeapon* Weapon = FindHero(1)->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe))
        {
            FActorSpawnParameters Params;
            Params.Owner = FindHero(1);
            Params.Instigator = FindHero(1);
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Duplicate = GetWorld()->SpawnActor<ARPGDemoHeroWeapon>(Weapon->GetClass(), Origin, FRotator::ZeroRotator, Params);
            FindHero(1)->GetHeroCombatComponent()->RegisterSpawnedWeapon(RPGDemoGameplayTags::Player_Weapon_Axe, Duplicate, false);
            const bool bCleaned = !IsValid(Duplicate) && FindHero(1)->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe) == Weapon;
            UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST %s case=DuplicateWeapon reason=original preserved and duplicate destroyed"), bCleaned ? TEXT("PASS") : TEXT("FAIL"));
            Failures += !bCleaned;
        }
        // Remove pre-existing wave actors only in the explicit disposable test.
        for (TActorIterator<ARPGDemoEnemyCharacter> It(GetWorld()); It; ++It)
        {
            It->OnDestroyed.RemoveAll(GetWorld()->GetAuthGameMode());
            It->Destroy();
        }
        CaseIndex = bDisconnectOnly ? 13 : 0;
        BeginCase();
        return;
    }
    if (bCaseFinished)
    {
        if (CaseIndex == 10 && !bActivated && Fixture && Now >= NextCaseAt - 1.0)
        {
            bActivated = true;
            bool bInterrupted = false;
            auto* FixtureASC = Fixture->GetRPGDemoAbilitySystemComponent();
            for (const FGameplayAbilitySpec& Spec : FixtureASC->GetActivatableAbilities())
            {
                if (Spec.IsActive() && Spec.Ability && Spec.Ability->GetAssetTags().HasTag(RPGDemoGameplayTags::Shared_Ability_HitReact))
                {
                    FixtureASC->CancelAbilityHandle(Spec.Handle);
                    bInterrupted = true;
                    break;
                }
            }
            UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST %s case=HitReactInterrupted reason=active enemy reaction cancelled"), bInterrupted ? TEXT("PASS") : TEXT("FAIL"));
            Failures += !bInterrupted;
        }
        if (Now < NextCaseAt) return;
        ++CaseIndex;
        if (bSinglePlayer)
        {
            while (CaseIndex == 1 || CaseIndex == 4 || CaseIndex == 6 || CaseIndex == 11) ++CaseIndex;
            if (CaseIndex >= 12)
            {
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST COMPLETE singlePlayer=1 failures=%d"), Failures);
                DestroyFixture();
                GEngine->Exec(GetWorld(), TEXT("quit"));
                return;
            }
        }
        if (CaseIndex >= UE_ARRAY_COUNT(CaseNames))
        {
            UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST COMPLETE singlePlayer=0 failures=%d"), Failures);
            DestroyFixture();
            if (auto* Mode = GetWorld()->GetAuthGameMode<ARPGDemoSurvivalGameMode>()) Mode->ReturnAllPlayersToLobby();
            SetActorTickEnabled(false);
            return;
        }
        BeginCase();
        return;
    }
    if (CaseIndex == 13)
    {
        if (FindHero(2))
        {
            if (Now - CaseStarted > 30) FinishCase(false, TEXT("disconnect timeout"));
            return;
        }
        int32 Weapons = 0;
        bool bOrphan = false;
        for (TActorIterator<ARPGDemoHeroWeapon> It(GetWorld()); It; ++It)
        {
            ++Weapons;
            bOrphan |= It->GetOwner() != FindHero(1);
        }
        FinishCase(Weapons == 1 && !bOrphan, TEXT("remaining player owns exactly one weapon"));
        return;
    }
    if (!Fixture) { FinishCase(false, TEXT("fixture vanished")); return; }
    const int32 PlayerRole = (CaseIndex == 1 || CaseIndex == 4 || CaseIndex == 6 || CaseIndex == 11) ? 2 : 1;
    ARPGDemoHeroCharacter* Hero = FindHero(PlayerRole);
    if (CaseIndex == 12 && Hero && Hero != OldPawn.Get())
    {
        const bool bWeapon = Hero->GetHeroCombatComponent()->GetHeroCarriedWeaponByTag(RPGDemoGameplayTags::Player_Weapon_Axe) != nullptr;
        if (bWeapon) FinishCase(!OldPawn.IsValid(), TEXT("old pawn destroyed and new pawn weapon registered"));
        return;
    }
    if (!Hero && CaseIndex != 12) { FinishCase(false, TEXT("player pawn vanished")); return; }
    auto* ASC = Fixture->GetRPGDemoAbilitySystemComponent();
    if (CaseIndex == 0 && Now >= NextWaitingAuditAt)
    {
        NextWaitingAuditAt = Now + 2.0;
        AAIController* AI = Cast<AAIController>(Fixture->GetController());
        UBlackboardComponent* BB = AI ? AI->GetBlackboardComponent() : nullptr;
        UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST naturalAI controller=%s running=%d target=%s distance=%.1f blackboardDistance=%.1f"),
            *GetNameSafe(AI), AI && AI->BrainComponent && AI->BrainComponent->IsRunning(),
            *GetNameSafe(BB ? BB->GetValueAsObject(TEXT("TargetActor")) : nullptr), Hero ? FVector::Dist(Fixture->GetActorLocation(), Hero->GetActorLocation()) : -1.f,
            BB ? BB->GetValueAsFloat(TEXT("DistToTarget")) : -1.f);
    }
    if (!bFixtureReady && AttackClass && ASC->FindAbilitySpecFromClass(AttackClass))
    {
        bFixtureReady = true;
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 10000.f);
        ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 10000.f);
        if (CaseIndex == 10 || CaseIndex == 11) HealthBefore = 10000.f;
    }
    if ((CaseIndex != 0 && CaseIndex < 10) || CaseIndex == 12)
    {
        if (!bActivated && Now - CaseStarted > 0.7 && AttackClass && ASC->FindAbilitySpecFromClass(AttackClass))
        {
            bActivated = ASC->TryActivateAbilityByClass(AttackClass);
            if (CaseIndex == 9 && bActivated)
            {
                ASC->CancelAbilityHandle(ASC->FindAbilitySpecFromClass(AttackClass)->Handle);
            }
        }
    }
    if (bFixtureReady && CaseIndex >= 10 && CaseIndex <= 11 && Fixture->GetRPGDemoAttributeSet()->GetCurrentHealth() < HealthBefore)
        FinishCase(true, TEXT("real owning-client melee caused authoritative damage"));
    else if (CaseIndex == 7 && bActivated && Now - CaseStarted > 3.0)
        FinishCase(Hero->GetRPGDemoAttributeSet()->GetCurrentHealth() == HealthBefore, TEXT("facing block prevented damage"));
    else if (CaseIndex == 9 && bActivated && Now - CaseStarted > 3.0)
    {
        const auto* Combat = Fixture->GetPawnCombatComponent();
        const auto* Weapon = Combat ? Combat->GetCharacterCurrentlyEquippedWeapon() : nullptr;
        const bool bClosed = (!Weapon || Weapon->GetWeaponCollisionBox()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
            && Fixture->GetLeftHandCollisionBox()->GetCollisionEnabled() == ECollisionEnabled::NoCollision
            && Fixture->GetRightHandCollisionBox()->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
        FinishCase(bClosed && Hero->GetRPGDemoAttributeSet()->GetCurrentHealth() == HealthBefore, TEXT("cancelled ability leaves all damage collision closed"));
    }
    else if (CaseIndex < 10 && CaseIndex != 7 && CaseIndex != 9 && Hero && Hero->GetRPGDemoAttributeSet()->GetCurrentHealth() < HealthBefore)
        FinishCase(true, TEXT("real enemy montage/collision damaged player"));
    if (Now - CaseStarted > (CaseIndex == 12 ? 20.0 : 12.0)) FinishCase(false, TEXT("no expected result before deadline"));
}

void ARPGDemoCombatTestHarness::TickClient()
{
    if (CaseIndex < 0 || CaseIndex >= UE_ARRAY_COUNT(CaseNames)) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->IsLocalController()) return;
    ARPGDemoHeroCharacter* Hero = Cast<ARPGDemoHeroCharacter>(PC->GetPawn());
    int32 PlayerRole = 1;
    FParse::Value(FCommandLine::Get(), TEXT("RPGDemoCombatRole="), PlayerRole);
    if (LastClientCase != CaseIndex)
    {
        LastClientCase = CaseIndex;
        ClientCaseStarted = GetWorld()->GetRealTimeSeconds();
        bClientEquipped = bClientAttacked = bClientSawFlash = bClientSawShake = bClientSawHitStop = bClientSawHitReact = false;
        bClientCheckedPose = bClientSawFlashEnd = bClientSawMovement = false;
        PoseSampleAt = 0.0;
        ClientStartLocation = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
        UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST clientCase index=%d role=%d"), CaseIndex, PlayerRole);
    }
    if (Hero)
    {
        // The Guardian can back away before its melee branch. Follow through
        // real player input so this case tests its behavior tree and not a
        // stationary target that happens to remain outside melee range.
        if (CaseIndex == 0 && PlayerRole == 1 && Fixture && FVector::Dist(Hero->GetActorLocation(), Fixture->GetActorLocation()) > 145.f)
            Hero->AddMovementInput((Fixture->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D(), 1.f);
        auto* ASC = Hero->GetRPGDemoAbilitySystemComponent();
        if (((CaseIndex == 10 && PlayerRole == 1) || (CaseIndex == 11 && PlayerRole == 2)) && ASC)
        {
            if (!bClientEquipped)
            {
                bClientEquipped = true;
                ASC->TryActivateAbilitiesByTag(RPGDemoGameplayTags::Player_Ability_Equip_Axe.GetTag().GetSingleTagContainer());
            }
            if (!bClientAttacked && GetWorld()->GetRealTimeSeconds() - ClientCaseStarted > 1.0)
            {
                bClientAttacked = ASC->TryActivateAbilitiesByTag((CaseIndex == 10 ? RPGDemoGameplayTags::Player_Ability_LightAttack_Axe : RPGDemoGameplayTags::Player_Ability_HeavyAttack_Axe).GetTag().GetSingleTagContainer());
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST ownerAttack role=%d accepted=%d"), PlayerRole, bClientAttacked);
            }
        }
        if ((CaseIndex == 10 && PlayerRole == 2) || (CaseIndex == 11 && PlayerRole == 1))
        {
            Hero->AddMovementInput(FVector(0, 1, 0), 0.25f);
            if (!bClientSawMovement && FVector::Dist(Hero->GetActorLocation(), ClientStartLocation) > 20.f)
            {
                bClientSawMovement = true;
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST otherPlayerMovement index=%d role=%d distance=%.1f timeDilation=%.3f"), CaseIndex, PlayerRole,
                    FVector::Dist(Hero->GetActorLocation(), ClientStartLocation), GetWorld()->GetWorldSettings()->TimeDilation);
            }
        }
        if (auto* Anim = Cast<URPGDemoHeroAnimInstance>(Hero->GetMesh()->GetAnimInstance()))
        {
            if (!bClientSawHitStop && Anim->GetLocalHitStopAlpha() > 0.f)
            {
                bClientSawHitStop = true;
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST hitStopObserved index=%d role=%d alpha=%.3f timeDilation=%.3f"), CaseIndex, PlayerRole, Anim->GetLocalHitStopAlpha(), GetWorld()->GetWorldSettings()->TimeDilation);
            }
            if (Anim->GetLocalHitStopAlpha() >= 0.99f && !bClientCheckedPose)
            {
                const double SampleNow = GetWorld()->GetRealTimeSeconds();
                const UAnimMontage* Montage = Anim->GetCurrentActiveMontage();
                const FTransform HandPose = Hero->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_Component);
                if (PoseSampleAt == 0.0)
                {
                    PoseSampleAt = SampleNow;
                    PoseSampleTransform = HandPose;
                    PoseSampleMontagePosition = Montage ? Anim->Montage_GetPosition(Montage) : 0.f;
                }
                else if (SampleNow - PoseSampleAt >= 0.06)
                {
                    bClientCheckedPose = true;
                    const float Position = Montage ? Anim->Montage_GetPosition(Montage) : 0.f;
                    UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST frozenPoseWithLiveMontage index=%d role=%d frozen=%d montageAdvanced=%d elapsed=%.3f"),
                        CaseIndex, PlayerRole, PoseSampleTransform.Equals(HandPose, 0.1f), Position > PoseSampleMontagePosition + 0.01f, SampleNow - PoseSampleAt);
                }
            }
            if (!bClientSawHitReact && Anim->GetCurrentActiveMontage() && Anim->GetCurrentActiveMontage()->GetName().Contains(TEXT("HitReact")))
            {
                bClientSawHitReact = true;
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST playerHitReactObserved index=%d role=%d"), CaseIndex, PlayerRole);
            }
        }
        if (!bClientSawShake && PC->PlayerCameraManager)
        {
            if (auto* Modifier = Cast<UCameraModifier_CameraShake>(PC->PlayerCameraManager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass())))
            {
                TArray<FActiveCameraShakeInfo> Shakes;
                Modifier->GetActiveCameraShakes(Shakes);
                if (!Shakes.IsEmpty())
                {
                    bClientSawShake = true;
                    UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST cameraShakeObserved index=%d role=%d count=%d"), CaseIndex, PlayerRole, Shakes.Num());
                }
            }
        }
    }
    if (Fixture && !bClientSawFlash)
    {
        for (UMaterialInterface* Material : Fixture->GetMesh()->GetMaterials())
        {
            float Value = 0.f;
            if (Material && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("HitFxSwitch")), Value) && Value > 0.5f)
            {
                bClientSawFlash = true;
                UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST enemyFlashObserved index=%d role=%d value=%.1f"), CaseIndex, PlayerRole, Value);
                FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("CombatCase%d-Client%d.png"), CaseIndex, PlayerRole), true, false);
                break;
            }
        }
    }
    if (Fixture && bClientSawFlash && !bClientSawFlashEnd)
    {
        float Value = 1.f;
        UMaterialInterface* Material = Fixture->GetMesh()->GetMaterial(0);
        if (Material && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("HitFxSwitch")), Value) && Value < 0.5f)
        {
            bClientSawFlashEnd = true;
            UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST enemyFlashRestored index=%d role=%d value=%.1f"), CaseIndex, PlayerRole, Value);
        }
    }
    if (CaseIndex == 13 && PlayerRole == 2 && !bClientRequestedDisconnect)
    {
        bClientRequestedDisconnect = true;
        if (!FParse::Param(FCommandLine::Get(), TEXT("RPGDemoCombatAbrupt"))) GEngine->Exec(GetWorld(), TEXT("quit"));
        else UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST readyForAbruptDisconnect"));
    }
    if (CaseIndex == 13 && PlayerRole == 1 && !bClientRequestedDisconnect && Hero)
    {
        int32 Pawns = 0;
        int32 Weapons = 0;
        bool bOrphan = false;
        for (TActorIterator<ARPGDemoHeroCharacter> It(GetWorld()); It; ++It) ++Pawns;
        for (TActorIterator<ARPGDemoHeroWeapon> It(GetWorld()); It; ++It)
        {
            ++Weapons;
            bOrphan |= It->GetOwner() != Hero;
        }
        if (Pawns == 1)
        {
            bClientRequestedDisconnect = true;
            UE_LOG(LogRPGDemoCombat, Log, TEXT("TEST clientWeaponCleanup role=1 weapons=%d orphan=%d"), Weapons, bOrphan);
        }
    }
}

void ARPGDemoCombatTestHarness::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority()) TickServer();
    if (GetNetMode() != NM_DedicatedServer) TickClient();
    if (!FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->TimeDilation, 1.f))
        UE_LOG(LogRPGDemoCombat, Error, TEXT("TEST global time dilation changed: %.3f"), GetWorld()->GetWorldSettings()->TimeDilation);
}
