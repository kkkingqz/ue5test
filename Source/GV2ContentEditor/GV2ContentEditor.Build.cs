using System.IO;
using UnrealBuildTool;

public class GV2ContentEditor : ModuleRules
{
    public GV2ContentEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GV2ContentCore",
            "GV2ContentHostSupport",
            "GV2ContentAuthoring"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "InputCore",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "PropertyEditor",
            "WorkspaceMenuStructure"
        });
    }
}
