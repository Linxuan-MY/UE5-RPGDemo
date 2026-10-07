#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Controllers/RPGDemoAIController.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "RPGDemoGameplayTags.h"

// Exercise the real perception component and Guardian tree on the arena's navmesh.
// Only the disposable PIE world is changed; editor assets and AI tuning are preserved.
class FRPGDemoApproachStrafeCommand : public IAutomationLatentCommand
{
public:
 explicit FRPGDemoApproachStrafeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
 virtual ~FRPGDemoApproachStrafeCommand() override
 {
  if (!bRequestedPIE || !GEditor) return;
  GEditor->RequestEndPlayMap();
  auto* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
  Defaults->SetPlayNetMode(PreviousNetMode);
  Defaults->SetRunUnderOneProcess(bPreviousOneProcess);
  Defaults->SetPlayNumberOfClients(PreviousClientCount);
  Defaults->bLaunchSeparateServer = bPreviousSeparateServer;
  Defaults->SaveConfig();
 }

 virtual bool Update() override
 {
  if (!bRequestedPIE)
  {
   const auto* Defaults = GetDefault<ULevelEditorPlaySettings>();
   Defaults->GetPlayNetMode(PreviousNetMode);
   Defaults->GetRunUnderOneProcess(bPreviousOneProcess);
   Defaults->GetPlayNumberOfClients(PreviousClientCount);
   bPreviousSeparateServer = Defaults->bLaunchSeparateServer;
   Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
   Settings->SetPlayNetMode(PIE_Standalone);
   Settings->SetRunUnderOneProcess(true);
   Settings->SetPlayNumberOfClients(1);
   Settings->bLaunchSeparateServer = false;
   FRequestPlaySessionParams Params;
   Params.EditorPlaySettings = Settings.Get();
   Params.GlobalMapOverride = TEXT("/Game/Maps/SurvivalGameModeMap");
   GEditor->RequestPlaySession(Params);
   bRequestedPIE = true;
   Started = FPlatformTime::Seconds();
   return false;
  }
  if (FPlatformTime::Seconds() - Started > 45.0)
  {
   Test->AddError(FString::Printf(TEXT("Approach/Strafe timed out at stage %d; distance %.1f, target losses %d, move restarts %d."),
    Stage, Enemy.IsValid() && Hero.IsValid() ? Enemy->GetDistanceTo(Hero.Get()) : -1.f, TargetLosses, MoveRestarts));
   return true;
  }
  UWorld* World = nullptr;
  for (const FWorldContext& Context : GEngine->GetWorldContexts())
   if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Standalone)
    World = Context.World();
  if (!World) return false;

