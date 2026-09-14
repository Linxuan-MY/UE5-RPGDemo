import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class UpgradeRegressionTests(unittest.TestCase):
    def read(self, relative):
        return (ROOT / relative).read_text(encoding='utf-8')

    def test_enemy_startup_is_idempotent_and_begin_play_safe(self):
        source = self.read('Source/RPGDemo/Private/Characters/RPGDemoEnemyCharacter.cpp')
        self.assertIn('InitEnemyStartUpData();', source)
        self.assertIn('bEnemyStartUpDataInitialized', source)
        self.assertGreaterEqual(source.count('InitEnemyStartUpData();'), 2)

    def test_damage_execution_rejects_uninitialized_defense(self):
        source = self.read('Source/RPGDemo/Private/AbilitySystem/GE_ExecCalc/GE_ExecCalc_DamageTaken.cpp')
        self.assertIn('SafeTargetDefensePower', source)
        self.assertIn('FMath::Max(TargetDefensePower, 1.f)', source)

    def test_weapon_registration_restores_runtime_visibility(self):
        source = self.read('Source/RPGDemo/Private/Components/Combat/PawnCombatComponent.cpp')
        self.assertIn('SetActorHiddenInGame(false)', source)

    def test_weapon_collision_is_authority_only_and_null_safe(self):
        source = self.read('Source/RPGDemo/Private/Components/Combat/PawnCombatComponent.cpp')
        self.assertIn('!OwningPawn->HasAuthority()', source)
        self.assertIn('!IsValid(WeaponToToggle)', source)
        self.assertNotIn('check(WeaponToToggle)', source)

    def test_remote_character_gas_and_weapon_state_are_rebuilt(self):
        character = self.read('Source/RPGDemo/Private/Characters/RPGDemoBaseCharacter.cpp')
        weapon = self.read('Source/RPGDemo/Private/Items/Weapons/RPGDemoWeaponBase.cpp')
        ability = self.read('Source/RPGDemo/Private/AbilitySystem/Abilities/RPGDemoGameplayAbility.cpp')
        self.assertIn('InitAbilityActorInfo(this, this)', character)
        self.assertIn('OnRep_WeaponRegistrationData', weapon)
        self.assertIn('RegisterSpawnedWeapon(ReplicatedWeaponTag', weapon)
        self.assertIn('ActorInfo->AvatarActor->GetWorld()', ability)

    def test_replicated_attributes_drive_client_ui(self):
        attributes = self.read('Source/RPGDemo/Private/AbilitySystem/RPGDemoAttributeSet.cpp')
        widget = self.read('Source/RPGDemo/Private/Widgets/RPGDemoWidgetBase.cpp')
        self.assertIn('BroadcastHealthToUI();', attributes)
        self.assertIn('BroadcastRageToUI();', attributes)
        self.assertIn('Replicated attributes may arrive before the overlay binds', widget)

    def test_ai_can_switch_between_multiplayer_targets(self):
        source = self.read('Source/RPGDemo/Private/Controllers/RPGDemoAIController.cpp')
        self.assertIn('Prefer the nearest visible hero', source)
        self.assertIn('GetCurrentlyPerceivedActors', source)

    def test_listen_server_rejects_global_pause(self):
        header = self.read('Source/RPGDemo/Public/GameModes/RPGDemoBaseGameMode.h')
        source = self.read('Source/RPGDemo/Private/GameModes/RPGDemoBaseGameMode.cpp')
        self.assertIn('virtual bool SetPause', header)
        self.assertIn('GetNetMode() != NM_Standalone', source)
        self.assertIn('return Super::SetPause', source)

    def test_network_pause_menus_do_not_take_exclusive_slate_focus(self):
        source = self.read('Source/RPGDemo/Private/RPGDemoFunctionLibrary.cpp')
        widget = self.read('Source/RPGDemo/Private/Widgets/RPGDemoWidgetBase.cpp')
        asc = self.read('Source/RPGDemo/Private/AbilitySystem/RPGDemoAbilitySystemComponent.cpp')
        self.assertIn('FInputModeGameAndUI', source)
        self.assertIn('EMouseLockMode::DoNotLock', source)
        self.assertIn('ControlledPawn->DisableInput', source)
        self.assertIn('ControlledPawn->EnableInput', source)
        self.assertIn('GetOwningLocalPlayer() == OwningLocalPlayer', widget)
        self.assertIn('CancelInputHeldAbilities', source)
        self.assertIn('InputTag_MustBeHeld', asc)

    def test_avoid_direction_is_shared_with_server(self):
        header = self.read('Source/RPGDemo/Public/Characters/RPGDemoHeroCharacter.h')
        source = self.read('Source/RPGDemo/Private/Characters/RPGDemoHeroCharacter.cpp')
        self.assertIn('ServerUpdateMovementInputDirection', header)
        self.assertIn('ReplicatedMovementInputDirection', header)
        self.assertIn('DOREPLIFETIME(ARPGDemoHeroCharacter, ReplicatedMovementInputDirection)', source)
        self.assertIn('GetNetworkMovementInputDirection', source)

    def test_room_creation_reuses_initial_search_and_pie_subsystem(self):
        source = self.read('Source/RPGDemo/Private/RPGDemoGameInstance.cpp')
        self.assertIn('Online::GetSubsystem(GetWorld())', source)
        self.assertIn('CreateRoom queued behind the active LAN search', source)
        self.assertIn('bCreateAfterSearch = true;', source)
        self.assertIn('Creating LAN room', source)

    def test_lobby_to_survival_travel_flushes_lobby_game_mode_option(self):
        source = self.read('Source/RPGDemo/Private/RPGDemoGameInstance.cpp')
        self.assertIn('RPGDemoMultiplayer=1?RPGDemoDifficulty=%s', source)
        self.assertIn('*RPGDemoSessions::SurvivalMap, *DifficultyOption), true);', source)

    def test_survival_controller_restores_input_after_lobby_travel(self):
        header = self.read('Source/RPGDemo/Public/Controllers/RPGDemoHeroController.h')
        source = self.read('Source/RPGDemo/Private/Controllers/RPGDemoHeroController.cpp')
        self.assertIn('virtual void AcknowledgePossession(APawn* PossessedPawn) override;', header)
        self.assertIn('FInputModeGameOnly', source)
        self.assertIn('ResetIgnoreMoveInput();', source)
        self.assertIn('ResetIgnoreLookInput();', source)
        self.assertIn('ControlledPawn->EnableInput(this);', source)

    def test_rage_has_native_periodic_cost_fallback(self):
        source = self.read('Source/RPGDemo/Private/AbilitySystem/Abilities/RPGDemoHeroGameplayAbility.cpp')
        self.assertIn('GE_Hero_Cost_Rage', source)
        self.assertIn('Player_Ability_Rage', source)

    def test_loading_screen_uses_real_movie_player_capability_check(self):
        source = self.read('Source/RPGDemo/Private/RPGDemoGameInstance.cpp')
        self.assertIn('IsMoviePlayerEnabled()', source)
        self.assertNotIn('if (!GetMoviePlayer())', source)

    def test_loading_screen_owns_level_travel_and_pie_fallback(self):
        header = self.read('Source/RPGDemo/Public/RPGDemoGameInstance.h')
        source = self.read('Source/RPGDemo/Private/RPGDemoGameInstance.cpp')
        self.assertIn('OpenGameLevelWithLoadingScreen', header)
        self.assertIn('OpenLevelBySoftObjectPtr', source)
        self.assertIn('AddViewportWidgetContent', source)
        self.assertIn('PostLoadMapWithWorld.RemoveAll(this)', source)

    def test_loading_screen_does_not_force_movie_completion(self):
        source = self.read('Source/RPGDemo/Private/RPGDemoGameInstance.cpp')
        self.assertNotIn('StopMovie(', source)


if __name__ == '__main__':
    unittest.main()
