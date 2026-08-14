#include "GV2ContentCore/DefinitionEnvelope.h"

#include "GV2ContentCore/StableId.h"

#include <map>
#include <set>
#include <utility>

namespace GV2ContentCore
{
namespace
{
std::string EscapeJsonPointerToken(const std::string_view Token)
{
    std::string Escaped;
    for (const char Character : Token)
    {
        if (Character == '~') Escaped += "~0";
        else if (Character == '/') Escaped += "~1";
        else Escaped.push_back(Character);
    }
    return Escaped;
}

FDiagnostic MakeDiagnostic(
    std::string Code,
    std::string Message,
    const FParsedDocument& Document,
    const std::string& JsonPointer,
    const std::string& PackageId,
    const std::uint32_t PackageLoadIndex,
    const std::string& RelativeSource,
    const bool bPreferKeySpan = false)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = std::move(Code);
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = PackageId;
    Diagnostic.PackageLoadIndex = PackageLoadIndex;
    Diagnostic.RelativeSource = RelativeSource;
    Diagnostic.JsonPointer = JsonPointer;
    if (const FParsedLocation* Location = Document.FindLocation(JsonPointer))
    {
        Diagnostic.Span = bPreferKeySpan && Location->KeySpan.has_value()
            ? Location->KeySpan
            : std::optional<FSourceSpan>(Location->ValueSpan);
    }
    else if (const FParsedLocation* RootLocation = Document.FindLocation(""))
    {
        Diagnostic.Span = RootLocation->ValueSpan;
    }
    return Diagnostic;
}

void CheckUnknownFields(
    const FValue& Object,
    const std::set<std::string_view>& AllowedFields,
    const FParsedDocument& Document,
    const std::string& Pointer,
    const std::string& DiagnosticCode,
    const std::string& PackageId,
    const std::uint32_t PackageLoadIndex,
    const std::string& RelativeSource,
    std::vector<FDiagnostic>& Diagnostics)
{
    for (const auto& [FieldName, FieldValue] : Object.AsObject())
    {
        if (AllowedFields.contains(FieldName)) continue;
        const std::string FieldPointer = Pointer + "/" + EscapeJsonPointerToken(FieldName);
        Diagnostics.push_back(MakeDiagnostic(
            DiagnosticCode,
            "Unknown definition envelope field: " + FieldName,
            Document, FieldPointer, PackageId, PackageLoadIndex, RelativeSource, true));
    }
}
}

FDefinitionEntry::FDefinitionEntry(
    std::string InId,
    FValue InData,
    std::vector<std::string> InTags,
    const bool bInDeprecated,
    FValue InExtensions,
    const std::size_t InSourceIndex,
    const FSourceSpan InSourceSpan)
    : Id(std::move(InId))
    , Data(std::move(InData))
    , Tags(std::move(InTags))
    , bDeprecated(bInDeprecated)
    , Extensions(std::move(InExtensions))
    , SourceIndex(InSourceIndex)
    , SourceSpan(InSourceSpan)
{
}

FDefinitionFile::FDefinitionFile(
    const std::int64_t InSchemaVersion,
    std::string InDefinitionType,
    std::vector<FDefinitionEntry> InDefinitions,
    FValue InExtensions,
    std::string InPackageId,
    const std::uint32_t InPackageLoadIndex,
    std::string InRelativeSource)
    : SchemaVersion(InSchemaVersion)
    , DefinitionType(std::move(InDefinitionType))
    , Definitions(std::move(InDefinitions))
    , Extensions(std::move(InExtensions))
    , PackageId(std::move(InPackageId))
    , PackageLoadIndex(InPackageLoadIndex)
    , RelativeSource(std::move(InRelativeSource))
{
}

