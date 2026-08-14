#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/ParseLimits.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreParseLimitsTest,
    "GV2.Runtime.ContentCore.ParseLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreParseLimitsTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    FParseLimits Limits;
    Limits.MaxFileSizeBytes = 100;
    Limits.MaxNestingDepth = 5;
    Limits.MaxStringLengthBytes = 20;
    Limits.MaxContainerEntries = 10;

    // 1. Valid UTF-8 without BOM
    std::vector<FDiagnostic> Diags;
    std::string_view ValidInput = "{\n  \"key\": \"значение\"\n}";
    auto ValidRes = ValidateUtf8AndLimits(ValidInput, Limits, Diags);
    TestTrue(TEXT("Valid UTF-8 input succeeds"), ValidRes.has_value());
    TestFalse(TEXT("Valid UTF-8 has no BOM"), ValidRes->bHasBOM);
    TestEqual(TEXT("Cleaned view matches raw input"), ValidRes->CleanedView, ValidInput);
    TestTrue(TEXT("No diagnostics for valid UTF-8"), Diags.empty());

    // 2. Valid UTF-8 with BOM
    Diags.clear();
    std::string BomInput = "\xEF\xBB\xBF{\n  \"key\": \"val\"\n}";
    auto BomRes = ValidateUtf8AndLimits(BomInput, Limits, Diags);
    TestTrue(TEXT("Valid UTF-8 with BOM succeeds"), BomRes.has_value());
    TestTrue(TEXT("BOM detected"), BomRes->bHasBOM);
    TestEqual(TEXT("Cleaned view strips 3 BOM bytes"), BomRes->CleanedView.size(), BomInput.size() - 3);
    TestEqual(TEXT("ByteOffsetShift is 3"), BomRes->ByteOffsetShift, static_cast<size_t>(3));
    TestTrue(TEXT("No diagnostics for BOM input"), Diags.empty());

    // 3. Invalid UTF-8 sequence
    Diags.clear();
    std::string InvalidUtf8Input = "{\n  \"key\": \"\xFF\xFF\"\n}";
    auto InvalidUtf8Res = ValidateUtf8AndLimits(InvalidUtf8Input, Limits, Diags);
    TestFalse(TEXT("Invalid UTF-8 fails validation"), InvalidUtf8Res.has_value());
    TestEqual(TEXT("Produces 1 diagnostic"), Diags.size(), static_cast<size_t>(1));
    if (!Diags.empty())
    {
        TestEqual(TEXT("Code is invalid_utf8"), Diags[0].Code, std::string("core:diagnostic.json5.invalid_utf8"));
    }

    // 4. File size limit exceeded
    Diags.clear();
    std::string LargeInput(150, 'a');
    auto LargeRes = ValidateUtf8AndLimits(LargeInput, Limits, Diags);
    TestFalse(TEXT("Exceeding file size fails"), LargeRes.has_value());
    TestEqual(TEXT("Produces 1 diagnostic"), Diags.size(), static_cast<size_t>(1));
    if (!Diags.empty())
    {
        TestEqual(TEXT("Code is limit_file_size"), Diags[0].Code, std::string("core:diagnostic.json5.limit.file_size"));
    }

    // 5. Nesting depth limit helper
    Diags.clear();
    FSourceSpan Span{ 1, 1, 1, 5 };
    bool bDepthOk = CheckNestingDepth(6, Limits, Span, Diags);
    TestFalse(TEXT("Exceeding depth fails"), bDepthOk);
    TestEqual(TEXT("Produces 1 diagnostic"), Diags.size(), static_cast<size_t>(1));
    if (!Diags.empty())
    {
        TestEqual(TEXT("Code is limit_nesting_depth"), Diags[0].Code, std::string("core:diagnostic.json5.limit.nesting_depth"));
    }

    // 6. String length limit helper
    Diags.clear();
    bool bStringOk = CheckStringLength(25, Limits, Span, Diags);
    TestFalse(TEXT("Exceeding string length fails"), bStringOk);
    TestEqual(TEXT("Produces 1 diagnostic"), Diags.size(), static_cast<size_t>(1));
    if (!Diags.empty())
    {
        TestEqual(TEXT("Code is limit_string_length"), Diags[0].Code, std::string("core:diagnostic.json5.limit.string_length"));
    }

    // 7. Container entries limit helper
    Diags.clear();
    bool bContainerOk = CheckContainerEntries(15, Limits, Span, Diags);
    TestFalse(TEXT("Exceeding container entries fails"), bContainerOk);
    TestEqual(TEXT("Produces 1 diagnostic"), Diags.size(), static_cast<size_t>(1));
    if (!Diags.empty())
    {
        TestEqual(TEXT("Code is limit_container_entries"), Diags[0].Code, std::string("core:diagnostic.json5.limit.container_entries"));
    }

    Diags.clear();
    FParseLimits UnsafeLimits;
    UnsafeLimits.MaxNestingDepth = FParseLimits::MaxSupportedNestingDepth + 1;
    auto UnsafeResult = ValidateUtf8AndLimits("{}", UnsafeLimits, Diags);
    TestFalse(TEXT("Nesting limit above portable ceiling is rejected"), UnsafeResult.has_value());
    TestTrue(
        TEXT("Unsafe nesting configuration has typed diagnostic"),
        !Diags.empty() && Diags[0].Code == "core:diagnostic.json5.limit.invalid_configuration");

    return true;
}

#endif
