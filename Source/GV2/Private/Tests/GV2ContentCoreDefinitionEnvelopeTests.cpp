#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "Misc/AutomationTest.h"

#include <map>

namespace
{
class FDefinitionEnvelopeSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, std::string> Sources;

    std::optional<std::string> ReadSource(
        const std::string_view PackageId,
        const std::string_view RelativeSource) const override
    {
        const auto Found = Sources.find(std::string(PackageId) + "/" + std::string(RelativeSource));
        return Found == Sources.end() ? std::nullopt : std::optional<std::string>(Found->second);
    }
};

std::optional<GV2ContentCore::FDefinitionFile> ParseEnvelope(
    const std::string_view Source,
    const std::string& RelativeSource,
    std::vector<GV2ContentCore::FDiagnostic>& Diagnostics)
{
    using namespace GV2ContentCore;
    auto Document = ParseJson5Document(Source, FParseLimits{}, Diagnostics, "core", 0, RelativeSource);
    if (!Document.has_value()) return std::nullopt;
    return ParseDefinitionFileEnvelope(*Document, "core", 0, RelativeSource, Diagnostics);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreDefinitionEnvelopeTest,
    "GV2.Runtime.ContentCore.DefinitionEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreDefinitionEnvelopeTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    std::vector<FDiagnostic> Diagnostics;
    const auto HasCode = [&Diagnostics](const std::string_view Code)
    {
        for (const FDiagnostic& Diagnostic : Diagnostics)
        {
            if (Diagnostic.Code == Code) return true;
        }
        return false;
    };
    auto Valid = ParseEnvelope(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.iron_sword', data: { price: 10 }, tags: ['weapon', 'melee'], "
        "deprecated: true, extensions: { core: {} } },"
        "{ id: 'core:item.potion', data: null }"
        "], extensions: { core: {} } }",
        "definitions/items.json5",
        Diagnostics);
    TestTrue(TEXT("valid closed definition envelope parses"), Valid.has_value() && Diagnostics.empty());
    if (!Valid.has_value()) return false;
    TestEqual(TEXT("definition type retained"), Valid->GetDefinitionType(), std::string("item"));
    TestEqual(TEXT("schema version retained"), Valid->GetSchemaVersion(), static_cast<std::int64_t>(1));
    TestEqual(TEXT("entries retained"), Valid->GetDefinitions().size(), static_cast<std::size_t>(2));
    TestTrue(TEXT("metadata retained"), Valid->GetDefinitions()[0].IsDeprecated()
        && Valid->GetDefinitions()[0].GetTags().size() == 2);
    TestTrue(TEXT("metadata defaults materialized"), !Valid->GetDefinitions()[1].IsDeprecated()
        && Valid->GetDefinitions()[1].GetTags().empty()
        && Valid->GetDefinitions()[1].GetExtensions().IsObject());
    TestTrue(TEXT("data presence differs from null"), Valid->GetDefinitions()[1].GetData().IsNull());

    Diagnostics.clear();
    auto UnknownRoot = ParseEnvelope(
        "{ schema_version: 1, type: 'item', definitions: [], entries: [] }",
        "definitions/unknown_root.json5",
        Diagnostics);
    TestFalse(TEXT("definition root is closed"), UnknownRoot.has_value());
    TestTrue(TEXT("unknown root code and key span are stable"), !Diagnostics.empty()
        && Diagnostics[0].Code == "core:diagnostic.definition.file.unknown_field"
        && Diagnostics[0].JsonPointer == std::optional<std::string>("/entries")
        && Diagnostics[0].Span.has_value());

    Diagnostics.clear();
    auto InvalidRoot = ParseEnvelope(
        "{ schema_version: 0, type: 'Item', definitions: {}, extensions: [] }",
        "definitions/invalid_root.json5",
        Diagnostics);
    TestFalse(TEXT("invalid required root fields are rejected"), InvalidRoot.has_value());
    TestTrue(TEXT("schema_version must be positive int64"),
        HasCode("core:diagnostic.definition.file.invalid_schema_version"));
    TestTrue(TEXT("type must be canonical segment"),
        HasCode("core:diagnostic.definition.file.invalid_type"));
    TestTrue(TEXT("definitions must be array"),
        HasCode("core:diagnostic.definition.file.invalid_definitions"));
    TestTrue(TEXT("root extension shell must be object"),
        HasCode("core:diagnostic.definition.file.invalid_extensions"));

    Diagnostics.clear();
    auto InvalidEntry = ParseEnvelope(
        "{ schema_version: 1, type: 'item', definitions: ["
        "1,"
        "{ id: 'core:screen.wrong_kind', extra: true, tags: ['same', 'same'], deprecated: 1, extensions: [] },"
        "{ id: 'Core:item.invalid', data: {}, tags: [1] }"
        "] }",
        "definitions/invalid_entry.json5",
        Diagnostics);
    TestFalse(TEXT("invalid entry shell is rejected"), InvalidEntry.has_value());
    TestTrue(TEXT("entry must be object"), HasCode("core:diagnostic.definition.entry.invalid_shape"));
    TestTrue(TEXT("entry is closed"), HasCode("core:diagnostic.definition.entry.unknown_field"));
    TestTrue(TEXT("ID kind must match file type"), HasCode("core:diagnostic.definition.entry.id_kind_mismatch"));
    TestTrue(TEXT("ID must be canonical"), HasCode("core:diagnostic.definition.entry.invalid_id"));
    TestTrue(TEXT("data is required even when null is valid data"), HasCode("core:diagnostic.definition.entry.missing_data"));
    TestTrue(TEXT("tags must be unique"), HasCode("core:diagnostic.definition.entry.duplicate_tag"));
    TestTrue(TEXT("each tag must be string"), HasCode("core:diagnostic.definition.entry.invalid_tag"));
    TestTrue(TEXT("deprecated must be boolean"), HasCode("core:diagnostic.definition.entry.invalid_deprecated"));
    TestTrue(TEXT("entry extension shell must be object"), HasCode("core:diagnostic.definition.entry.invalid_extensions"));

    Diagnostics.clear();
    auto First = ParseEnvelope(
        "{ schema_version: 1, type: 'item', definitions: [{ id: 'core:item.same', data: {} }] }",
        "definitions/a.json5", Diagnostics);
    auto Second = ParseEnvelope(
        "{ schema_version: 1, type: 'item', definitions: [{ id: 'core:item.same', data: {} }] }",
        "definitions/b.json5", Diagnostics);
    TestTrue(TEXT("duplicate fixtures parse individually"), First.has_value() && Second.has_value());
    std::vector<FDefinitionFile> Files;
    if (First.has_value()) Files.push_back(std::move(*First));
    if (Second.has_value()) Files.push_back(std::move(*Second));
    TestFalse(TEXT("duplicate ID within package is rejected"), ValidatePackageDefinitionIds(Files, Diagnostics));
    TestTrue(TEXT("duplicate diagnostic retains both locations"), !Diagnostics.empty()
        && Diagnostics.back().Code == "core:diagnostic.definition.entry.duplicate_id"
        && Diagnostics.back().DefinitionId == std::optional<std::string>("core:item.same")
        && Diagnostics.back().Span.has_value()
        && Diagnostics.back().RelatedSpan.has_value());

    FDefinitionEnvelopeSourceProvider Provider;
    Provider.Sources.emplace("core/schemas/item.json5",
        "{ id: 'core:schema.item.v1', definition_type: 'item', schema_version: 1, "
        "root: { kind: 'object', fields: {} }, semantic_validators: [], extensions: {} }");
    Provider.Sources.emplace("core/definitions/items.json5",
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:screen.bad_kind', data: {} }] }");
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult BuildResult = BuildRepository(
        { FPackageDescriptor("core", "core", 0, { "definitions/items.json5" },
            { FSchemaBinding("item", 1, "core:schema.item.v1", "schemas/item.json5") }) }, Options);
    TestTrue(TEXT("BuildRepository runs definition envelope validation"), BuildResult.IsFailure());
    if (BuildResult.IsFailure())
    {
        TestEqual(TEXT("integrated kind mismatch code"), BuildResult.GetDiagnostics()[0].Code,
            std::string("core:diagnostic.definition.entry.id_kind_mismatch"));
        TestTrue(TEXT("integrated definition provenance"),
            BuildResult.GetDiagnostics()[0].DefinitionId == std::optional<std::string>("core:screen.bad_kind"));
    }

    return true;
}

#endif
