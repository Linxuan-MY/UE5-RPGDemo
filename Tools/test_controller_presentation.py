import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ControllerPresentationTests(unittest.TestCase):
    def test_local_controller_owns_survival_widgets(self):
        header = (ROOT / "Source/RPGDemo/Public/Controllers/RPGDemoHeroController.h").read_text(encoding="utf-8")
        source = (ROOT / "Source/RPGDemo/Private/Controllers/RPGDemoHeroController.cpp").read_text(encoding="utf-8")
        self.assertIn("ShowCountdownMessage", header)
        self.assertIn("ShowTransientMessage", header)
        self.assertIn("ShowResultScreen", header)
        self.assertIn("IsLocalController()", source)
        self.assertIn("WBP_WaveTextWithCountDown", source)
        self.assertIn("WBP_WinScreen", source)
        self.assertIn("WBP_LoseScreen", source)


if __name__ == "__main__":
    unittest.main()