std::optional<FDefinitionFile> ParseDefinitionFileEnvelope(
    const FParsedDocument& Document,
    std::string PackageId,
    const std::uint32_t PackageLoadIndex,
    std::string RelativeSource,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const std::size_t InitialDiagnosticCount = OutDiagnostics.size();
    const FValue& Root = Document.GetRootValue();
    if (!Root.IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.definition.file.invalid_shape",
            "Definition file root must be an object",
            Document, "", PackageId, PackageLoadIndex, RelativeSource));
        return std::nullopt;
    }

    static const std::set<std::string_view> RootFields{
        "schema_version", "type", "definitions", "extensions"
    };
    CheckUnknownFields(
        Root, RootFields, Document, "",
        "core:diagnostic.definition.file.unknown_field",
        PackageId, PackageLoadIndex, RelativeSource, OutDiagnostics);

    const FValue* SchemaVersion = Root.FindField("schema_version");
    const FValue* DefinitionType = Root.FindField("type");
    const FValue* Definitions = Root.FindField("definitions");
    const FValue* Extensions = Root.FindField("extensions");
    if (SchemaVersion == nullptr || !SchemaVersion->IsInteger() || SchemaVersion->AsInteger() <= 0)
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.definition.file.invalid_schema_version",
            "Definition file requires a positive int64 schema_version",
            Document, "/schema_version", PackageId, PackageLoadIndex, RelativeSource));
    }
    if (DefinitionType == nullptr || !DefinitionType->IsString()
        || !FStableId::IsValidSegment(DefinitionType->IsString() ? DefinitionType->AsString() : ""))
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.definition.file.invalid_type",
            "Definition file requires type as a canonical Stable ID kind segment",
            Document, "/type", PackageId, PackageLoadIndex, RelativeSource));
    }
    if (Definitions == nullptr || !Definitions->IsArray())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.definition.file.invalid_definitions",
            "Definition file requires a definitions array",
            Document, "/definitions", PackageId, PackageLoadIndex, RelativeSource));
    }
    if (Extensions != nullptr && !Extensions->IsObject())
    {
        OutDiagnostics.push_back(MakeDiagnostic(
            "core:diagnostic.definition.file.invalid_extensions",
            "Definition file extensions must be an object",
            Document, "/extensions", PackageId, PackageLoadIndex, RelativeSource));
    }

    std::vector<FDefinitionEntry> ParsedDefinitions;
    if (Definitions != nullptr && Definitions->IsArray())
    {
        ParsedDefinitions.reserve(Definitions->AsArray().size());
        static const std::set<std::string_view> EntryFields{
            "id", "data", "tags", "deprecated", "extensions"
        };
        for (std::size_t Index = 0; Index < Definitions->AsArray().size(); ++Index)
        {
            const FValue& Entry = Definitions->AsArray()[Index];
            const std::string Pointer = "/definitions/" + std::to_string(Index);
            if (!Entry.IsObject())
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.definition.entry.invalid_shape",
                    "Definition entry must be an object",
                    Document, Pointer, PackageId, PackageLoadIndex, RelativeSource));
                continue;
            }

            const std::size_t EntryDiagnosticCount = OutDiagnostics.size();
            CheckUnknownFields(
                Entry, EntryFields, Document, Pointer,
                "core:diagnostic.definition.entry.unknown_field",
                PackageId, PackageLoadIndex, RelativeSource, OutDiagnostics);

            const FValue* Id = Entry.FindField("id");
            const FValue* Data = Entry.FindField("data");
            const FValue* Tags = Entry.FindField("tags");
            const FValue* Deprecated = Entry.FindField("deprecated");
            const FValue* EntryExtensions = Entry.FindField("extensions");

            FStableIdView ParsedId;
            if (Id == nullptr || !Id->IsString() || !FStableId::Parse(Id->AsString(), ParsedId))
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.definition.entry.invalid_id",
                    "Definition entry requires a canonical Stable ID",
                    Document, Pointer + "/id", PackageId, PackageLoadIndex, RelativeSource));
            }
            else if (DefinitionType != nullptr && DefinitionType->IsString()
                && FStableId::IsValidSegment(DefinitionType->AsString())
                && ParsedId.Kind != DefinitionType->AsString())
            {
                FDiagnostic Diagnostic = MakeDiagnostic(
                    "core:diagnostic.definition.entry.id_kind_mismatch",
                    "Definition ID kind " + std::string(ParsedId.Kind)
                        + " must match definition file type " + DefinitionType->AsString(),
                    Document, Pointer + "/id", PackageId, PackageLoadIndex, RelativeSource);
                Diagnostic.DefinitionId = Id->AsString();
                OutDiagnostics.push_back(std::move(Diagnostic));
            }
            if (Data == nullptr)
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.definition.entry.missing_data",
                    "Definition entry requires data",
                    Document, Pointer + "/data", PackageId, PackageLoadIndex, RelativeSource));
            }

            std::vector<std::string> ParsedTags;
            std::set<std::string> SeenTags;
            if (Tags != nullptr)
            {
                if (!Tags->IsArray())
                {
                    OutDiagnostics.push_back(MakeDiagnostic(
                        "core:diagnostic.definition.entry.invalid_tags",
                        "Definition entry tags must be an array",
                        Document, Pointer + "/tags", PackageId, PackageLoadIndex, RelativeSource));
                }
                else
                {
                    ParsedTags.reserve(Tags->AsArray().size());
                    for (std::size_t TagIndex = 0; TagIndex < Tags->AsArray().size(); ++TagIndex)
                    {
                        const FValue& Tag = Tags->AsArray()[TagIndex];
                        const std::string TagPointer = Pointer + "/tags/" + std::to_string(TagIndex);
                        if (!Tag.IsString())
                        {
                            OutDiagnostics.push_back(MakeDiagnostic(
                                "core:diagnostic.definition.entry.invalid_tag",
                                "Definition metadata tag must be a string",
                                Document, TagPointer, PackageId, PackageLoadIndex, RelativeSource));
                        }
                        else if (!SeenTags.insert(Tag.AsString()).second)
                        {
                            OutDiagnostics.push_back(MakeDiagnostic(
                                "core:diagnostic.definition.entry.duplicate_tag",
                                "Definition metadata tags must be unique",
                                Document, TagPointer, PackageId, PackageLoadIndex, RelativeSource));
                        }
                        else ParsedTags.push_back(Tag.AsString());
                    }
                }
            }
            if (Deprecated != nullptr && !Deprecated->IsBoolean())
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.definition.entry.invalid_deprecated",
                    "Definition entry deprecated metadata must be boolean",
                    Document, Pointer + "/deprecated", PackageId, PackageLoadIndex, RelativeSource));
            }
            if (EntryExtensions != nullptr && !EntryExtensions->IsObject())
            {
                OutDiagnostics.push_back(MakeDiagnostic(
                    "core:diagnostic.definition.entry.invalid_extensions",
                    "Definition entry extensions must be an object",
                    Document, Pointer + "/extensions", PackageId, PackageLoadIndex, RelativeSource));
            }

            if (Id != nullptr && Id->IsString() && FStableId::IsValid(Id->AsString()))
            {
                for (std::size_t DiagnosticIndex = EntryDiagnosticCount;
                    DiagnosticIndex < OutDiagnostics.size(); ++DiagnosticIndex)
                {
                    OutDiagnostics[DiagnosticIndex].DefinitionId = Id->AsString();
                }
            }

            if (OutDiagnostics.size() == EntryDiagnosticCount)
            {
                const FParsedLocation* Location = Document.FindLocation(Pointer + "/id");
                ParsedDefinitions.push_back(FDefinitionEntry(
                    Id->AsString(),
                    *Data,
                    std::move(ParsedTags),
                    Deprecated == nullptr ? false : Deprecated->AsBoolean(),
                    EntryExtensions == nullptr ? FValue::MakeObject() : *EntryExtensions,
                    Index,
                    Location == nullptr ? FSourceSpan{} : Location->ValueSpan));
            }
        }
    }

    if (OutDiagnostics.size() != InitialDiagnosticCount) return std::nullopt;
    return FDefinitionFile(
        SchemaVersion->AsInteger(),
        DefinitionType->AsString(),
        std::move(ParsedDefinitions),
        Extensions == nullptr ? FValue::MakeObject() : *Extensions,
        std::move(PackageId),
        PackageLoadIndex,
        std::move(RelativeSource));
}

