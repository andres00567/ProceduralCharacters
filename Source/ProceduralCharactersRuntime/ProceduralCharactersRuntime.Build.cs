using UnrealBuildTool;

public class ProceduralCharactersRuntime : ModuleRules
{
    public ProceduralCharactersRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "DeveloperSettings", "AnimGraphRuntime", "AnimationCore"
        });
    }
}
