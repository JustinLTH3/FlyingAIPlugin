using UnrealBuildTool;

public class FATestSuite : ModuleRules
{
    public FATestSuite(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrecompileForTargets = PrecompileTargetsType.Any;
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "FACore", "FunctionalTesting","TestFramework", "UnrealEd"
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