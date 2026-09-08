using System.IO;
using UnrealBuildTool;

public class GV2 : ModuleRules
{
    public GV2(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;

        // PCC-05: FGV2PropertyConsumerFactory::GetKindHandlingStatus relies on an
        // exhaustive switch over EGV2PreparedUiValueKind (no default:) to make an
        // unclassified new kind a build error. UBT/Clang disable -Wswitch by default
        // engine-wide, which would silently defeat that gate; re-enable it as an error
        // for this module so the gate is real.
        CppCompileWarningSettings.SwitchUnhandledEnumeratorWarningLevel = WarningLevel.Error;

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
            "GV2ContentHostSupport",
            // PSC-09A (ADR-0043 D2): the one allowed direction -- GV2 depends on the
            // physical Apply module; GV2PresentationApply.Build.cs has, and must keep,
            // no reverse edge back to GV2 or any content/authority module.
            "GV2PresentationApply"
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
