import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DedicatedServerUITests(unittest.TestCase):
    def read(self, relative):
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_multiplayer_widget_connects_directly_and_surfaces_status(self):
        header = self.read("Source/RPGDemo/Public/Widgets/RPGDemoMultiplayerWidget.h")
        source = self.read("Source/RPGDemo/Private/Widgets/RPGDemoMultiplayerWidget.cpp")
        self.assertIn("HandleConnectClicked", header)
        self.assertIn("HandleConnectionStateChanged", header)
        self.assertIn("ConnectToDedicatedServer", source)
        self.assertIn("OnConnectionStateChanged.AddUniqueDynamic", source)
        self.assertIn("GetResolvedDedicatedServerEndpoint", source)


if __name__ == "__main__":
    unittest.main()
