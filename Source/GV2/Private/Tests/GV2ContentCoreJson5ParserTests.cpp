#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Json5Parser.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreJson5ParserTest,
    "GV2.Runtime.ContentCore.Json5Parser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreJson5ParserTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    FParseLimits Limits;

    // 1. Scalar roots
    std::vector<FDiagnostic> Diags1;
    auto NullVal = ParseJson5("null", Limits, Diags1);
    TestTrue(TEXT("Parse null root"), NullVal.has_value() && NullVal->IsNull());

    std::vector<FDiagnostic> Diags2;
    auto BoolVal = ParseJson5("true", Limits, Diags2);
    TestTrue(TEXT("Parse bool root"), BoolVal.has_value() && BoolVal->IsBoolean() && BoolVal->AsBoolean());

    std::vector<FDiagnostic> Diags3;
    auto NumVal = ParseJson5("0x10", Limits, Diags3);
    TestTrue(TEXT("Parse hex int root"), NumVal.has_value() && NumVal->IsInteger() && NumVal->AsInteger() == 16);

    // 2. Complex object & array tree
    std::string_view InputDoc = "{\n"
                                "  $schema: 'http://json-schema.org/draft-07/schema#',\n"
                                "  package_id: 'test_pkg',\n"
                                "  values: [10, 20.5, true, null, /* comment */ { nested: 'val' },],\n"
                                "}";

    std::vector<FDiagnostic> DiagsDoc;
    auto RootDoc = ParseJson5(InputDoc, Limits, DiagsDoc);
    TestTrue(TEXT("Parse complex document succeeds"), RootDoc.has_value());
    TestTrue(TEXT("No diagnostics on valid JSON5"), DiagsDoc.empty());

    if (RootDoc.has_value() && RootDoc->IsObject())
    {
        const FValue::FObject& Obj = RootDoc->AsObject();
        TestEqual(TEXT("Object has 3 properties"), Obj.size(), static_cast<size_t>(3));
        if (Obj.size() >= 3)
        {
            TestEqual(TEXT("Property 0 key"), Obj[0].first, std::string("$schema"));
            TestEqual(TEXT("Property 1 key"), Obj[1].first, std::string("package_id"));
            TestEqual(TEXT("Property 2 key"), Obj[2].first, std::string("values"));
            TestTrue(TEXT("Property 2 is array"), Obj[2].second.IsArray());
            if (Obj[2].second.IsArray())
            {
                const FValue::FArray& Arr = Obj[2].second.AsArray();
                TestEqual(TEXT("Array has 5 elements"), Arr.size(), static_cast<size_t>(5));
            }
        }
    }

    // 3. Nesting depth limit failure
    FParseLimits DepthLimits;
    DepthLimits.MaxNestingDepth = 2;
    std::vector<FDiagnostic> DiagsDepth;
    auto DepthFail = ParseJson5("{ a: { b: { c: 1 } } }", DepthLimits, DiagsDepth);
    TestFalse(TEXT("Exceeding depth limit fails parser"), DepthFail.has_value());
    TestFalse(TEXT("Diagnostics generated for depth limit"), DiagsDepth.empty());
    if (!DiagsDepth.empty())
    {
        TestEqual(TEXT("Code is limit_nesting_depth"), DiagsDepth[0].Code, std::string("core:diagnostic.json5.limit.nesting_depth"));
    }

    // 4. Unexpected EOF
    std::vector<FDiagnostic> DiagsEof;
    auto EofFail = ParseJson5("{ a: [ 1, 2", Limits, DiagsEof);
    TestFalse(TEXT("Truncated document fails parser"), EofFail.has_value());
    TestFalse(TEXT("Diagnostics generated for unexpected EOF"), DiagsEof.empty());
    if (!DiagsEof.empty())
    {
        TestTrue(TEXT("Code is unexpected_eof"), DiagsEof[0].Code == "core:diagnostic.json5.unexpected_eof");
    }

    // 5. Duplicate key error
    std::vector<FDiagnostic> DiagsDup;
    auto DupFail = ParseJson5("{ dup_key: 1, dup_key: 2 }", Limits, DiagsDup);
    TestFalse(TEXT("Duplicate key document fails parser"), DupFail.has_value());
    TestFalse(TEXT("Diagnostics generated for duplicate key"), DiagsDup.empty());
    if (!DiagsDup.empty())
    {
        TestTrue(TEXT("Code is duplicate_key"), DiagsDup[0].Code == "core:diagnostic.json5.duplicate_key");
        TestTrue(TEXT("Duplicate diagnostic points to first declaration"), DiagsDup[0].RelatedSpan.has_value());
    }

    // 6. Numeric normalization and rejection (PCC-11)
    std::vector<FDiagnostic> DiagsNan;
    auto NanFail = ParseJson5("NaN", Limits, DiagsNan);
    TestFalse(TEXT("NaN literal fails parser"), NanFail.has_value());
    if (!DiagsNan.empty())
    {
        TestTrue(TEXT("Code is invalid_number"), DiagsNan[0].Code == "core:diagnostic.json5.invalid_number");
    }

    std::vector<FDiagnostic> DiagsNegZero;
    auto NegZeroVal = ParseJson5("-0.0", Limits, DiagsNegZero);
    TestTrue(TEXT("Negative zero parses successfully"), NegZeroVal.has_value());
    if (NegZeroVal.has_value() && NegZeroVal->IsNumber())
    {
        TestTrue(TEXT("-0.0 canonicalized to +0.0"), !std::signbit(NegZeroVal->AsNumber()));
    }

    // 7. Full conformance determinism and Cyrillic / multiline string test (PCC-12)
    std::string ConformanceDoc = R"(
        // Subsystem configuration fixture
        {
            title: "Тестовый пакет",
            version: 1,
            ratio: 3.14159,
            hex_id: 0x1F,
            exp_val: 1e3,
            neg_zero: -0.0,
            multiline: "Строка 1 \
Строка 2",
            tags: ["тест", "кириллица", 100,],
            nested: { flag: true, empty_val: null },
        }
    )";

    std::vector<FDiagnostic> DiagsConf1;
    auto ConfVal1 = ParseJson5(ConformanceDoc, Limits, DiagsConf1);
    TestTrue(TEXT("Conformance doc parses successfully"), ConfVal1.has_value() && DiagsConf1.empty());

    std::vector<FDiagnostic> DiagsConf2;
    auto ConfVal2 = ParseJson5(ConformanceDoc, Limits, DiagsConf2);
    TestTrue(TEXT("Repeat parse produces deterministic tree"), ConfVal2.has_value() && DiagsConf2.empty());
    if (ConfVal1.has_value() && ConfVal2.has_value())
    {
        TestTrue(TEXT("Trees are equal"), *ConfVal1 == *ConfVal2);
    }

    std::vector<FDiagnostic> DiagsNumbers;
    auto MinHex = ParseJson5("-0x8000000000000000", Limits, DiagsNumbers);
    TestTrue(TEXT("INT64_MIN hexadecimal form parses"), MinHex.has_value() && MinHex->AsInteger() == std::numeric_limits<std::int64_t>::min());
    auto LeadingDot = ParseJson5(".5", Limits, DiagsNumbers);
    auto TrailingDot = ParseJson5("1.", Limits, DiagsNumbers);
    TestTrue(TEXT("JSON5 leading-dot number parses"), LeadingDot.has_value() && LeadingDot->AsNumber() == 0.5);
    TestTrue(TEXT("JSON5 trailing-dot number parses"), TrailingDot.has_value() && TrailingDot->AsNumber() == 1.0);
    for (const std::string_view InvalidNumber : { std::string_view("."), std::string_view("01"), std::string_view("1e+") })
    {
        std::vector<FDiagnostic> InvalidNumberDiagnostics;
        auto InvalidNumberValue = ParseJson5(InvalidNumber, Limits, InvalidNumberDiagnostics);
        TestFalse(TEXT("Malformed numeric form is rejected"), InvalidNumberValue.has_value());
        TestTrue(
            TEXT("Malformed numeric form has a typed diagnostic"),
            !InvalidNumberDiagnostics.empty()
                && InvalidNumberDiagnostics[0].Code == "core:diagnostic.json5.invalid_number");
    }

    std::vector<FDiagnostic> DiagsLocations;
    auto Located = ParseJson5Document("{ outer: [7] } // after", Limits, DiagsLocations);
    TestTrue(TEXT("Parsed document retains source locations"), Located.has_value());
    if (Located.has_value())
    {
        const FParsedLocation* RootLocation = Located->FindLocation("");
        const FParsedLocation* ValueLocation = Located->FindLocation("/outer/0");
        const FParsedLocation* ArrayLocation = Located->FindLocation("/outer");
        TestTrue(TEXT("Root location exists"), RootLocation != nullptr);
        TestTrue(TEXT("Nested value location exists"), ValueLocation != nullptr);
        TestTrue(TEXT("Property key location exists"), ArrayLocation != nullptr && ArrayLocation->KeySpan.has_value());
        if (RootLocation != nullptr)
        {
            TestEqual(TEXT("Root span ends at closing brace"), RootLocation->ValueSpan.EndColumn, 15u);
        }
    }

    return true;
}

#endif
