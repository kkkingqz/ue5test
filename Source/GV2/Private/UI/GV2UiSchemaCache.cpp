#include "UI/GV2UiSchemaCache.h"
#include "UI/GV2PresentationAuthorityProbe.h"

#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/StableId.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

FGV2UiSchemaCache::FGV2UiSchemaCache(TArray<FGV2SchemaPackageRoot> InPackageRoots)
    : PackageRoots(MoveTemp(InPackageRoots))
{
    DiscoverAll();
}

// PAH-04: pre_ready_discovery -- only called from this constructor, only called
// from RebuildSchemaCacheForSession(), only called from StartSession().
void FGV2UiSchemaCache::DiscoverAll()
{
    for (const FGV2SchemaPackageRoot& Root : PackageRoots)
    {
        const std::filesystem::path RootPath(TCHAR_TO_UTF8(*Root.RootDirectory));
        std::error_code Ec;
        if (!std::filesystem::is_directory(RootPath, Ec) || Ec)
        {
            continue;
        }

        for (const auto& Entry : std::filesystem::recursive_directory_iterator(
                 RootPath, std::filesystem::directory_options::skip_permission_denied, Ec))
        {
            if (!Entry.is_regular_file() || Entry.path().extension() != ".json5")
            {
                continue;
            }
            static const std::string Suffix = ".schema.json5";
            const std::string FileName = Entry.path().filename().string();
            if (FileName.size() < Suffix.size()
                || FileName.compare(FileName.size() - Suffix.size(), Suffix.size(), Suffix) != 0)
            {
                continue;
            }

            std::ifstream File(Entry.path(), std::ios::binary);
            if (!File)
            {
                continue;
            }
            std::ostringstream Buffer;
            Buffer << File.rdbuf();
            const std::string Source = Buffer.str();

            const std::string RelativeSource = std::filesystem::relative(Entry.path(), RootPath, Ec).generic_string();
            std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
            std::optional<GV2ContentCore::FParsedDocument> Parsed = GV2ContentCore::ParseJson5Document(
                Source, GV2ContentCore::FParseLimits{}, Diagnostics);
            if (!Parsed.has_value() || !Diagnostics.empty())
            {
                // A malformed schema file is a content authoring error surfaced elsewhere
                // (validate_docs.py / gv2-headless --check-scripts content build); this cache
                // simply does not register it, so schema_id lookups against it report
                // "unknown schema" rather than silently using a broken tree.
                continue;
            }

            const GV2ContentCore::FValue& SchemaRoot = Parsed->GetRootValue();
            if (!SchemaRoot.IsObject())
            {
                continue;
            }
            const GV2ContentCore::FValue* IdField = SchemaRoot.FindField("id");
            if (IdField == nullptr || !IdField->IsString())
            {
                continue;
            }

            // PSC-04: *.schema.json5 is shared with repository content-definition schemas
            // (definition_type-bound, validated by FSchemaRegistry at repository build --
            // e.g. GameData/core/schemas/actor_v1.schema.json5), which are not ui_field/
            // ui_value schemas and were never meant to reach CompileUiFieldSpec. Only a
            // schema_domain-bearing file belongs to this cache; a definition_type file (or
            // any other schema.json5 lacking schema_domain) is silently not registered here,
            // exactly like a malformed file -- it was already unreachable via any production
            // lookup (nothing queries a ui_field schema_id for it), CompileAll() just made
            // the distinction load-bearing instead of latent.
            if (SchemaRoot.FindField("schema_domain") == nullptr)
            {
                continue;
            }

            // DCA-19: a schema's own `id` namespace must match the package it was
            // physically discovered under -- otherwise a file placed in one package's
            // directory could silently register itself into another package's
            // namespace (accepted before this check, purely because nothing compared
            // the two). Treated exactly like a malformed schema: not registered, so
            // lookups against its declared id report "unknown schema".
            GV2ContentCore::FStableIdView IdView;
            const std::string RootPackageIdUtf8 = TCHAR_TO_UTF8(*Root.PackageId);
            if (!GV2ContentCore::FStableId::Parse(IdField->AsString(), IdView)
                || IdView.Namespace != RootPackageIdUtf8)
            {
                continue;
            }

            auto Document = std::make_shared<const GV2ContentCore::FParsedDocument>(MoveTemp(*Parsed));
            Resolver.RegisterUiSchemaDocument(IdField->AsString(), Document, TCHAR_TO_UTF8(*Root.PackageId), RelativeSource);
            DiscoveredSchemaIds.AddUnique(UTF8_TO_TCHAR(IdField->AsString().c_str()));
        }
    }
    DiscoveredSchemaIds.Sort();
}

// PAH-08: phase=prepare -- called only from FGV2SessionContentCandidate::Build(), during
// StartSession()'s bootstrap, before the Lua VM/session can reach Ready.
bool FGV2UiSchemaCache::CompileAll(FString& OutError) const
{
    for (const FString& SchemaId : DiscoveredSchemaIds)
    {
        FString CompileError;
        if (GetCompiledSchema(TCHAR_TO_UTF8(*SchemaId), CompileError) == nullptr)
        {
            OutError = CompileError;
            return false;
        }
    }
    return true;
}

// PAH-08: phase=authority
GV2ContentCore::FCompiledUiFieldSpecPtr FGV2UiSchemaCache::GetCompiledSchema(
    const std::string& SchemaId,
    FString& OutError) const
{
    GV2_NOTE_AUTHORITY_RESOLVE();
    if (const auto CacheIt = CompiledCache.find(SchemaId); CacheIt != CompiledCache.end())
    {
        return CacheIt->second;
    }

    const std::optional<GV2ContentCore::FResolvedUiSchema> Resolved = Resolver.FindUiSchema(SchemaId);
    if (!Resolved.has_value() || Resolved->RootSpec == nullptr)
    {
        OutError = FString::Printf(TEXT("unknown schema_id '%s' (no *.schema.json5 declares this id)"), UTF8_TO_TCHAR(SchemaId.c_str()));
        return nullptr;
    }

    GV2ContentCore::FValidationDiagnosticContext Context;
    Context.SchemaId = SchemaId;
    Context.PackageId = Resolved->PackageId;
    Context.RelativeSource = Resolved->RelativeSource;
    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    GV2ContentCore::FCompiledUiFieldSpecPtr Compiled = GV2ContentCore::CompileUiFieldSpec(
        *Resolved->RootSpec, Resolved->Document, "root", Context, Diagnostics, &Resolver);
    if (Compiled == nullptr || !Diagnostics.empty())
    {
        OutError = FString::Printf(
            TEXT("schema '%s' failed to compile (%d diagnostic(s))"),
            UTF8_TO_TCHAR(SchemaId.c_str()),
            static_cast<int32>(Diagnostics.size()));
        return nullptr;
    }

    CompiledCache.emplace(SchemaId, Compiled);
    return Compiled;
}
