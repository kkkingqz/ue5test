#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "Misc/AutomationTest.h"
#include <map>
#include <type_traits>

namespace
{
class FMemoryContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreBuildResultTest,
    "GV2.Runtime.ContentCore.BuildResultAPI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreBuildResultTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;
    static_assert(!std::is_assignable_v<FCandidate&, const FCandidate&>);

    // 1. Success Build Result
    FCandidate Candidate(
        FValue::MakeObject({ { "version", FValue("1.0.0") } }),
        ECandidateStage::RepositoryResolved);
    FBuildResult SuccessResult = FBuildResult::Success(Candidate);

    TestTrue(TEXT("SuccessResult is success"), SuccessResult.IsSuccess());
    TestFalse(TEXT("SuccessResult is not failure"), SuccessResult.IsFailure());
    TestEqual(TEXT("Candidate matches"), SuccessResult.GetCandidate(), Candidate);
    TestFalse(TEXT("stage tag without frozen snapshot is not publishable"),
        SuccessResult.GetCandidate().IsPublishable());

    bool bCaughtLogicErrorOnDiagnostics = false;
    try
    {
        SuccessResult.GetDiagnostics();
    }
    catch (const std::logic_error&)
    {
        bCaughtLogicErrorOnDiagnostics = true;
    }
    TestTrue(TEXT("Accessing diagnostics on success throws std::logic_error"), bCaughtLogicErrorOnDiagnostics);

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

    TestFalse(TEXT("FailureResult is not success"), FailureResult.IsSuccess());
    TestTrue(TEXT("FailureResult is failure"), FailureResult.IsFailure());
    TestEqual(TEXT("Diagnostics list size is 2"), FailureResult.GetDiagnostics().size(), static_cast<size_t>(2));

    // Check automatic sorting
    TestEqual(TEXT("First sorted diagnostic is line 2"), FailureResult.GetDiagnostics()[0].Span->StartLine, 2u);
    TestEqual(TEXT("Second sorted diagnostic is line 10"), FailureResult.GetDiagnostics()[1].Span->StartLine, 10u);

    bool bCaughtLogicErrorOnCandidate = false;
    try
    {
        FailureResult.GetCandidate();
    }
    catch (const std::logic_error&)
    {
        bCaughtLogicErrorOnCandidate = true;
    }
    TestTrue(TEXT("Accessing candidate on failure throws std::logic_error"), bCaughtLogicErrorOnCandidate);

    bool bRejectedEmptyFailure = false;
    try
    {
        FBuildResult::Failure({});
    }
    catch (const std::invalid_argument&)
    {
        bRejectedEmptyFailure = true;
    }
    TestTrue(TEXT("Failure state requires diagnostics"), bRejectedEmptyFailure);

    const FPackageDescriptor EmptyCore("core", "core", 0);
    const FBuildResult EmptyCoreResult = BuildRepository({ EmptyCore });
    TestTrue(TEXT("BuildRepository accepts the empty core package"), EmptyCoreResult.IsSuccess());
    if (EmptyCoreResult.IsSuccess())
    {
        TestTrue(TEXT("resolved empty candidate is publishable"),
            EmptyCoreResult.GetCandidate().GetStage() == ECandidateStage::RepositoryResolved
            && EmptyCoreResult.GetCandidate().IsPublishable());
        TestTrue(TEXT("Empty core candidate root is an object"), EmptyCoreResult.GetCandidate().GetRootValue().IsObject());
        const FValue* EmptyDefinitions = EmptyCoreResult.GetCandidate().GetRootValue().FindField("definitions");
        TestTrue(TEXT("Empty core candidate contains an empty definitions array"),
            EmptyDefinitions != nullptr && EmptyDefinitions->IsArray() && EmptyDefinitions->AsArray().empty());
    }

    const FBuildResult InvalidPackageResult = BuildRepository({ FPackageDescriptor("mod", "mod", 1) });
    TestTrue(TEXT("Invalid package set returns only diagnostics"), InvalidPackageResult.IsFailure());

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
    TestTrue(TEXT("Schema-valid core source produces an M3 candidate"), ParsedResult.IsSuccess());
    if (ParsedResult.IsSuccess())
    {
        TestTrue(TEXT("M4 candidate is a publishable repository snapshot"),
            ParsedResult.GetCandidate().GetStage() == ECandidateStage::RepositoryResolved
            && ParsedResult.GetCandidate().IsPublishable());
        const FValue* Definitions = ParsedResult.GetCandidate().GetRootValue().FindField("definitions");
        TestTrue(TEXT("M3 candidate contains a definitions array"),
            Definitions != nullptr && Definitions->IsArray() && Definitions->AsArray().empty());
    }
    TestEqual(TEXT("Definition and schema sources are both read"), SourceProvider.Reads.size(), static_cast<size_t>(2));
    if (SourceProvider.Reads.size() == 2)
    {
        TestEqual(TEXT("Sources are read in relative-path order"), SourceProvider.Reads[0], std::string("core/definitions/items.json5"));
        TestEqual(TEXT("Schema source participates in repository parse"), SourceProvider.Reads[1], std::string("core/schemas/item.schema.json5"));
    }

    FMemoryContentSourceProvider InvalidSourceProvider;
    InvalidSourceProvider.Sources.emplace("core/definitions/items.json5", "{ broken:");
    FBuildOptions InvalidSourceOptions;
    InvalidSourceOptions.SourceProvider = &InvalidSourceProvider;
    const FBuildResult InvalidSourceResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" }) },
        InvalidSourceOptions);
    TestTrue(TEXT("Repository build rejects malformed JSON5"), InvalidSourceResult.IsFailure());
    if (InvalidSourceResult.IsFailure())
    {
        const FDiagnostic& ParseDiagnostic = InvalidSourceResult.GetDiagnostics()[0];
        TestEqual(TEXT("Repository exposes parser diagnostic"), ParseDiagnostic.Code, std::string("core:diagnostic.json5.unexpected_eof"));
        TestTrue(TEXT("Parser diagnostic retains package context"), ParseDiagnostic.PackageId == std::optional<std::string>("core"));
        TestTrue(TEXT("Parser diagnostic retains source context"), ParseDiagnostic.RelativeSource == std::optional<std::string>("definitions/items.json5"));
    }

    return true;
}

#endif
