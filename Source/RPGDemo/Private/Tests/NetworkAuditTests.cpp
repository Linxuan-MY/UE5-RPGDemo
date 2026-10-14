#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/BoxComponent.h"
#include "AbilitySystem/RPGDemoAbilitySystemComponent.h"
#include "AbilitySystem/RPGDemoAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "Characters/RPGDemoHeroCharacter.h"
#include "Characters/RPGDemoEnemyCharacter.h"
#include "GameModes/RPGDemoPlayerState.h"
#include "Items/RPGDemoProjectileBase.h"
#include "Items/PickUps/RPGDemoStoneBase.h"
#include "RPGDemoFunctionLibrary.h"
#include "RPGDemoGameInstance.h"
#include "RPGDemoGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Controllers/RPGDemoHeroController.h"
#include "AbilitySystem/Abilities/HeroGameplayAbility_TargetLock.h"
#include "AI/BTTask_RotateToFaceTarget.h"

// Own a disposable single-process dedicated-server PIE session with two NM_Client worlds.
// This deliberately tests real dedicated-server actor channels and the owner's GAS activation RPC.
class FRPGDemoNetworkRegressionCommand : public IAutomationLatentCommand
{
public:
 explicit FRPGDemoNetworkRegressionCommand(FAutomationTestBase* InTest, bool bInBlueprintEnemy = false, int32 InEnemyVariant = 0, bool bInImmediateDeath = false)
  : Test(InTest), Started(FPlatformTime::Seconds()), bBlueprintEnemy(bInBlueprintEnemy), EnemyVariant(InEnemyVariant), bImmediateDeath(bInImmediateDeath)
 {
  EndPIEHandle = FEditorDelegates::PrePIEEnded.AddLambda([this](bool)
  {
   // Also release snapshots if a user/controller stops PIE before the latent command ends.
   Projectile.Reset(); ClientProjectile.Reset(); Stone.Reset(); ClientStone.Reset(); Enemy.Reset(); ClientEnemy.Reset();
  });
 }
 virtual ~FRPGDemoNetworkRegressionCommand() override
 {
  FEditorDelegates::PrePIEEnded.Remove(EndPIEHandle);
  if (DropDefaults.IsValid() && DropChanceProperty)
   *DropChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(DropDefaults.Get()) = OriginalDropChance;
  if (GrantedDropDefaults.IsValid() && GrantedDropChanceProperty)
   *GrantedDropChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(GrantedDropDefaults.Get()) = OriginalGrantedDropChance;
  if (bRequestedPIE && GEditor)
  {
   GEditor->RequestEndPlayMap();
   // PIE copies multiplayer options back into the CDO even with a transient settings object.
   ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
   Defaults->SetPlayNetMode(PreviousNetMode);
   Defaults->SetRunUnderOneProcess(bPreviousOneProcess);
   Defaults->SetPlayNumberOfClients(PreviousClientCount);
   Defaults->bLaunchSeparateServer = bPreviousLaunchSeparateServer;
   Defaults->SaveConfig();
  }
 }
 virtual bool Update() override
 {
  if (!bRequestedPIE)
  {
   const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
   Defaults->GetPlayNetMode(PreviousNetMode);
   Defaults->GetRunUnderOneProcess(bPreviousOneProcess);
   Defaults->GetPlayNumberOfClients(PreviousClientCount);
   bPreviousLaunchSeparateServer = Defaults->bLaunchSeparateServer;
   Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
   Settings->SetPlayNetMode(PIE_Client);
   Settings->SetRunUnderOneProcess(true);
   Settings->SetPlayNumberOfClients(2);
   Settings->bLaunchSeparateServer = true;
   FRequestPlaySessionParams Params;
   Params.EditorPlaySettings = Settings.Get();
   Params.GlobalMapOverride = TEXT("/Game/Maps/CombatTestMap");
   GEditor->RequestPlaySession(Params);
   bRequestedPIE = true;
   Started = FPlatformTime::Seconds();
   return false;
  }
  if (FPlatformTime::Seconds() - Started > 60.0)
  {
   Test->AddError(FString::Printf(TEXT("Network audit timed out at stage %d. The test-owned dedicated server and two clients did not reach the expected state."), Stage));
   Test->AddInfo(FString::Printf(TEXT("Fixture refs: projectile=%d stone=%d enemy=%d; impact=%u consume=%d dissolve=%d"),
    ClientProjectile.IsValid(), ClientStone.IsValid(), ClientEnemy.IsValid(),
    ClientProjectile.Get() ? ClientProjectile->LastImpactSequence : 0,
    ClientStone.Get() ? ClientStone->bLocalConsumptionPresented : false,
    ClientEnemy.Get() ? ClientEnemy->bLocalDissolvePresentationApplied : false));
   return true;
  }
  UWorld* Server = nullptr;
  TArray<UWorld*> Clients;
  for (const FWorldContext& Context : GEngine->GetWorldContexts())
  {
   if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
   if (Context.World()->GetNetMode() == NM_DedicatedServer) Server = Context.World();
   if (Context.World()->GetNetMode() == NM_Client) Clients.Add(Context.World());
  }
  if (!Server || Clients.Num() != 2) return false;
  UWorld* Client = Clients[0];
  if (Stage == 0)
  {
   for (TActorIterator<ARPGDemoHeroCharacter> It(Client); It; ++It)
    if (It->IsLocallyControlled()) ClientHero = *It;
   const ARPGDemoPlayerState* ClientState = ClientHero.IsValid()
    ? ClientHero->GetPlayerState<ARPGDemoPlayerState>() : nullptr;
   if (!ClientState) return false;
   for (TActorIterator<ARPGDemoHeroCharacter> It(Server); It; ++It)
   {
    if (!It->GetController() || !It->GetController()->IsPlayerController()) continue;
    const ARPGDemoPlayerState* ServerState = It->GetPlayerState<ARPGDemoPlayerState>();
    if (ServerState && ServerState->GetPlayerId() == ClientState->GetPlayerId()) Remote = *It;
    else Host = *It;
   }
   if (!Host.IsValid() || !Remote.IsValid() || !ClientHero.IsValid()) return false;
   Test->TestEqual(TEXT("PIE server is dedicated"), Server->GetNetMode(), NM_DedicatedServer);
   Test->TestEqual(TEXT("PIE client count"), Clients.Num(), 2);
   Test->TestEqual(TEXT("Hero ASC owner is PlayerState"), Remote->GetRPGDemoAbilitySystemComponent()->GetOwnerActor(), static_cast<AActor*>(Remote->GetPlayerState()));
   Test->TestEqual(TEXT("Hero ASC avatar is current Pawn"), Remote->GetRPGDemoAbilitySystemComponent()->GetAvatarActor(), static_cast<AActor*>(Remote.Get()));
   auto* ASC = Remote->GetRPGDemoAbilitySystemComponent();
   const float PreviousHealth = ASC->GetNumericAttribute(URPGDemoAttributeSet::GetCurrentHealthAttribute());
   const float PreviousMaxHealth = ASC->GetNumericAttribute(URPGDemoAttributeSet::GetMaxHealthAttribute());
   ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), 100.f);
   ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), 100.f);
   UGameplayEffect* Damage = NewObject<UGameplayEffect>();
   Damage->DurationPolicy = EGameplayEffectDurationType::Instant;
   auto& DamageMod = Damage->Modifiers.AddDefaulted_GetRef();
   DamageMod.Attribute = URPGDemoAttributeSet::GetDamageTakenAttribute();
   DamageMod.ModifierOp = EGameplayModOp::Additive;
   DamageMod.ModifierMagnitude = FScalableFloat(10.f);
   ASC->ApplyGameplayEffectToSelf(Damage, 1.f, ASC->MakeEffectContext());
   ASC->ApplyGameplayEffectToSelf(Damage, 1.f, ASC->MakeEffectContext());
   Test->TestEqual(TEXT("Two additive hits deduct their own damage once"), ASC->GetNumericAttribute(URPGDemoAttributeSet::GetCurrentHealthAttribute()), 80.f);
   Test->TestEqual(TEXT("Persistent ASC does not retain old damage"), ASC->GetNumericAttribute(URPGDemoAttributeSet::GetDamageTakenAttribute()), 0.f);
   ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetMaxHealthAttribute(), PreviousMaxHealth);
   ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentHealthAttribute(), PreviousHealth);
   RageClass = LoadClass<UGameplayAbility>(nullptr, TEXT("/Game/PlayerCharacter/GameplayAbility/GA_Hero_Rage.GA_Hero_Rage_C"));
   if (!Test->TestNotNull(TEXT("Real Rage ability loads"), RageClass)) return true;
   if (!ASC->FindAbilitySpecFromClass(RageClass)) ASC->GiveAbility(FGameplayAbilitySpec(RageClass, 1));
   ASC->SetNumericAttributeBase(URPGDemoAttributeSet::GetCurrentRageAttribute(), 0.f);
   URPGDemoFunctionLibrary::RemoveGameplayTagFromActorIfFound(Remote.Get(), RPGDemoGameplayTags::Player_Status_Rage_Full);
   URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Remote.Get(), RPGDemoGameplayTags::Player_Status_Rage_None);
   UGameplayEffect* Fill = NewObject<UGameplayEffect>();
   Fill->DurationPolicy = EGameplayEffectDurationType::Instant;
   FGameplayModifierInfo& Mod = Fill->Modifiers.AddDefaulted_GetRef();
   Mod.Attribute = URPGDemoAttributeSet::GetCurrentRageAttribute();
   Mod.ModifierOp = EGameplayModOp::Additive;
   Mod.ModifierMagnitude = FScalableFloat(ASC->GetNumericAttribute(URPGDemoAttributeSet::GetMaxRageAttribute()));
   ASC->ApplyGameplayEffectToSelf(Fill, 1.f, ASC->MakeEffectContext());
   Test->TestFalse(TEXT("Zero-to-full clears Rage.None on server"), ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Player_Status_Rage_None));
   Remote->ForceNetUpdate();
   Stage = 1;
   return false;
  }
  if (Stage == 1)
  {
   auto* ASC = ClientHero->GetRPGDemoAbilitySystemComponent();
   FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(RageClass);
   if (!Spec || !ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Player_Status_Rage_Full)) return false;
   Test->TestFalse(TEXT("Owner clears Rage.None after replication"), ASC->HasMatchingGameplayTag(RPGDemoGameplayTags::Player_Status_Rage_None));
   Test->TestTrue(TEXT("Owner can activate real LocalPredicted Rage"), ASC->TryActivateAbility(Spec->Handle));
   Stage = 2;
   return false;
  }
  if (Stage == 2)
  {
   auto* ASC = Remote->GetRPGDemoAbilitySystemComponent();
   const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(RageClass);
   if (!Spec || !Spec->IsActive()) return false;
   Test->TestTrue(TEXT("Server accepted owner's Rage activation"), Spec->IsActive());
   ASC->CancelAbilityHandle(Spec->Handle);
   Host->GetCharacterMovement()->DisableMovement();
   Remote->GetCharacterMovement()->DisableMovement();
   Host->SetActorLocation(FVector(0, 0, 5000));
   Remote->SetActorLocation(FVector(100, 0, 5000));
   FActorSpawnParameters Params;
   Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Params.Name = TEXT("NetworkAuditProjectile");
   Projectile.Reset(Server->SpawnActor<ARPGDemoProjectileBase>(FVector(0, 0, 5300), FRotator::ZeroRotator, Params));
   Projectile->SetLifeSpan(0);
   Projectile->bAlwaysRelevant = true;
   Projectile->ProjectileMovementComp->StopMovementImmediately();
   Projectile->ProjectileMovementComp->Deactivate();
   Projectile->SetActorEnableCollision(false);
   Params.Name = TEXT("NetworkAuditStone");
   Stone.Reset(Server->SpawnActor<ARPGDemoStoneBase>(FVector(50, 0, 5000), FRotator::ZeroRotator, Params));
   Stone->SetActorEnableCollision(false);
   Stone->bAlwaysRelevant = true;
   UClass* StoneClass = LoadClass<ARPGDemoStoneBase>(nullptr, TEXT("/Game/Items/Stones/BP_RageStone.BP_RageStone_C"));
   if (!Test->TestNotNull(TEXT("Rage stone loads"), StoneClass)) return true;
   Stone->StoneGameplayEffectClass = StoneClass->GetDefaultObject<ARPGDemoStoneBase>()->StoneGameplayEffectClass;
   const TCHAR* EnemyPaths[] = {TEXT("/Game/EnemyCharacter/Gruntling/Guardian/BP_Gruntling_Guardian.BP_Gruntling_Guardian_C"),
    TEXT("/Game/EnemyCharacter/Gruntling/Glacer/BP_Gruntling_Glacer.BP_Gruntling_Glacer_C"),
    TEXT("/Game/EnemyCharacter/FrostGiant/BP_FrostGiant.BP_FrostGiant_C")};
   UClass* EnemyClass = bBlueprintEnemy ? LoadClass<ARPGDemoEnemyCharacter>(nullptr, EnemyPaths[EnemyVariant]) : ARPGDemoEnemyCharacter::StaticClass();
   if (!Test->TestNotNull(TEXT("Enemy fixture class loads"), EnemyClass)) return true;
   if (bBlueprintEnemy)
   {
    DropDefaults.Reset(LoadClass<UGameplayAbility>(nullptr,
     TEXT("/Game/Shared/GameplayAbility/GA_Enemy_SpawnStone_Base.GA_Enemy_SpawnStone_Base_C"))->GetDefaultObject());
    DropChanceProperty = FindFProperty<FStructProperty>(DropDefaults->GetClass(), TEXT("StoneSpawnChanceScalableFloat"));
    if (!Test->TestNotNull(TEXT("Original scalable drop probability"), DropChanceProperty)) return true;
    OriginalDropChance = *DropChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(DropDefaults.Get());
    // Make the original random drop deterministic for this disposable session only.
    *DropChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(DropDefaults.Get()) = FScalableFloat(1.f);
    UFunction* Function = EnemyClass->FindFunctionByName(TEXT("OnEnemyDied"));
    FSoftObjectProperty* Input = Function ? FindFProperty<FSoftObjectProperty>(Function, TEXT("DissolveNiagaraSystem")) : nullptr;
    if (!Test->TestNotNull(TEXT("Real OnEnemyDied input is a soft object property"), Input)) return true;
    Test->AddInfo(FString::Printf(TEXT("OnEnemyDied reflected frame=%d input size=%d offset=%d; old guessed frame=%d"),
     Function->ParmsSize, Input->GetSize(), Input->GetOffset_ForInternal(), int32(sizeof(UObject*))));
   }
   Enemy.Reset(Server->SpawnActorDeferred<ARPGDemoEnemyCharacter>(EnemyClass, FTransform(FVector(0, 0, 5500)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
   Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
   Enemy->bAlwaysRelevant = true;
   Enemy->FinishSpawning(FTransform(FVector(0, 0, 5500)));
   Enemy->GetCharacterMovement()->DisableMovement();
   UBTTask_RotateToFaceTarget* RotateTask = NewObject<UBTTask_RotateToFaceTarget>();
   Enemy->SetActorRotation(FRotator::ZeroRotator);
   Test->TestTrue(TEXT("Upright yaw alignment completes regardless of actor center Z"), RotateTask->HasReachedAnglePrecision(Enemy.Get(), Remote.Get()));
   Remote->SetActorLocation(FVector(100, 0, 5500));
   Test->TestTrue(TEXT("Same-height upright yaw alignment completes"), RotateTask->HasReachedAnglePrecision(Enemy.Get(), Remote.Get()));
   Remote->SetActorLocation(FVector(100, 0, 5000));
   Enemy->SetActorRotation(FRotator(0, 180, 0));
   Test->TestFalse(TEXT("Opposite yaw still requires rotation"), RotateTask->HasReachedAnglePrecision(Enemy.Get(), Remote.Get()));
   Enemy->SetActorRotation(FRotator::ZeroRotator);
   if (bImmediateDeath)
   {
    DeathStartLocation = Enemy->GetActorLocation();
    Enemy->BeginReplicatedDeathPresentation();
    URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Enemy.Get(), RPGDemoGameplayTags::Shared_Status_Dead);
   }
   Test->AddInfo(TEXT("Rage owner/server activation passed; waiting for native fixture replicas."));
   Stage = 3;
   return false;
  }
  if (Stage == 3)
  {
   for (TActorIterator<ARPGDemoProjectileBase> It(Client); It; ++It)
    if (It->GetClass() == ARPGDemoProjectileBase::StaticClass()) ClientProjectile.Reset(*It);
   for (TActorIterator<ARPGDemoStoneBase> It(Client); It; ++It)
    if (It->GetClass() == ARPGDemoStoneBase::StaticClass()) ClientStone.Reset(*It);
   for (TActorIterator<ARPGDemoEnemyCharacter> It(Client); It; ++It)
    if (It->GetFName() == Enemy->GetFName()) ClientEnemy.Reset(*It);
   if (!ClientProjectile.IsValid() || !ClientStone.IsValid() || !ClientEnemy.IsValid()) return false;
   if (bBlueprintEnemy && !bImmediateDeath && !Enemy->bEnemyStartUpDataInitialized) return false;
   if (bBlueprintEnemy && !bImmediateDeath && !bGuaranteedGrantedDrop)
   {
    bGuaranteedGrantedDrop = true;
    for (const auto& Spec : Enemy->GetRPGDemoAbilitySystemComponent()->GetActivatableAbilities())
    {
     if (!Spec.Ability || !Spec.Ability->GetAssetTags().HasTagExact(RPGDemoGameplayTags::Enemy_Ability_SpawnStone)) continue;
     Test->AddInfo(FString::Printf(TEXT("Actual drop ability: %s, level=%d"), *Spec.Ability->GetPathName(), Spec.Level));
     if (Spec.Ability == DropDefaults.Get()) break;
     GrantedDropDefaults.Reset(Spec.Ability);
     GrantedDropChanceProperty = FindFProperty<FStructProperty>(Spec.Ability->GetClass(), TEXT("StoneSpawnChanceScalableFloat"));
     if (!Test->TestNotNull(TEXT("Granted scalable drop probability"), GrantedDropChanceProperty)) return true;
     OriginalGrantedDropChance = *GrantedDropChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(GrantedDropDefaults.Get());
     *GrantedDropChanceProperty->ContainerPtrToValuePtr<FScalableFloat>(GrantedDropDefaults.Get()) = FScalableFloat(1.f);
     break;
    }
   }
   if (bBlueprintEnemy)
   {
    if (!bCountdownShown)
    {
     ARPGDemoHeroController* Controller = Cast<ARPGDemoHeroController>(ClientHero->GetController());
     UClass* WidgetClass = LoadClass<UUserWidget>(nullptr,
      TEXT("/Game/Widgets/GameModeWidgets/WBP_WaveTextWithCountDown.WBP_WaveTextWithCountDown_C"));
     if (!Test->TestNotNull(TEXT("Local presentation controller"), Controller) ||
      !Test->TestNotNull(TEXT("Real countdown widget class"), WidgetClass)) return true;
     Controller->ShowCountdownMessage(FText::FromString(TEXT("Countdown regression")), 0.75f);
     TArray<UUserWidget*> Widgets;
     UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Client, Widgets, WidgetClass, true);
     if (!Test->TestEqual(TEXT("One countdown added to client viewport"), Widgets.Num(), 1)) return true;
     CountdownWidget = Widgets[0];
     CountdownStarted = Client->GetTimeSeconds();
     bCountdownShown = true;
     return false;
    }
    if (Client->GetTimeSeconds() - CountdownStarted < 0.2) return false;
    if (!Test->TestNotNull(TEXT("Countdown survives until the first animation sample"), CountdownWidget.Get())) return true;
    UTextBlock* Text = Cast<UTextBlock>(CountdownWidget->GetWidgetFromName(TEXT("TextBlock_CountDownText")));
    Test->TestTrue(TEXT("Real countdown event updates the visible number"), Text && Text->GetText().ToString() == TEXT("1"));
    const UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(CountdownWidget->GetClass());
    bool bAnimationAdvanced = false;
    if (WidgetClass)
     for (UWidgetAnimation* Animation : WidgetClass->Animations)
      bAnimationAdvanced |= CountdownWidget->IsAnimationPlaying(Animation) && CountdownWidget->GetAnimationCurrentTime(Animation) > 0.f;
    Test->TestTrue(TEXT("Wave entry animation plays and advances in the client viewport"), bAnimationAdvanced);
    if (!bImmediateDeath) DeathStartLocation = Enemy->GetActorLocation();
    ClientDeathStartLocation = ClientEnemy->GetActorLocation();
   }
   // Same-frame collision callbacks must emit one terminal impact, before replicated destruction.
   FHitResult Hit;
   Hit.ImpactPoint = Projectile->GetActorLocation();
   Projectile->OnProjectileHit(nullptr, nullptr, nullptr, FVector::ZeroVector, Hit);
   Projectile->OnProjectileHit(nullptr, nullptr, nullptr, FVector::ZeroVector, Hit);
   Test->TestEqual(TEXT("Server terminal impact emitted once"), Projectile->ImpactSequence, uint32(1));
   Enemy->BeginReplicatedDeathPresentation();
   Enemy->BeginReplicatedDeathPresentation();
   Test->TestFalse(TEXT("Target lock rejects dying enemies before tag replication"), UHeroGameplayAbility_TargetLock::IsValidLockTarget(Enemy.Get()));
   URPGDemoFunctionLibrary::AddGameplayTagToActorIfNone(Enemy.Get(), RPGDemoGameplayTags::Shared_Status_Dead);
   // Both owning players request the same stone. The server serializes these RPCs.
   ClientHero->ServerTryConsumeNearbyStones();
   Host->ServerTryConsumeNearbyStones();
   Test->AddInfo(TEXT("Fixture replicas found; impact, death and concurrent pickup dispatched."));
   Stage = 4;
   return false;
  }
  if (Stage == 4)
  {
   if (bBlueprintEnemy)
   {
    ServerDeathDistance = FMath::Max(ServerDeathDistance, FVector::Distance(Enemy->GetActorLocation(), DeathStartLocation));
    ClientDeathDistance = FMath::Max(ClientDeathDistance, FVector::Distance(ClientEnemy->GetActorLocation(), ClientDeathStartLocation));
   }
   if (ClientProjectile->LastImpactSequence != 1 || !ClientStone->bLocalConsumptionPresented ||
    !ClientEnemy->bLocalDissolvePresentationApplied) return false;
   if (bBlueprintEnemy)
   {
    UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(ClientEnemy->GetMesh()->GetMaterial(0));
    bool bNiagaraSpawned = false;
    for (USceneComponent* Child : ClientEnemy->GetMesh()->GetAttachChildren())
    {
     const UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Child);
     bNiagaraSpawned |= Niagara && Niagara->GetAsset() == ClientEnemy->ReplicatedDeathPresentation.DissolveSystem.Get();
    }
    if (!Material || !bNiagaraSpawned || !ClientEnemy->GetMesh()->bPauseAnims ||
     Material->K2_GetScalarParameterValue(TEXT("DissolveAmount")) <= 0.05f) return false;
    Test->TestNotNull(TEXT("Real server death montage replicated"), ClientEnemy->ReplicatedDeathPresentation.DeathMontage.Get());
    Test->TestTrue(TEXT("Death montage contains root motion"), Enemy->ReplicatedDeathPresentation.DeathMontage && Enemy->ReplicatedDeathPresentation.DeathMontage->HasRootMotion());
    Test->TestTrue(TEXT("Dedicated server applies death root motion"), ServerDeathDistance > 5.f);
    if (bImmediateDeath)
        Test->TestTrue(TEXT("Late-joining corpse replica catches up to authoritative root motion"),
            FVector::Distance(ClientEnemy->GetActorLocation(), Enemy->GetActorLocation()) < 10.f);
    else Test->TestTrue(TEXT("Remote client applies death root motion"), ClientDeathDistance > 5.f);
    Test->TestTrue(TEXT("Countdown widget removes itself when the supplied duration completes"), !CountdownWidget.IsValid() || !CountdownWidget->IsInViewport());
    Test->AddInfo(FString::Printf(TEXT("Death root motion displacement: server=%.2f cm, client=%.2f cm"), ServerDeathDistance, ClientDeathDistance));
    Test->TestNotNull(TEXT("Cold dissolve soft asset resolved on server"), Enemy->ReplicatedDeathPresentation.DissolveSystem.Get());
    Test->TestTrue(TEXT("Remote real Blueprint spawned dissolve Niagara"), bNiagaraSpawned);
    Test->AddInfo(TEXT("Configured enemy death snapshot advanced the original dissolve curve without runtime errors."));
   }
   Test->TestEqual(TEXT("Remote received terminal impact before actor destruction"), ClientProjectile->LastImpactSequence, uint32(1));
   Test->TestEqual(TEXT("Dedicated server presented impact once"), Projectile->LastImpactSequence, uint32(1));
   Test->TestTrue(TEXT("Server death state started"), Enemy->ReplicatedDeathPresentation.bStarted);
   Test->TestTrue(TEXT("Remote death presentation applied"), ClientEnemy->bLocalDeathPresentationApplied);
   Test->TestTrue(TEXT("Remote received death tag"), ClientEnemy->GetRPGDemoAbilitySystemComponent()->HasMatchingGameplayTag(RPGDemoGameplayTags::Shared_Status_Dead));
   Test->TestTrue(TEXT("Server stone consumed"), Stone->IsConsumed());
   Test->TestTrue(TEXT("Dedicated server received consume presentation"), Stone->bLocalConsumptionPresented);
   Test->TestFalse(TEXT("A second consumer cannot apply the effect again"), Stone->Consume(Remote->GetRPGDemoAbilitySystemComponent(), 1));
   Test->AddInfo(bBlueprintEnemy ? TEXT("Blueprint enemy death regression passed.") : TEXT("Network contract regression passed. Native death fixture verifies dispatch, not montage/material rendering."));
   PoseAtDissolve = ClientEnemy->GetMesh()->GetComponentSpaceTransforms();
   LastDissolveAmount = bBlueprintEnemy ? Cast<UMaterialInstanceDynamic>(ClientEnemy->GetMesh()->GetMaterial(0))->K2_GetScalarParameterValue(TEXT("DissolveAmount")) : 0.f;
   CleanupStarted = FPlatformTime::Seconds();
   Stage = 5;
   return false;
  }
  if (Stage == 5)
  {
   if (IsValid(ClientEnemy.Get()) && bBlueprintEnemy)
   {
    const auto& Pose = ClientEnemy->GetMesh()->GetComponentSpaceTransforms();
    bool bStable = Pose.Num() == PoseAtDissolve.Num();
    for (int32 Index = 0; bStable && Index < Pose.Num(); ++Index) bStable &= Pose[Index].Equals(PoseAtDissolve[Index], 0.05f);
    if (!bStable) { Test->AddError(TEXT("Corpse bone pose changed during dissolve")); return true; }
    auto* Material = Cast<UMaterialInstanceDynamic>(ClientEnemy->GetMesh()->GetMaterial(0));
    const float Amount = Material ? Material->K2_GetScalarParameterValue(TEXT("DissolveAmount")) : -1.f;
    if (Amount + 0.001f < LastDissolveAmount) { Test->AddError(TEXT("Dissolve progress moved backwards")); return true; }
    LastDissolveAmount = Amount;
   }
   if (IsValid(Enemy.Get()) || IsValid(ClientEnemy.Get()) || IsValid(Stone.Get()) || IsValid(ClientStone.Get())) return false;
   if (bBlueprintEnemy)
   {
    int32 ServerDrops = 0;
    for (TActorIterator<ARPGDemoStoneBase> It(Server); It; ++It) if (It->GetClass() != ARPGDemoStoneBase::StaticClass()) ++ServerDrops;
    if (!ServerDrops) return false;
    for (UWorld* OtherClient : Clients)
    {
     int32 Drops = 0;
     for (TActorIterator<ARPGDemoStoneBase> It(OtherClient); It; ++It) if (It->GetClass() != ARPGDemoStoneBase::StaticClass()) ++Drops;
     if (!Drops) return false;
     Test->TestEqual(TEXT("Exactly one real death drop replicated after corpse destruction"), Drops, 1);
    }
    Test->TestEqual(TEXT("Server drops exactly once despite duplicate death requests"), ServerDrops, 1);
   }
   Test->TestTrue(TEXT("Dissolve pose sampled until authoritative cleanup"), FPlatformTime::Seconds() - CleanupStarted > 0.5);
   Test->TestFalse(TEXT("Target lock rejects destroyed enemies"), UHeroGameplayAbility_TargetLock::IsValidLockTarget(Enemy.Get()));
   for (UWorld* OtherClient : Clients)
   {
    bool bStoneRemains = false, bEnemyRemains = false;
    for (TActorIterator<ARPGDemoStoneBase> It(OtherClient); It; ++It) bStoneRemains |= It->GetClass() == ARPGDemoStoneBase::StaticClass();
    for (TActorIterator<ARPGDemoEnemyCharacter> It(OtherClient); It; ++It) bEnemyRemains |= It->GetFName() == Enemy->GetFName();
    Test->TestFalse(TEXT("Consumed stone removed from each client"), bStoneRemains);
    Test->TestFalse(TEXT("Corpse removed from each client"), bEnemyRemains);
   }
   return true;
  }
  return false;
 }
