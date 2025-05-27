using UnrealBuildTool;

public class FABehaviourTree : ModuleRules
{
    public FABehaviourTree(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrecompileForTargets = PrecompileTargetsType.Any;
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "AIModule", "GameplayTasks","NavigationSystem", "FACore"
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