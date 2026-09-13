using UnrealBuildTool;

public class GV2Target : TargetRules
{
    public GV2Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.Add("GV2");

        PreBuildSteps.Add("python3 \"$(ProjectDir)/Tools/Build/generate_build_identity.py\" --repo-root \"$(ProjectDir)\" --output \"$(ProjectDir)/Source/GV2/Public/GV2BuildIdentity.gen.h\"");
    }
}
