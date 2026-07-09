using UnrealBuildTool;

public class NiagaraJsonGeneratorEditor : ModuleRules
{
	public NiagaraJsonGeneratorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Niagara",
			"NiagaraCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"EditorScriptingUtilities",
			"Json",
			"ToolMenus",
			"Slate",
			"SlateCore",
			"DesktopPlatform",
		});
	}
}
