using UnrealBuildTool;

public class GV2ContentCore : ModuleRules
{
    public GV2ContentCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;

        // Only the Unreal module bootstrap uses Core. Portable public headers and
        // implementation sources remain dependency-free and are shared with CMake.
        PrivateDependencyModuleNames.Add("Core");
        PublicDefinitions.Add("GV2_CONTENT_CORE_IMPORTS=1");
        PrivateDefinitions.Add("GV2_CONTENT_CORE_EXPORTS=1");
    }
}