private:
 FAutomationTestBase* Test;
 FDelegateHandle EndPIEHandle;
 double Started;
 bool bBlueprintEnemy = false;
 int32 EnemyVariant = 0;
 bool bImmediateDeath = false;
 TArray<FTransform> PoseAtDissolve;
 float LastDissolveAmount = 0.f;
 double CleanupStarted = 0.0;
 int32 Stage = 0;
 TStrongObjectPtr<UObject> DropDefaults;
 FStructProperty* DropChanceProperty = nullptr;
 FScalableFloat OriginalDropChance;
 TStrongObjectPtr<UGameplayAbility> GrantedDropDefaults;
 FStructProperty* GrantedDropChanceProperty = nullptr;
 FScalableFloat OriginalGrantedDropChance;
 bool bGuaranteedGrantedDrop = false;
 bool bRequestedPIE = false;
 EPlayNetMode PreviousNetMode = PIE_Standalone;
 bool bPreviousOneProcess = true;
 bool bPreviousLaunchSeparateServer = false;
 int32 PreviousClientCount = 1;
 TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
 UClass* RageClass = nullptr;
 TWeakObjectPtr<ARPGDemoHeroCharacter> Host, Remote, ClientHero;
 TStrongObjectPtr<ARPGDemoProjectileBase> Projectile, ClientProjectile;
 TStrongObjectPtr<ARPGDemoStoneBase> Stone, ClientStone;
 TStrongObjectPtr<ARPGDemoEnemyCharacter> Enemy, ClientEnemy;
 TWeakObjectPtr<UUserWidget> CountdownWidget;
 bool bCountdownShown = false;
 double CountdownStarted = 0.0;
 FVector DeathStartLocation = FVector::ZeroVector, ClientDeathStartLocation = FVector::ZeroVector;
 double ServerDeathDistance = 0.0, ClientDeathDistance = 0.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoEndpointValidationTest, "RPGDemo.Network.EndpointValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoEndpointValidationTest::RunTest(const FString& Parameters)
{
 TestTrue(TEXT("IPv4 endpoint"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("127.0.0.1:7777")));
 TestTrue(TEXT("DNS endpoint"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("demo.example.com:7777")));
 TestFalse(TEXT("Missing port"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("demo.example.com")));
 TestFalse(TEXT("Empty host"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT(":7777")));
 TestFalse(TEXT("Non-numeric port"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("localhost:game")));
 TestFalse(TEXT("Zero port"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("localhost:0")));
 TestFalse(TEXT("Out-of-range port"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("localhost:65536")));
 TestFalse(TEXT("Whitespace in host"), URPGDemoGameInstance::IsValidDedicatedServerEndpoint(TEXT("bad host:7777")));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoNetworkContractsTest, "RPGDemo.Network.DedicatedServerContracts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoNetworkContractsTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoNetworkRegressionCommand(this));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoBlueprintDeathTest, "RPGDemo.Network.BlueprintEnemyDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoBlueprintDeathTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoNetworkRegressionCommand(this, true));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoGlacerDeathTest, "RPGDemo.Network.GlacerDeathCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoGlacerDeathTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoNetworkRegressionCommand(this, true, 1));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoGiantDeathTest, "RPGDemo.Network.GiantDeathCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoGiantDeathTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoNetworkRegressionCommand(this, true, 2));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoEarlyDeathTest, "RPGDemo.Network.ImmediateDeathWithoutWeapon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoEarlyDeathTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FRPGDemoNetworkRegressionCommand(this, true, 0, true));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDemoSpawnPolicyTest, "RPGDemo.Network.SpawnAbilityPolicies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDemoSpawnPolicyTest::RunTest(const FString& Parameters)
{
 for (const TCHAR* Name : {TEXT("GA_Enemy_SpawnEnemies_Base"), TEXT("GA_Enemy_SpawnStone_Base")})
 {
  const FString Path = FString::Printf(TEXT("/Game/Shared/GameplayAbility/%s.%s_C"), Name, Name);
  UClass* Class = LoadClass<UGameplayAbility>(nullptr, *Path);
  if (!TestNotNull(Path, Class)) continue;
  const UGameplayAbility* Ability = Class->GetDefaultObject<UGameplayAbility>();
  TestEqual(Path + TEXT(" execution"), Ability->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
  TestEqual(Path + TEXT(" security"), Ability->GetNetSecurityPolicy(), EGameplayAbilityNetSecurityPolicy::ServerOnly);
 }
 return true;
}
#endif
