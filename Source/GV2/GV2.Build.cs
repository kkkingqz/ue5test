using System.IO;
using UnrealBuildTool;

public class GV2 : ModuleRules
{
    public GV2(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "UMG",
            "CommonUI",
            "GV2RuntimeCore",
            "GV2ContentCore",
            "GV2ContentHostSupport"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "ImageCore",
            "Slate",
            "SlateCore"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("GV2ContentAuthoring");
            PrivateDependencyModuleNames.Add("GV2TestSupport");
            PrivateDependencyModuleNames.Add("GV2ContentEditor");
        }

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

        // Real GameDataRepository packages ("core", "rh", definitions/schemas JSON5).
        // Tests/Fixtures/PortableContentCore is test-only and intentionally
        // never staged here.
        string GameDataDirectory = Path.Combine(ModuleDirectory, "..", "..", "GameData");
        if (Directory.Exists(GameDataDirectory))
        {
            foreach (string GameDataFile in Directory.GetFiles(
                GameDataDirectory,
                "*",
                SearchOption.AllDirectories))
            {
                RuntimeDependencies.Add(GameDataFile, StagedFileType.NonUFS);
            }
        }
    }
}
