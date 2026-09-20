import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]


class SourceEngineAssociationTests(unittest.TestCase):
    def test_project_uses_portable_source_engine_path(self) -> None:
        project = json.loads((PROJECT_ROOT / "RPGDemo.uproject").read_text(encoding="utf-8"))

        self.assertEqual(
            project["EngineAssociation"],
            "../../UE Source/UnrealEngine-5.8",
        )
        self.assertFalse(project["EngineAssociation"].startswith("{"))

    def test_configuration_script_uses_source_engine_tools(self) -> None:
        script = (PROJECT_ROOT / "Tools" / "Configure-SourceEngine.ps1").read_text(encoding="utf-8")

        self.assertIn("GenerateProjectFiles.bat", script)
        self.assertIn("Engine\\Build\\BatchFiles\\Build.bat", script)
        self.assertIn("[System.IO.Path]::GetRelativePath", script)
        self.assertIn("-CurrentPlatform", script)


if __name__ == "__main__":
    unittest.main()