  if (Stage == 0)
  {
   // Prevent unrelated wave spawns from affecting the isolated navigation scenario.
   if (auto* GameMode = World->GetAuthGameMode()) GameMode->SetActorTickEnabled(false);
   for (TActorIterator<ARPGDemoHeroCharacter> It(World); It; ++It)
    if (It->IsPlayerControlled()) Hero = *It;
   if (!Hero.IsValid()) return false;
   auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
   const auto* NavData = Nav ? Nav->GetDefaultNavDataInstance() : nullptr;
   if (!NavData) return false;
   FNavLocation Goal;
   if (!Nav->ProjectPointToNavigation(Hero->GetActorLocation(), Goal, FVector(100, 100, 300))) return false;
   const FVector HeroLocation = Goal.Location + FVector(0, 0, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
   FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ApproachStrafeFixture), false, Hero.Get());
   bool bFoundSpawn = false;
   for (int32 Index = 0; Index < 16 && !bFoundSpawn; ++Index)
   {
    const float Angle = Index * 2.f * PI / 16.f;
    FNavLocation Candidate;
    if (!Nav->ProjectPointToNavigation(Goal.Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * 1800.f,
     Candidate, FVector(250, 250, 300))) continue;
    const float Distance = FVector::Dist(Candidate.Location, Goal.Location);
    if (Distance < 1200.f || Distance > 2200.f) continue;
    FPathFindingQuery Query(Hero.Get(), *NavData, Candidate.Location, Goal.Location);
    const FPathFindingResult Path = Nav->FindPathSync(Query);
    if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial() || Path.Path->GetLength() > Distance * 1.15f) continue;
    EnemyStart = Candidate.Location + FVector(0, 0, 100);
    if (World->LineTraceTestByChannel(EnemyStart, HeroLocation, ECC_Visibility, TraceParams)) continue;
    bFoundSpawn = true;
   }
   if (!Test->TestTrue(TEXT("Arena has a visible navigable approach longer than 1200 cm"), bFoundSpawn)) return true;
   Hero->GetCharacterMovement()->DisableMovement();
   Hero->SetActorLocation(HeroLocation);
   for (TActorIterator<ARPGDemoEnemyCharacter> It(World); It; ++It) It->Destroy();
   UClass* EnemyClass = LoadClass<ARPGDemoEnemyCharacter>(nullptr,
    TEXT("/Game/EnemyCharacter/Gruntling/Guardian/BP_Gruntling_Guardian.BP_Gruntling_Guardian_C"));
   if (!Test->TestNotNull(TEXT("Actual Guardian Blueprint loads"), EnemyClass)) return true;
   FActorSpawnParameters Spawn;
   Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Spawn.Name = TEXT("ApproachStrafeRegressionGuardian");
   Enemy = World->SpawnActor<ARPGDemoEnemyCharacter>(EnemyClass, EnemyStart,
    (HeroLocation - EnemyStart).Rotation(), Spawn);
   if (!Test->TestNotNull(TEXT("Actual Guardian spawns"), Enemy.Get())) return true;
   Stage = 1;
   return false;
  }

  if (!Enemy.IsValid() || !Hero.IsValid())
  {
   Test->AddError(TEXT("Approach/Strafe fixture lost its enemy or player."));
   return true;
  }
  auto* AI = Cast<ARPGDemoAIController>(Enemy->GetController());
  auto* Blackboard = AI ? AI->GetBlackboardComponent() : nullptr;
  if (!Blackboard) return false;
  const float Distance = Enemy->GetDistanceTo(Hero.Get());
  const bool bStrafing = Enemy->GetRPGDemoAbilitySystemComponent()->HasMatchingGameplayTag(RPGDemoGameplayTags::Enemy_Status_Strafe);
  if (Stage == 1)
  {
   if (Enemy->GetRPGDemoAbilitySystemComponent()->GetNumericAttribute(URPGDemoAttributeSet::GetMaxHealthAttribute()) <= 0.f ||
    Blackboard->GetValueAsObject(TEXT("TargetActor")) != Hero.Get() ||
    AI->GetMoveStatus() != EPathFollowingStatus::Moving || Enemy->GetVelocity().Size2D() < 50.f) return false;
   // A queued Move To is already "Moving" while the crowd manager initializes.
   // Measure interruptions only after the pawn has actually started its approach.
   InitialDistance = Distance;
   PhaseStarted = World->GetTimeSeconds();
   MoveID = AI->GetCurrentMoveRequestID();
   Stage = 2;
   return false;
  }
  if (Stage == 2 && Distance > 650.f)
  {
   if (Blackboard->GetValueAsObject(TEXT("TargetActor")) != Hero.Get()) ++TargetLosses;
   if (bStrafing) ++FarStrafeFrames;
   if (AI->GetCurrentMoveRequestID() != MoveID)
   {
    ++MoveRestarts;
    MoveID = AI->GetCurrentMoveRequestID();
   }
   if (World->GetTimeSeconds() - PhaseStarted > .5f)
   {
    IdleTime = Enemy->GetVelocity().Size2D() < 5.f ? IdleTime + World->GetDeltaSeconds() : 0.f;
    MaxIdleTime = FMath::Max(MaxIdleTime, IdleTime);
   }
   return false;
  }
  if (Stage == 2)
  {
   Test->TestEqual(TEXT("Far approach retains the perceived player every frame"), TargetLosses, 0);
   Test->TestEqual(TEXT("Far approach uses one uninterrupted Move To request"), MoveRestarts, 0);
   Test->TestEqual(TEXT("Strafe never runs outside its distance threshold"), FarStrafeFrames, 0);
   Test->TestTrue(TEXT("Far approach has no stop lasting 0.3 seconds"), MaxIdleTime < .3f);
   Test->AddInfo(FString::Printf(TEXT("Guardian approached %.1f -> %.1f cm; target losses=%d, move restarts=%d, longest stop=%.3fs."),
    InitialDistance, Distance, TargetLosses, MoveRestarts, MaxIdleTime));
   Stage = 3;
   PhaseStarted = World->GetTimeSeconds();
  }
  if (Stage == 3)
  {
   if (!bStrafing) return false;
   Test->TestTrue(TEXT("Actual Guardian enters Strafe within 600 cm"), Distance <= 600.f);
   Test->AddInfo(FString::Printf(TEXT("Guardian entered Strafe at %.1f cm."), Distance));
   // A retreating player must cause the existing Both-abort decorator to resume approach.
   Hero->SetActorLocation(EnemyStart);
   RetreatDistance = Enemy->GetDistanceTo(Hero.Get());
   PhaseStarted = World->GetTimeSeconds();
   Stage = 4;
   return false;
  }
  if (Stage == 4 && World->GetTimeSeconds() - PhaseStarted >= 1.f && Distance < RetreatDistance - 200.f)
  {
   Test->TestEqual(TEXT("Player retreat retains the same target"), Blackboard->GetValueAsObject(TEXT("TargetActor")), static_cast<UObject*>(Hero.Get()));
   Test->TestFalse(TEXT("Player retreat disables Strafe outside 600 cm"), bStrafing);
   Test->TestEqual(TEXT("Player retreat resumes continuous approach"), AI->GetMoveStatus(), EPathFollowingStatus::Moving);
   Test->AddInfo(FString::Printf(TEXT("After player retreat, Guardian resumed approach %.1f -> %.1f cm."), RetreatDistance, Distance));
   return true;
  }
  return false;
 }

private:
 FAutomationTestBase* Test;
 bool bRequestedPIE = false;
 EPlayNetMode PreviousNetMode = PIE_Standalone;
 bool bPreviousOneProcess = true, bPreviousSeparateServer = false;
 int32 PreviousClientCount = 1, Stage = 0;
 TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
 TWeakObjectPtr<ARPGDemoHeroCharacter> Hero;
 TWeakObjectPtr<ARPGDemoEnemyCharacter> Enemy;
 FVector EnemyStart = FVector::ZeroVector;
 FAIRequestID MoveID;
 double Started = 0.0;
 float PhaseStarted = 0.f, InitialDistance = 0.f, RetreatDistance = 0.f;
 float IdleTime = 0.f, MaxIdleTime = 0.f;
 int32 TargetLosses = 0, MoveRestarts = 0, FarStrafeFrames = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoApproachStrafeTest, "RPGDemo.AI.ContinuousApproachAndStrafe",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoApproachStrafeTest::RunTest(const FString& Parameters)
{
 if (GEditor->PlayWorld)
 {
  AddError(TEXT("Stop the existing PIE session before running the approach/Strafe regression."));
  return false;
 }
 ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoApproachStrafeCommand(this));
 return true;
}
#endif
