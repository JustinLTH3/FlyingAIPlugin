using UnrealBuildTool;

public class FACoreEditor : ModuleRules
{
	public FACoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", "FACore", "UMG", "UMGEditor", "BlueprintEditorLibrary", "Blutility", "ScriptableEditorWidgets", "UnrealEd"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			}
		);
	}
}