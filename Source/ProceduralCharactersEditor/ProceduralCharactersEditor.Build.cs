using UnrealBuildTool;

public class ProceduralCharactersEditor : ModuleRules
{
    public ProceduralCharactersEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "AnimGraph", "BlueprintGraph", "Kismet", "AssetRegistry",
            "ProceduralCharactersRuntime"
        });
    }
}
