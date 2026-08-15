using UnrealBuildTool;

public class GV2ContentHostSupport : ModuleRules
{
    public GV2ContentHostSupport(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;

        // Only the Unreal module bootstrap uses Core. Portable public headers and
        // implementation sources remain dependency-free (besides GV2ContentCore)
        // and are shared with CMake.
        PublicDependencyModuleNames.Add("GV2ContentCore");
        PrivateDependencyModuleNames.Add("Core");
        PublicDefinitions.Add("GV2_CONTENT_HOST_SUPPORT_IMPORTS=1");
        PrivateDefinitions.Add("GV2_CONTENT_HOST_SUPPORT_EXPORTS=1");
    }
}
