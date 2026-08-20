using UnrealBuildTool;

public class GV2EditorTarget : TargetRules
{
    public GV2EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.Add("GV2");
        ExtraModuleNames.Add("GV2ContentEditor");
    }
}