bool ValidatePackageDefinitionIds(
    const std::vector<FDefinitionFile>& DefinitionFiles,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    struct FFirstDefinition final
    {
        const FDefinitionFile* File = nullptr;
        const FDefinitionEntry* Entry = nullptr;
    };
    std::map<std::pair<std::string, std::string>, FFirstDefinition> Seen;
    const std::size_t InitialDiagnosticCount = OutDiagnostics.size();
    for (const FDefinitionFile& File : DefinitionFiles)
    {
        for (const FDefinitionEntry& Entry : File.GetDefinitions())
        {
            const auto Key = std::pair(File.GetPackageId(), Entry.GetId());
            const auto [Found, bInserted] = Seen.emplace(Key, FFirstDefinition{ &File, &Entry });
            if (bInserted) continue;

            FDiagnostic Diagnostic;
            Diagnostic.Code = "core:diagnostic.definition.entry.duplicate_id";
            Diagnostic.Message = "Definition ID is duplicated within one package";
            Diagnostic.PackageId = File.GetPackageId();
            Diagnostic.PackageLoadIndex = File.GetPackageLoadIndex();
            Diagnostic.RelativeSource = File.GetRelativeSource();
            Diagnostic.DefinitionId = Entry.GetId();
            Diagnostic.JsonPointer = "/definitions/" + std::to_string(Entry.GetSourceIndex()) + "/id";
            Diagnostic.Span = Entry.GetSourceSpan();
            Diagnostic.RelatedSpan = Found->second.Entry->GetSourceSpan();
            Diagnostic.RelatedMessage = "Previously declared at "
                + Found->second.File->GetRelativeSource() + "/definitions/"
                + std::to_string(Found->second.Entry->GetSourceIndex());
            OutDiagnostics.push_back(std::move(Diagnostic));
        }
    }
    return OutDiagnostics.size() == InitialDiagnosticCount;
}
}
