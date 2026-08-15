#include "GV2ContentCore/Testing/ParseLimitsConformance.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunParseLimitsConformance()
{
    FParseLimits Limits;
    Limits.MaxFileSizeBytes = 100;
    Limits.MaxNestingDepth = 5;
    Limits.MaxStringLengthBytes = 20;
    Limits.MaxContainerEntries = 10;

    // 1. Valid UTF-8 without BOM
    std::vector<FDiagnostic> Diags;
    std::string_view ValidInput = "{\n  \"key\": \"значение\"\n}";
    auto ValidRes = ValidateUtf8AndLimits(ValidInput, Limits, Diags);
    if (!ValidRes.has_value() || ValidRes->bHasBOM || ValidRes->CleanedView != ValidInput || !Diags.empty())
    {
        return "parse_limits.valid_utf8_without_bom";
    }

    // 2. Valid UTF-8 with BOM
    Diags.clear();
    std::string BomInput = "\xEF\xBB\xBF{\n  \"key\": \"val\"\n}";
    auto BomRes = ValidateUtf8AndLimits(BomInput, Limits, Diags);
    if (!BomRes.has_value()
        || !BomRes->bHasBOM
        || BomRes->CleanedView.size() != BomInput.size() - 3
        || BomRes->ByteOffsetShift != static_cast<size_t>(3)
        || !Diags.empty())
    {
        return "parse_limits.valid_utf8_with_bom";
    }

    // 3. Invalid UTF-8 sequence
    Diags.clear();
    std::string InvalidUtf8Input = "{\n  \"key\": \"\xFF\xFF\"\n}";
    auto InvalidUtf8Res = ValidateUtf8AndLimits(InvalidUtf8Input, Limits, Diags);
    if (InvalidUtf8Res.has_value()
        || Diags.size() != 1
        || Diags[0].Code != "core:diagnostic.json5.invalid_utf8")
    {
        return "parse_limits.invalid_utf8_sequence_rejected";
    }

    // 4. File size limit exceeded
    Diags.clear();
    std::string LargeInput(150, 'a');
    auto LargeRes = ValidateUtf8AndLimits(LargeInput, Limits, Diags);
    if (LargeRes.has_value()
        || Diags.size() != 1
        || Diags[0].Code != "core:diagnostic.json5.limit.file_size")
    {
        return "parse_limits.file_size_limit_exceeded";
    }

    // 5. Nesting depth limit helper
    Diags.clear();
    FSourceSpan Span{ 1, 1, 1, 5 };
    bool bDepthOk = CheckNestingDepth(6, Limits, Span, Diags);
    if (bDepthOk || Diags.size() != 1 || Diags[0].Code != "core:diagnostic.json5.limit.nesting_depth")
    {
        return "parse_limits.check_nesting_depth_helper";
    }

    // 6. String length limit helper
    Diags.clear();
    bool bStringOk = CheckStringLength(25, Limits, Span, Diags);
    if (bStringOk || Diags.size() != 1 || Diags[0].Code != "core:diagnostic.json5.limit.string_length")
    {
        return "parse_limits.check_string_length_helper";
    }

    // 7. Container entries limit helper
    Diags.clear();
    bool bContainerOk = CheckContainerEntries(15, Limits, Span, Diags);
    if (bContainerOk || Diags.size() != 1 || Diags[0].Code != "core:diagnostic.json5.limit.container_entries")
    {
        return "parse_limits.check_container_entries_helper";
    }

    // 8. Unsafe nesting depth config rejected
    Diags.clear();
    FParseLimits UnsafeLimits;
    UnsafeLimits.MaxNestingDepth = FParseLimits::MaxSupportedNestingDepth + 1;
    auto UnsafeResult = ValidateUtf8AndLimits("{}", UnsafeLimits, Diags);
    if (UnsafeResult.has_value()
        || Diags.empty()
        || Diags[0].Code != "core:diagnostic.json5.limit.invalid_configuration")
    {
        return "parse_limits.unsafe_nesting_configuration_rejected";
    }

    // 9. Canonical limits constants
    FParseLimits CanonicalLimits;
    if (CanonicalLimits.MaxNestingDepth != 64
        || FParseLimits::MaxSupportedNestingDepth != 64
        || CanonicalLimits.MaxContainerEntries != 10000)
    {
        return "parse_limits.canonical_limit_constants";
    }

    // 10. Nesting depth 65 rejected at parse time
    std::string DeepJson;
    for (int i = 0; i < 65; ++i) DeepJson += "[";
    DeepJson += "1";
    for (int i = 0; i < 65; ++i) DeepJson += "]";
    Diags.clear();
    auto DeepParseResult = ParseJson5(DeepJson, CanonicalLimits, Diags);
    if (DeepParseResult.has_value()
        || Diags.empty()
        || Diags[0].Code != "core:diagnostic.json5.limit.nesting_depth")
    {
        return "parse_limits.parse_time_nesting_depth_65_rejected";
    }

    // 11. Nesting depth 64 parsed successfully
    std::string ValidDeepJson;
    for (int i = 0; i < 64; ++i) ValidDeepJson += "[";
    ValidDeepJson += "1";
    for (int i = 0; i < 64; ++i) ValidDeepJson += "]";
    Diags.clear();
    auto ValidDeepResult = ParseJson5(ValidDeepJson, CanonicalLimits, Diags);
    if (!ValidDeepResult.has_value() || !Diags.empty())
    {
        return "parse_limits.parse_time_nesting_depth_64_accepted";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
