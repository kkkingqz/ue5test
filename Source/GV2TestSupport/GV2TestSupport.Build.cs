using UnrealBuildTool;

public class GV2TestSupport : ModuleRules
{
    public GV2TestSupport(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;

        // TAS-04: the one module both hosts call to run Tests/Lua specs.
        // Test-only — no gameplay host links this module.
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "GV2ContentCore",
            "GV2ContentHostSupport",
            "GV2RuntimeCore"
        });
        PrivateDependencyModuleNames.Add("Core");
        PublicDefinitions.Add("GV2_TEST_SUPPORT_IMPORTS=1");
        PrivateDefinitions.Add("GV2_TEST_SUPPORT_EXPORTS=1");
    }
}
