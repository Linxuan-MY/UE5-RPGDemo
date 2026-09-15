using UnrealBuildTool;
using System.Collections.Generic;

public class RPGDemoServerTarget : TargetRules
{
    public RPGDemoServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("RPGDemo");
    }
}
