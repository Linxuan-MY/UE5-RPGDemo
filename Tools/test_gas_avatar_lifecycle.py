import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GasAvatarLifecycleTests(unittest.TestCase):
    def test_controller_replication_preserves_player_state_owner(self):
        source = (ROOT / "Source/RPGDemo/Private/Characters/RPGDemoBaseCharacter.cpp").read_text(
            encoding="utf-8-sig"
        )
        self.assertIn("AActor* AbilityOwner", source)
        self.assertIn("!AbilityOwner || AbilityOwner == this", source)

    def test_startup_grant_is_marked_only_after_data_loads(self):
        source = (ROOT / "Source/RPGDemo/Private/Characters/RPGDemoHeroCharacter.cpp").read_text(
            encoding="utf-8-sig"
        )
        load_index = source.index("CharacterStartUpData.LoadSynchronous()")
        mark_index = source.index("TryMarkHeroStartUpDataGranted()")
        self.assertLess(load_index, mark_index)


if __name__ == "__main__":
    unittest.main()
