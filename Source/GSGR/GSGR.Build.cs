// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GSGR : ModuleRules
{
	public GSGR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"GSGR",
			"GSGR/Variant_Platforming",
			"GSGR/Variant_Platforming/Animation",
			"GSGR/Variant_Combat",
			"GSGR/Variant_Combat/AI",
			"GSGR/Variant_Combat/Animation",
			"GSGR/Variant_Combat/Gameplay",
			"GSGR/Variant_Combat/Interfaces",
			"GSGR/Variant_Combat/UI",
			"GSGR/Variant_SideScrolling",
			"GSGR/Variant_SideScrolling/AI",
			"GSGR/Variant_SideScrolling/Gameplay",
			"GSGR/Variant_SideScrolling/Interfaces",
			"GSGR/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
