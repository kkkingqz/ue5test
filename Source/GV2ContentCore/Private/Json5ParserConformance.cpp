#include "GV2ContentCore/Testing/Json5ParserConformance.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunJson5ParserConformance()
{
    FParseLimits Limits;

    // 1. Scalar roots
    std::vector<FDiagnostic> Diags1;
    auto NullVal = ParseJson5("null", Limits, Diags1);
    if (!NullVal.has_value() || !NullVal->IsNull())
    {
        return "json5_parser.scalar_null_root";
    }

    std::vector<FDiagnostic> Diags2;
    auto BoolVal = ParseJson5("true", Limits, Diags2);
    if (!BoolVal.has_value() || !BoolVal->IsBoolean() || !BoolVal->AsBoolean())
    {
        return "json5_parser.scalar_bool_root";
    }

    std::vector<FDiagnostic> Diags3;
    auto NumVal = ParseJson5("0x10", Limits, Diags3);
    if (!NumVal.has_value() || !NumVal->IsInteger() || NumVal->AsInteger() != 16)
    {
        return "json5_parser.scalar_hex_int_root";
    }

    // 2. Complex object & array tree
    std::string_view InputDoc = "{\n"
                                "  $schema: 'http://json-schema.org/draft-07/schema#',\n"
                                "  package_id: 'test_pkg',\n"
                                "  values: [10, 20.5, true, null, /* comment */ { nested: 'val' },],\n"
                                "}";

    std::vector<FDiagnostic> DiagsDoc;
    auto RootDoc = ParseJson5(InputDoc, Limits, DiagsDoc);
    if (!RootDoc.has_value() || !DiagsDoc.empty() || !RootDoc->IsObject())
    {
        return "json5_parser.complex_document_parse";
    }

    const FValue::FObject& Obj = RootDoc->AsObject();
    if (Obj.size() != 3
        || Obj[0].first != "$schema"
        || Obj[1].first != "package_id"
        || Obj[2].first != "values"
        || !Obj[2].second.IsArray()
        || Obj[2].second.AsArray().size() != 5)
    {
        return "json5_parser.complex_document_tree_structure";
    }

    // 3. Nesting depth limit failure
    FParseLimits DepthLimits;
    DepthLimits.MaxNestingDepth = 2;
    std::vector<FDiagnostic> DiagsDepth;
    auto DepthFail = ParseJson5("{ a: { b: { c: 1 } } }", DepthLimits, DiagsDepth);
    if (DepthFail.has_value()
        || DiagsDepth.empty()
        || DiagsDepth[0].Code != "core:diagnostic.json5.limit.nesting_depth")
    {
        return "json5_parser.nesting_depth_limit_failure";
    }

    // 4. Unexpected EOF
    std::vector<FDiagnostic> DiagsEof;
    auto EofFail = ParseJson5("{ a: [ 1, 2", Limits, DiagsEof);
    if (EofFail.has_value()
        || DiagsEof.empty()
        || DiagsEof[0].Code != "core:diagnostic.json5.unexpected_eof")
    {
        return "json5_parser.unexpected_eof";
    }

    // 5. Duplicate key error
    std::vector<FDiagnostic> DiagsDup;
    auto DupFail = ParseJson5("{ dup_key: 1, dup_key: 2 }", Limits, DiagsDup);
    if (DupFail.has_value()
        || DiagsDup.empty()
        || DiagsDup[0].Code != "core:diagnostic.json5.duplicate_key"
        || !DiagsDup[0].RelatedSpan.has_value())
    {
        return "json5_parser.duplicate_key_error_with_related_span";
    }

    // 6. Numeric normalization and rejection
    std::vector<FDiagnostic> DiagsNan;
    auto NanFail = ParseJson5("NaN", Limits, DiagsNan);
    if (NanFail.has_value()
        || DiagsNan.empty()
        || DiagsNan[0].Code != "core:diagnostic.json5.invalid_number")
    {
        return "json5_parser.nan_rejected";
    }

    std::vector<FDiagnostic> DiagsNegZero;
    auto NegZeroVal = ParseJson5("-0.0", Limits, DiagsNegZero);
    if (!NegZeroVal.has_value()
        || !NegZeroVal->IsNumber()
        || std::signbit(NegZeroVal->AsNumber()))
    {
        return "json5_parser.negative_zero_canonicalized";
    }

    // 7. Full conformance determinism and Cyrillic / multiline string test
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
    if (!ConfVal1.has_value() || !DiagsConf1.empty())
    {
        return "json5_parser.conformance_document_parse";
    }

    std::vector<FDiagnostic> DiagsConf2;
    auto ConfVal2 = ParseJson5(ConformanceDoc, Limits, DiagsConf2);
    if (!ConfVal2.has_value() || !DiagsConf2.empty() || (*ConfVal1 != *ConfVal2))
    {
        return "json5_parser.conformance_document_deterministic";
    }

    // 8. Number forms: INT64_MIN hex, leading dot, trailing dot, invalid forms
    std::vector<FDiagnostic> DiagsNumbers;
    auto MinHex = ParseJson5("-0x8000000000000000", Limits, DiagsNumbers);
    if (!MinHex.has_value() || MinHex->AsInteger() != std::numeric_limits<std::int64_t>::min())
    {
        return "json5_parser.min_int64_hex";
    }

    auto LeadingDot = ParseJson5(".5", Limits, DiagsNumbers);
    auto TrailingDot = ParseJson5("1.", Limits, DiagsNumbers);
    if (!LeadingDot.has_value() || LeadingDot->AsNumber() != 0.5
        || !TrailingDot.has_value() || TrailingDot->AsNumber() != 1.0)
    {
        return "json5_parser.leading_trailing_dot_numbers";
    }

    for (const std::string_view InvalidNumber : { std::string_view("."), std::string_view("01"), std::string_view("1e+") })
    {
        std::vector<FDiagnostic> InvalidNumberDiagnostics;
        auto InvalidNumberValue = ParseJson5(InvalidNumber, Limits, InvalidNumberDiagnostics);
        if (InvalidNumberValue.has_value()
            || InvalidNumberDiagnostics.empty()
            || InvalidNumberDiagnostics[0].Code != "core:diagnostic.json5.invalid_number")
        {
            return "json5_parser.malformed_number_rejected";
        }
    }

    // 9. Source locations tracking
    std::vector<FDiagnostic> DiagsLocations;
    auto Located = ParseJson5Document("{ outer: [7] } // after", Limits, DiagsLocations);
    if (!Located.has_value())
    {
        return "json5_parser.parse_document_source_locations";
    }

    const FParsedLocation* RootLocation = Located->FindLocation("");
    const FParsedLocation* ValueLocation = Located->FindLocation("/outer/0");
    const FParsedLocation* ArrayLocation = Located->FindLocation("/outer");
    if (RootLocation == nullptr
        || ValueLocation == nullptr
        || ArrayLocation == nullptr
        || !ArrayLocation->KeySpan.has_value()
        || RootLocation->ValueSpan.EndColumn != 15u)
    {
        return "json5_parser.source_locations_spans_content";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
