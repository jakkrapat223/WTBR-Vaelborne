// Copyright Vaelborne: Dominion. All Rights Reserved.

using UnrealBuildTool;

public class WTBR : ModuleRules
{
    public WTBR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "EnhancedInput",
            "NetCore",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate", "SlateCore",
            "AssetRegistry",
        });

        // Add module root so subdirectory-prefixed includes resolve correctly:
        // e.g. #include "Components/WTBRHealthComponent.h" → Source/WTBR/Components/...
        PublicIncludePaths.Add(ModuleDirectory);
    }
}
