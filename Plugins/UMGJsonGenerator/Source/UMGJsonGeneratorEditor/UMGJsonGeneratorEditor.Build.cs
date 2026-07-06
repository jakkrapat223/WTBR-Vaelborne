using UnrealBuildTool;

public class UMGJsonGeneratorEditor : ModuleRules
{
	public UMGJsonGeneratorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"UMGEditor",
			"AssetTools",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"ToolMenus",
			"EditorStyle",
			"InputCore",
			"Kismet",
			"KismetCompiler",
			"BlueprintGraph",
			"DesktopPlatform",
			"ContentBrowser",
		});
	}
}
