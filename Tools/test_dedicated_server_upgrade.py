import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DedicatedServerUpgradeTests(unittest.TestCase):
    def read(self, relative):
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_server_target_exists_and_builds_rpgdemo_server(self):
        target = self.read("Source/RPGDemoServer.Target.cs")
        self.assertIn("TargetType.Server", target)
        self.assertIn('ExtraModuleNames.Add("RPGDemo")', target)
        self.assertIn("EngineIncludeOrderVersion.Unreal5_8", target)

    def test_network_settings_define_safe_local_default(self):
        header = self.read("Source/RPGDemo/Public/Settings/RPGDemoNetworkSettings.h")
        source = self.read("Source/RPGDemo/Private/Settings/RPGDemoNetworkSettings.cpp")
        config = self.read("Config/DefaultGame.ini")
        self.assertIn("UCLASS(Config=Game, DefaultConfig)", header)
        self.assertIn("DedicatedServerEndpoint", header)
        self.assertIn('TEXT("127.0.0.1:7777")', source)
        self.assertIn("[/Script/RPGDemo.RPGDemoNetworkSettings]", config)
        self.assertIn("DedicatedServerEndpoint=127.0.0.1:7777", config)

    def test_game_instance_exposes_direct_connect_contract(self):
        header = self.read("Source/RPGDemo/Public/RPGDemoGameInstance.h")
        source = self.read("Source/RPGDemo/Private/RPGDemoGameInstance.cpp")
        for symbol in (
            "ConnectToDedicatedServer",
            "DisconnectToMainMenu",
            "GetResolvedDedicatedServerEndpoint",
            "OnConnectionStateChanged",
        ):
            self.assertIn(symbol, header)
        self.assertIn("RPGDemoServer=", source)
        self.assertNotIn("CreateSession", source)
        self.assertNotIn("FindSessions", source)
        self.assertNotIn("JoinSession", source)
        self.assertNotIn("?listen", source)

    def test_online_subsystem_null_is_removed(self):
        build = self.read("Source/RPGDemo/RPGDemo.Build.cs")
        project = self.read("RPGDemo.uproject")
        engine = self.read("Config/DefaultEngine.ini")
        self.assertNotIn('"OnlineSubsystem"', build)
        self.assertNotIn('"OnlineSubsystemUtils"', build)
        self.assertNotIn('"OnlineSubsystemNull"', project)
        self.assertNotIn("[OnlineSubsystemNull]", engine)

    def test_player_state_owns_replicated_ability_system(self):
        header = self.read("Source/RPGDemo/Public/GameModes/RPGDemoPlayerState.h")
        source = self.read("Source/RPGDemo/Private/GameModes/RPGDemoPlayerState.cpp")
        self.assertIn("IAbilitySystemInterface", header)
        self.assertIn("GetAbilitySystemComponent", header)
        self.assertIn("LifeState", header)
        self.assertIn("RespawnEndServerTime", header)
        self.assertIn("SetIsReplicated(true)", source)
        self.assertIn("EGameplayEffectReplicationMode::Mixed", source)

    def test_hero_binds_player_state_as_owner_and_pawn_as_avatar(self):
        source = self.read("Source/RPGDemo/Private/Characters/RPGDemoHeroCharacter.cpp")
        header = self.read("Source/RPGDemo/Public/Characters/RPGDemoHeroCharacter.h")
        self.assertIn("OnRep_PlayerState", header)
        self.assertIn("InitAbilityActorInfo(RPGDemoPlayerState, this)", source)

    def test_survival_state_is_published_as_atomic_snapshot(self):
        header = self.read("Source/RPGDemo/Public/GameModes/RPGDemoGameState.h")
        self.assertIn("FRPGDemoSurvivalSnapshot", header)
        self.assertIn("TeamDefeated", header)
        self.assertIn("StateEndServerTime", header)
        self.assertIn("Revision", header)
        self.assertIn("ReplicatedUsing = OnRep_SurvivalSnapshot", header)

    def test_survival_death_api_is_player_specific(self):
        header = self.read("Source/RPGDemo/Public/GameModes/RPGDemoSurvivalGameMode.h")
        source = self.read("Source/RPGDemo/Private/GameModes/RPGDemoSurvivalGameMode.cpp")
        self.assertIn("NotifyPlayerDied(AController*", header)
        self.assertIn("RestartPlayer", source)
        self.assertIn("TeamDefeated", source)
        self.assertIn("RespawnDelaySeconds", source)

    def test_linux_build_and_atomic_deploy_tools_exist(self):
        build = self.read("Tools/Build-LinuxServer.ps1")
        deploy = self.read("Tools/Deploy-LinuxServer.ps1")
        service = self.read("Deploy/Linux/rpgdemo.service")
        self.assertIn("BuildCookRun", build)
        self.assertIn("Linux", build)
        self.assertIn("Get-FileHash", build)
        self.assertIn("/opt/rpgdemo/releases", deploy)
        self.assertIn("sha256sum", deploy)
        self.assertIn("rollback", deploy.lower())
        self.assertIn("Restart=on-failure", service)
        self.assertIn("7777", service)


if __name__ == "__main__":
    unittest.main()
