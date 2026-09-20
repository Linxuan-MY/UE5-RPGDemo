using UnrealBuildTool;
using System.Collections.Generic;

public class RPGDemoServerTarget : TargetRules
{
    public RPGDemoServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("RPGDemo");

        DisablePlugins.AddRange(new string[]
        {
            "OnlineSubsystemNull",
            "ModelContextProtocol",
            "AllToolsets"
        });
    }
}
