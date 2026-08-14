using UnrealBuildTool;
using System.IO;

public class GV2RuntimeCore : ModuleRules
{
    public GV2RuntimeCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "GV2ContentCore" });
        PublicDefinitions.Add("GV2_RUNTIME_CORE_IMPORTS=1");
        PrivateDefinitions.Add("GV2_RUNTIME_CORE_EXPORTS=1");
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "ThirdParty", "Lua54"));

        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PrivateDefinitions.Add("LUA_USE_LINUX=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDefinitions.Add("LUA_USE_WINDOWS=1");
        }
    }
}
