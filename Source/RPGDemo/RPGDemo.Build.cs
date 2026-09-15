// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RPGDemo : ModuleRules
{
	public RPGDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"GameplayAbilities",
			"GameplayTasks",
			"AnimGraphRuntime",
			"MotionWarping",
			"Niagara",
			"NavigationSystem",
			"MoviePlayer"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

	}
}
