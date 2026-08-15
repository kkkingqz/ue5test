#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GV2ContentHostSupport/LuaSpecIdentity.h"

// TAS-03: stable failure identity, `<spec>.<case>` derivation, and the code
// that distinguishes a case's logical failure from a spec-level Lua fault.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaSpecIdentityTest,
    "GV2.Runtime.ContentHostSupport.LuaSpecIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaSpecIdentityTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentHostSupport;

    // 1. Nested relative path -> dotted spec id, extension stripped.
    TestEqual(
        TEXT("nested path derives dotted spec id"),
        FString(DeriveLuaSpecId("world/current_location.lua").c_str()),
        FString("world.current_location"));

    // 2. Top-level relative path -> spec id unchanged besides extension.
    TestEqual(
        TEXT("top-level path derives unqualified spec id"),
        FString(DeriveLuaSpecId("smoke.lua").c_str()),
        FString("smoke"));

    // 3. Case failure identifier composition and code.
    const FLuaSpecFailure CaseFailure = MakeLuaSpecCaseFailure(
        "world.current_location", "wrapper_rejects_repeated_access", "assertion failed!");
    TestEqual(
        TEXT("case failure identifier is <spec>.<case>"),
        FString(CaseFailure.Identifier.c_str()),
        FString("world.current_location.wrapper_rejects_repeated_access"));
    TestEqual(TEXT("case failure code is LuaSpecCaseFailed"), FString(CaseFailure.Code.c_str()), FString("LuaSpecCaseFailed"));

    // 4. Spec-level fault identifier composition and code — distinguishable
    // from a case failure by Code alone, never by parsing Identifier.
    const FLuaSpecFailure SpecFault = MakeLuaSpecFault(
        "world.current_location", "LuaSpecSyntaxError", "unexpected symbol near 'end'");
    TestEqual(TEXT("spec fault identifier is the bare spec id"), FString(SpecFault.Identifier.c_str()), FString("world.current_location"));
    TestEqual(TEXT("spec fault code is the RunLuaSpec fault code"), FString(SpecFault.Code.c_str()), FString("LuaSpecSyntaxError"));
    TestNotEqual(TEXT("case failure and spec fault codes differ"), FString(CaseFailure.Code.c_str()), FString(SpecFault.Code.c_str()));

    // 5. Identifiers contain no absolute paths, addresses, or timing info —
    // they are pure derivations of RelativePath/CaseId/FaultCode inputs.
    TestFalse(TEXT("case failure identifier has no path separators from an absolute root"), CaseFailure.Identifier.starts_with("/"));
    TestFalse(TEXT("spec fault identifier has no path separators from an absolute root"), SpecFault.Identifier.starts_with("/"));

    return true;
}

#endif
