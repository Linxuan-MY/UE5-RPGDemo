import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DedicatedServerResultUITests(unittest.TestCase):
    def test_network_result_widget_hides_local_restart(self):
        source = (ROOT / "Source/RPGDemo/Private/Widgets/RPGDemoResultWidget.cpp").read_text(encoding="utf-8")
        self.assertIn("GetNetMode() != NM_Standalone", source)
        self.assertIn("WBP_PlayAgainButton", source)
        self.assertIn("WBP_TryAgainButton", source)
        self.assertIn("ESlateVisibility::Collapsed", source)


if __name__ == "__main__":
    unittest.main()
