#include "GV2ContentCore/Testing/BuildResultConformance.h"

#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/RepositoryBuilder.h"

#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace GV2ContentCore::Testing
{
namespace
{
class FMemoryContentSourceProvider final : public IContentSourceProvider
{
public:
    std::map<std::string, std::string> Sources;
    mutable std::vector<std::string> Reads;

    std::optional<std::string> ReadSource(
        const std::string_view PackageId,
        const std::string_view RelativeSource) const override
    {
        const std::string Key = std::string(PackageId) + "/" + std::string(RelativeSource);
        Reads.push_back(Key);
        const auto Found = Sources.find(Key);
        return Found == Sources.end() ? std::nullopt : std::optional<std::string>(Found->second);
    }
};
} // namespace

std::string RunBuildResultConformance()
{
    static_assert(!std::is_assignable_v<FCandidate&, const FCandidate&>);

    // 1. Success Build Result
    FCandidate Candidate(
        FValue::MakeObject({ { "version", FValue("1.0.0") } }),
        ECandidateStage::RepositoryResolved);
    FBuildResult SuccessResult = FBuildResult::Success(Candidate);

    if (!SuccessResult.IsSuccess() || SuccessResult.IsFailure())
    {
        return "build_result.success_state";
    }
    if (SuccessResult.GetCandidate() != Candidate)
    {
        return "build_result.candidate_matches";
    }
    if (SuccessResult.GetCandidate().IsPublishable())
    {
        return "build_result.stage_tag_without_frozen_snapshot_not_publishable";
    }

    bool bCaughtLogicErrorOnDiagnostics = false;
    try
    {
        SuccessResult.GetDiagnostics();
    }
    catch (const std::logic_error&)
    {
        bCaughtLogicErrorOnDiagnostics = true;
    }
    if (!bCaughtLogicErrorOnDiagnostics)
    {
        return "build_result.success_get_diagnostics_throws_logic_error";
    }

    // 2. Failure Build Result (with automatic sorting of diagnostics)
    FDiagnostic Diag1;
    Diag1.Code = "core:diagnostic.test.second";
    Diag1.Severity = EDiagnosticSeverity::Error;
    Diag1.Message = "Second error";
    Diag1.Span = FSourceSpan{ 10, 1, 10, 5 };

    FDiagnostic Diag2;
    Diag2.Code = "core:diagnostic.test.first";
    Diag2.Severity = EDiagnosticSeverity::Error;
    Diag2.Message = "First error";
    Diag2.Span = FSourceSpan{ 2, 1, 2, 5 };

    FBuildResult FailureResult = FBuildResult::Failure({ Diag1, Diag2 });

    if (FailureResult.IsSuccess() || !FailureResult.IsFailure())
    {
        return "build_result.failure_state";
    }
    if (FailureResult.GetDiagnostics().size() != 2)
    {
        return "build_result.failure_diagnostics_size";
    }
    if (!FailureResult.GetDiagnostics()[0].Span.has_value()
        || FailureResult.GetDiagnostics()[0].Span->StartLine != 2u
        || !FailureResult.GetDiagnostics()[1].Span.has_value()
        || FailureResult.GetDiagnostics()[1].Span->StartLine != 10u)
    {
        return "build_result.failure_diagnostics_sorting";
    }

    bool bCaughtLogicErrorOnCandidate = false;
    try
    {
        FailureResult.GetCandidate();
    }
    catch (const std::logic_error&)
    {
        bCaughtLogicErrorOnCandidate = true;
    }
    if (!bCaughtLogicErrorOnCandidate)
    {
        return "build_result.failure_get_candidate_throws_logic_error";
    }

    bool bRejectedEmptyFailure = false;
    try
    {
        FBuildResult::Failure({});
    }
    catch (const std::invalid_argument&)
    {
        bRejectedEmptyFailure = true;
    }
    if (!bRejectedEmptyFailure)
    {
        return "build_result.empty_failure_throws_invalid_argument";
    }

    // 3. Repository build with empty core package
    const FPackageDescriptor EmptyCore("core", "core", 0);
    const FBuildResult EmptyCoreResult = BuildRepository({ EmptyCore });
    if (!EmptyCoreResult.IsSuccess())
    {
        return "build_result.empty_core_build_success";
    }
    if (EmptyCoreResult.GetCandidate().GetStage() != ECandidateStage::RepositoryResolved
        || !EmptyCoreResult.GetCandidate().IsPublishable()
        || !EmptyCoreResult.GetCandidate().GetRootValue().IsObject())
    {
        return "build_result.empty_core_candidate_publishable";
    }
    const FValue* EmptyDefinitions = EmptyCoreResult.GetCandidate().GetRootValue().FindField("definitions");
    if (EmptyDefinitions == nullptr || !EmptyDefinitions->IsArray() || !EmptyDefinitions->AsArray().empty())
    {
        return "build_result.empty_core_definitions_array";
    }

    // 4. Invalid package set
    const FBuildResult InvalidPackageResult = BuildRepository({ FPackageDescriptor("mod", "mod", 1) });
    if (!InvalidPackageResult.IsFailure())
    {
        return "build_result.invalid_package_set_fails";
    }

    // 5. Source provider ordered reading
    FMemoryContentSourceProvider SourceProvider;
    SourceProvider.Sources.emplace(
        "core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: [] }");
    SourceProvider.Sources.emplace(
        "core/schemas/item.schema.json5",
        "{ id: 'core:schema.item', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    const FPackageDescriptor ParsedCore(
        "core",
        "core",
        0,
        { "definitions/items.json5" },
        { FSchemaBinding("item", 1, "core:schema.item", "schemas/item.schema.json5") });
    FBuildOptions ParseOptions;
    ParseOptions.SourceProvider = &SourceProvider;
    const FBuildResult ParsedResult = BuildRepository({ ParsedCore }, ParseOptions);
    if (!ParsedResult.IsSuccess()
        || ParsedResult.GetCandidate().GetStage() != ECandidateStage::RepositoryResolved
        || !ParsedResult.GetCandidate().IsPublishable())
    {
        return "build_result.parsed_core_publishable";
    }
    if (SourceProvider.Reads.size() != 2
        || SourceProvider.Reads[0] != "core/definitions/items.json5"
        || SourceProvider.Reads[1] != "core/schemas/item.schema.json5")
    {
        return "build_result.source_provider_reads_ordered_and_schema_included";
    }

    // 6. Malformed JSON5 diagnostic retention
    FMemoryContentSourceProvider InvalidSourceProvider;
    InvalidSourceProvider.Sources.emplace("core/definitions/items.json5", "{ broken:");
    FBuildOptions InvalidSourceOptions;
    InvalidSourceOptions.SourceProvider = &InvalidSourceProvider;
    const FBuildResult InvalidSourceResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" }) },
        InvalidSourceOptions);
    if (!InvalidSourceResult.IsFailure())
    {
        return "build_result.malformed_json5_fails";
    }
    if (InvalidSourceResult.GetDiagnostics().empty())
    {
        return "build_result.malformed_json5_diagnostics_not_empty";
    }
    const FDiagnostic& ParseDiagnostic = InvalidSourceResult.GetDiagnostics()[0];
    if (ParseDiagnostic.Code != "core:diagnostic.json5.unexpected_eof"
        || ParseDiagnostic.PackageId != std::optional<std::string>("core")
        || ParseDiagnostic.RelativeSource != std::optional<std::string>("definitions/items.json5"))
    {
        return "build_result.malformed_json5_retains_diagnostic_context";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
