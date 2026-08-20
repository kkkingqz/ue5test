using UnrealBuildTool;

public class GV2ContentAuthoring : ModuleRules
{
    public GV2ContentAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;

        // Only the Unreal module bootstrap uses Core. Portable public headers and
        // implementation sources remain dependency-free (besides GV2ContentCore and GV2ContentHostSupport)
        // and are shared with CMake.
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "GV2ContentCore",
            "GV2ContentHostSupport"
        });
        PrivateDependencyModuleNames.Add("Core");
        PublicDefinitions.Add("GV2_CONTENT_AUTHORING_IMPORTS=1");
        PrivateDefinitions.Add("GV2_CONTENT_AUTHORING_EXPORTS=1");
    }
}
