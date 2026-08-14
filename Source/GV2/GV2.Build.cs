using System.IO;
using UnrealBuildTool;

public class GV2 : ModuleRules
{
    public GV2(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "UMG",
            "CommonUI",
            "GV2RuntimeCore",
            "GV2ContentCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "ImageCore",
            "Slate",
            "SlateCore"
        });

        foreach (string ScriptFile in Directory.GetFiles(
            Path.Combine(ModuleDirectory, "..", "..", "Scripts"),
            "*.lua",
            SearchOption.AllDirectories))
        {
            RuntimeDependencies.Add(ScriptFile, StagedFileType.NonUFS);
        }

        string ResourceDirectory = Path.Combine(ModuleDirectory, "..", "..", "Resources");
        if (Directory.Exists(ResourceDirectory))
        {
            foreach (string ResourceFile in Directory.GetFiles(
                ResourceDirectory,
                "*",
                SearchOption.AllDirectories))
            {
                RuntimeDependencies.Add(ResourceFile, StagedFileType.NonUFS);
            }
        }

    }
}
