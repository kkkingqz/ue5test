using UnrealBuildTool;

// PSC-09A (ADR-0043 D2/D4): the dependency list below IS the primary guarantee that
// physical Apply cannot reach a content/authority type -- not a name a source-scanning
// gate has to know in advance. GV2, GV2ContentCore, GV2ContentHostSupport,
// GV2RuntimeCore, DeveloperSettings, AssetRegistry, ImageCore and every filesystem/
// content-authoring module are explicitly denied: this module simply never lists them,
// so UBT cannot link code that would let it read a snapshot, repository, package set,
// Screen Registry or Theme source, whatever such a type is named later.
// See Tools/Testing/validate_presentation_apply_module_graph.py for the declaration-
// derived gate that keeps this list honest.
public class GV2PresentationApply : ModuleRules
{
    public GV2PresentationApply(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "CommonUI",
            "Slate",
            "SlateCore"
        });
    }
}
