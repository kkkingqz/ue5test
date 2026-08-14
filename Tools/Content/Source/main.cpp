#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/PackageDiscovery.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentCore/Value.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// PCC-31..PCC-34: portable `gv2-content` CLI (validate/inspect/hash) over the
// shared GV2ContentCore library. Never links UE or gv2_runtime_core, so it
// builds and runs on a machine without Unreal Engine installed.
//
// PCC-34 exit codes (stable across --format=text|json):
//   0 - success
//   1 - invalid content (BuildRepository diagnostics, unknown/wrong-kind id)
//   2 - tool/configuration failure (bad CLI usage, missing/invalid root)
namespace
{
enum class EExitCode : int
{
    Success = 0,
    InvalidContent = 1,
    ToolFailure = 2,
};

enum class EOutputFormat : std::uint8_t
{
    Text,
    Json,
};

void PrintUsage(std::ostream& Out)
{
    Out << "gv2-content <command> [options]\n"
        << "\n"
        << "Commands:\n"
        << "  validate <package-root> [--format=text|json]\n"
        << "  inspect  <package-root> <definition-id> [--provenance] [--format=text|json]\n"
        << "  hash     <package-root> [--format=text|json]\n"
        << "\n"
        << "Exit codes: 0 success, 1 invalid content, 2 tool/configuration failure.\n";
}

// Filesystem-backed IContentSourceProvider for a single package root directory.
class FFilesystemContentSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    FFilesystemContentSourceProvider(std::filesystem::path InPackageRoot, std::string InPackageId)
        : PackageRoot(std::move(InPackageRoot))
        , PackageId(std::move(InPackageId))
    {
    }

    std::optional<std::string> ReadSource(
        std::string_view RequestedPackageId,
        std::string_view RelativeSource) const override
    {
        if (RequestedPackageId != PackageId)
        {
            return std::nullopt;
        }
        std::ifstream Stream(PackageRoot / std::string(RelativeSource), std::ios::binary);
        if (!Stream)
        {
            return std::nullopt;
        }
        return std::string{std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>()};
    }

private:
    std::filesystem::path PackageRoot;
    std::string PackageId;
};

std::optional<std::string> ReadFileToString(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream)
    {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> ListSortedJson5Files(const std::filesystem::path& Dir)
{
    std::vector<std::filesystem::path> Entries;
    std::error_code Ec;
    if (!std::filesystem::is_directory(Dir, Ec) || Ec)
    {
        return Entries;
    }
    for (const auto& Entry : std::filesystem::directory_iterator(Dir, Ec))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".json5")
        {
            Entries.push_back(Entry.path());
        }
    }
    std::sort(Entries.begin(), Entries.end());
    return Entries;
}

// Discovers a single-package FPackageDescriptor from a directory laid out as
// <root>/definitions/*.json5 and <root>/schemas/*.json5 (each schema resource
// self-declares "id"/"definition_type"/"schema_version"). This CLI-local
// discovery does not resolve mods, dependency order or extension schemas -
// those are out of PortableContentCore's first-release scope; a `gv2-content
// validate` root is always exactly one package.
struct FRootBuildOutcome
{
    bool bToolFailure = false;
    std::string ToolFailureMessage;
    std::optional<GV2ContentCore::FBuildResult> Result;
    // Kept alive for the lifetime of Result, which holds a raw pointer into it via FBuildOptions.
    std::unique_ptr<FFilesystemContentSourceProvider> Provider;
};

FRootBuildOutcome BuildFromPackageRoot(const std::filesystem::path& RawRoot)
{
    FRootBuildOutcome Outcome;

    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        Outcome.bToolFailure = true;
        Outcome.ToolFailureMessage = "package root not found or not a directory";
        return Outcome;
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> Descriptor =
        GV2ContentCore::DiscoverPackageFromDirectory(Root, Diagnostics);
    if (!Descriptor)
    {
        Outcome.Result.emplace(GV2ContentCore::FBuildResult::Failure(std::move(Diagnostics)));
        return Outcome;
    }

    Outcome.Provider = std::make_unique<FFilesystemContentSourceProvider>(Root, Descriptor->GetPackageId());
    GV2ContentCore::FBuildOptions Options;
    Options.SourceProvider = Outcome.Provider.get();

    Outcome.Result.emplace(GV2ContentCore::BuildRepository({*Descriptor}, Options));
    return Outcome;
}

std::string FormatDouble(double Value)
{
    std::ostringstream Stream;
    Stream.precision(17);
    Stream << Value;
    std::string Text = Stream.str();
    if (Text.find('.') == std::string::npos && Text.find('e') == std::string::npos
        && Text.find("inf") == std::string::npos && Text.find("nan") == std::string::npos)
    {
        Text += ".0";
    }
    return Text;
}

void WriteJsonEscapedString(std::ostream& Out, const std::string& Text)
{
    Out << '"';
    for (const unsigned char Ch : Text)
    {
        switch (Ch)
        {
        case '"':
            Out << "\\\"";
            break;
        case '\\':
            Out << "\\\\";
            break;
        case '\b':
            Out << "\\b";
            break;
        case '\f':
            Out << "\\f";
            break;
        case '\n':
            Out << "\\n";
            break;
        case '\r':
            Out << "\\r";
            break;
        case '\t':
            Out << "\\t";
            break;
        default:
            if (Ch < 0x20)
            {
                char Buffer[8];
                std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", Ch);
                Out << Buffer;
            }
            else
            {
                Out << static_cast<char>(Ch);
            }
        }
    }
    Out << '"';
}

void WriteJsonValue(std::ostream& Out, const GV2ContentCore::FValue& Value)
{
    using GV2ContentCore::EValueKind;
    switch (Value.GetKind())
    {
    case EValueKind::Null:
        Out << "null";
        break;
    case EValueKind::Boolean:
        Out << (Value.AsBoolean() ? "true" : "false");
        break;
    case EValueKind::Integer:
        Out << Value.AsInteger();
        break;
    case EValueKind::Number:
        Out << FormatDouble(Value.AsNumber());
        break;
    case EValueKind::String:
        WriteJsonEscapedString(Out, Value.AsString());
        break;
    case EValueKind::Array:
    {
        Out << '[';
        bool bFirst = true;
        for (const auto& Element : Value.AsArray())
        {
            if (!bFirst)
            {
                Out << ',';
            }
            bFirst = false;
            WriteJsonValue(Out, Element);
        }
        Out << ']';
        break;
    }
    case EValueKind::Object:
    {
        Out << '{';
        bool bFirst = true;
        for (const auto& Field : Value.AsObject())
        {
            if (!bFirst)
            {
                Out << ',';
            }
            bFirst = false;
            WriteJsonEscapedString(Out, Field.first);
            Out << ':';
            WriteJsonValue(Out, Field.second);
        }
        Out << '}';
        break;
    }
    }
}

const char* SeverityText(GV2ContentCore::EDiagnosticSeverity Severity)
{
    switch (Severity)
    {
    case GV2ContentCore::EDiagnosticSeverity::Error:
        return "error";
    case GV2ContentCore::EDiagnosticSeverity::Warning:
        return "warning";
    case GV2ContentCore::EDiagnosticSeverity::Info:
        return "info";
    }
    return "error";
}

void WriteDiagnosticText(std::ostream& Out, const GV2ContentCore::FDiagnostic& Diagnostic)
{
    Out << SeverityText(Diagnostic.Severity) << " " << Diagnostic.Code;
    if (Diagnostic.RelativeSource)
    {
        Out << " " << *Diagnostic.RelativeSource;
        if (Diagnostic.Span)
        {
            Out << ":" << Diagnostic.Span->StartLine << ":" << Diagnostic.Span->StartColumn;
        }
    }
    Out << " " << Diagnostic.Message;
    if (Diagnostic.DefinitionId)
    {
        Out << " (definition=" << *Diagnostic.DefinitionId << ")";
    }
    if (Diagnostic.JsonPointer)
    {
        Out << " (pointer=" << *Diagnostic.JsonPointer << ")";
    }
    Out << "\n";
}

void WriteSpanJson(std::ostream& Out, const char* Key, const GV2ContentCore::FSourceSpan& Span)
{
    Out << "\"" << Key << "\":{\"start_line\":" << Span.StartLine
        << ",\"start_column\":" << Span.StartColumn
        << ",\"end_line\":" << Span.EndLine
        << ",\"end_column\":" << Span.EndColumn << "}";
}

void WriteDiagnosticJson(std::ostream& Out, const GV2ContentCore::FDiagnostic& Diagnostic)
{
    Out << "{\"code\":";
    WriteJsonEscapedString(Out, Diagnostic.Code);
    Out << ",\"severity\":\"" << SeverityText(Diagnostic.Severity) << "\"";
    Out << ",\"message\":";
    WriteJsonEscapedString(Out, Diagnostic.Message);
    if (Diagnostic.PackageId)
    {
        Out << ",\"package_id\":";
        WriteJsonEscapedString(Out, *Diagnostic.PackageId);
    }
    if (Diagnostic.PackageLoadIndex)
    {
        Out << ",\"package_load_index\":" << *Diagnostic.PackageLoadIndex;
    }
    if (Diagnostic.RelativeSource)
    {
        Out << ",\"relative_source\":";
        WriteJsonEscapedString(Out, *Diagnostic.RelativeSource);
    }
    if (Diagnostic.Span)
    {
        Out << ",";
        WriteSpanJson(Out, "span", *Diagnostic.Span);
    }
    if (Diagnostic.RelatedSpan)
    {
        Out << ",";
        WriteSpanJson(Out, "related_span", *Diagnostic.RelatedSpan);
    }
    if (Diagnostic.RelatedMessage)
    {
        Out << ",\"related_message\":";
        WriteJsonEscapedString(Out, *Diagnostic.RelatedMessage);
    }
    if (Diagnostic.DefinitionId)
    {
        Out << ",\"definition_id\":";
        WriteJsonEscapedString(Out, *Diagnostic.DefinitionId);
    }
    if (Diagnostic.SchemaId)
    {
        Out << ",\"schema_id\":";
        WriteJsonEscapedString(Out, *Diagnostic.SchemaId);
    }
    if (Diagnostic.SchemaVersion)
    {
        Out << ",\"schema_version\":" << *Diagnostic.SchemaVersion;
    }
    if (Diagnostic.JsonPointer)
    {
        Out << ",\"json_pointer\":";
        WriteJsonEscapedString(Out, *Diagnostic.JsonPointer);
    }
    Out << "}";
}

void WriteProviderProvenanceJson(std::ostream& Out, const GV2ContentCore::FProviderProvenance& Provenance)
{
    Out << "{\"package_id\":";
    WriteJsonEscapedString(Out, Provenance.PackageId);
    Out << ",\"package_load_index\":" << Provenance.PackageLoadIndex;
    Out << ",\"relative_source\":";
    WriteJsonEscapedString(Out, Provenance.RelativeSource);
    Out << ",";
    WriteSpanJson(Out, "span", Provenance.SourceSpan);
    Out << ",\"schema_id\":";
    WriteJsonEscapedString(Out, Provenance.SchemaId);
    Out << ",\"schema_version\":" << Provenance.SchemaVersion;
    Out << "}";
}

void WriteDefinitionProvenanceJson(std::ostream& Out, const GV2ContentCore::FDefinitionProvenance& Provenance)
{
    Out << "{\"original_id\":";
    WriteJsonEscapedString(Out, Provenance.OriginalId);
    Out << ",\"canonical_id\":";
    WriteJsonEscapedString(Out, Provenance.CanonicalId);
    Out << ",\"redirect_chain\":[";
    for (std::size_t Index = 0; Index < Provenance.RedirectChain.size(); ++Index)
    {
        if (Index != 0)
        {
            Out << ",";
        }
        WriteJsonEscapedString(Out, Provenance.RedirectChain[Index]);
    }
    Out << "],\"winner\":";
    WriteProviderProvenanceJson(Out, Provenance.Winner);
    Out << ",\"shadowed\":[";
    for (std::size_t Index = 0; Index < Provenance.ShadowedProviders.size(); ++Index)
    {
        if (Index != 0)
        {
            Out << ",";
        }
        WriteProviderProvenanceJson(Out, Provenance.ShadowedProviders[Index]);
    }
    Out << "]}";
}

void WriteProvenanceText(std::ostream& Out, const GV2ContentCore::FDefinitionProvenance& Provenance)
{
    Out << "original_id: " << Provenance.OriginalId << "\n";
    Out << "canonical_id: " << Provenance.CanonicalId << "\n";
    if (!Provenance.RedirectChain.empty())
    {
        Out << "redirect_chain:";
        for (const auto& Hop : Provenance.RedirectChain)
        {
            Out << " " << Hop;
        }
        Out << "\n";
    }
    const auto WriteProvider = [&Out](const char* Label, const GV2ContentCore::FProviderProvenance& Provider)
    {
        Out << Label << ": package=" << Provider.PackageId
            << " load_index=" << Provider.PackageLoadIndex
            << " source=" << Provider.RelativeSource
            << ":" << Provider.SourceSpan.StartLine << ":" << Provider.SourceSpan.StartColumn
            << " schema=" << Provider.SchemaId << "@" << Provider.SchemaVersion << "\n";
    };
    WriteProvider("winner", Provenance.Winner);
    for (const auto& Shadowed : Provenance.ShadowedProviders)
    {
        WriteProvider("shadowed", Shadowed);
    }
}

int EmitDiagnosticsFailure(const std::vector<GV2ContentCore::FDiagnostic>& Diagnostics, EOutputFormat Format)
{
    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"invalid\",\"diagnostics\":[";
        for (std::size_t Index = 0; Index < Diagnostics.size(); ++Index)
        {
            if (Index != 0)
            {
                std::cout << ",";
            }
            WriteDiagnosticJson(std::cout, Diagnostics[Index]);
        }
        std::cout << "]}\n";
    }
    else
    {
        for (const auto& Diagnostic : Diagnostics)
        {
            WriteDiagnosticText(std::cout, Diagnostic);
        }
    }
    return static_cast<int>(EExitCode::InvalidContent);
}

// PCC-31: `gv2-content validate <package-root> [--format=text|json]`.
int RunValidate(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 1)
    {
        std::cerr << "usage: gv2-content validate <package-root> [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    FRootBuildOutcome Outcome = BuildFromPackageRoot(Positional[0]);
    if (Outcome.bToolFailure)
    {
        std::cerr << "gv2-content: " << Outcome.ToolFailureMessage << "\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const GV2ContentCore::FBuildResult& Result = *Outcome.Result;
    if (Result.IsFailure())
    {
        return EmitDiagnosticsFailure(Result.GetDiagnostics(), Format);
    }

    const GV2ContentCore::FRepositoryReadHandle Handle = Result.GetCandidate().GetReadHandle();
    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"content_hash\":";
        WriteJsonEscapedString(std::cout, Handle.GetContentHash());
        std::cout << "}\n";
    }
    else
    {
        std::cout << "ok content_hash=" << Handle.GetContentHash() << "\n";
    }
    return static_cast<int>(EExitCode::Success);
}

// PCC-32: `gv2-content inspect <package-root> <definition-id> [--provenance] [--format=text|json]`.
int RunInspect(const std::vector<std::string>& Positional, EOutputFormat Format, bool bProvenance)
{
    if (Positional.size() != 2)
    {
        std::cerr << "usage: gv2-content inspect <package-root> <definition-id> [--provenance] [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::string& RawId = Positional[1];
    const std::optional<GV2ContentCore::FDefinitionId> DefinitionId = GV2ContentCore::FDefinitionId::Parse(RawId);
    if (!DefinitionId)
    {
        std::cerr << "gv2-content: '" << RawId << "' is not a valid definition id\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    FRootBuildOutcome Outcome = BuildFromPackageRoot(Positional[0]);
    if (Outcome.bToolFailure)
    {
        std::cerr << "gv2-content: " << Outcome.ToolFailureMessage << "\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const GV2ContentCore::FBuildResult& Result = *Outcome.Result;
    if (Result.IsFailure())
    {
        return EmitDiagnosticsFailure(Result.GetDiagnostics(), Format);
    }

    const GV2ContentCore::FRepositoryReadHandle Handle = Result.GetCandidate().GetReadHandle();
    const GV2ContentCore::FRepositoryQueryResult Query = Handle.Require(*DefinitionId);
    if (!Query)
    {
        const GV2ContentCore::FRepositoryQueryError& Error = *Query.Error;
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"not_found\",\"code\":";
            WriteJsonEscapedString(std::cout, Error.Code);
            std::cout << ",\"requested_id\":";
            WriteJsonEscapedString(std::cout, Error.RequestedId);
            if (Error.CanonicalId)
            {
                std::cout << ",\"canonical_id\":";
                WriteJsonEscapedString(std::cout, *Error.CanonicalId);
            }
            std::cout << "}\n";
        }
        else
        {
            std::cout << "not_found " << Error.Code << " " << Error.RequestedId;
            if (Error.CanonicalId)
            {
                std::cout << " (canonical=" << *Error.CanonicalId << ")";
            }
            std::cout << "\n";
        }
        return static_cast<int>(EExitCode::InvalidContent);
    }

    const GV2ContentCore::FDefinitionProvenance* Provenance =
        bProvenance ? Handle.GetProvenance(*DefinitionId) : nullptr;

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"id\":";
        WriteJsonEscapedString(std::cout, DefinitionId->ToString());
        std::cout << ",\"definition\":";
        WriteJsonValue(std::cout, *Query.Definition);
        if (bProvenance)
        {
            std::cout << ",\"provenance\":";
            if (Provenance)
            {
                WriteDefinitionProvenanceJson(std::cout, *Provenance);
            }
            else
            {
                std::cout << "null";
            }
        }
        std::cout << "}\n";
    }
    else
    {
        std::cout << "id: " << DefinitionId->ToString() << "\n";
        std::cout << "definition: ";
        WriteJsonValue(std::cout, *Query.Definition);
        std::cout << "\n";
        if (bProvenance)
        {
            if (Provenance)
            {
                WriteProvenanceText(std::cout, *Provenance);
            }
            else
            {
                std::cout << "provenance: unavailable\n";
            }
        }
    }
    return static_cast<int>(EExitCode::Success);
}

// PCC-33: `gv2-content hash <package-root> [--format=text|json]`.
int RunHash(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 1)
    {
        std::cerr << "usage: gv2-content hash <package-root> [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    FRootBuildOutcome Outcome = BuildFromPackageRoot(Positional[0]);
    if (Outcome.bToolFailure)
    {
        std::cerr << "gv2-content: " << Outcome.ToolFailureMessage << "\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const GV2ContentCore::FBuildResult& Result = *Outcome.Result;
    if (Result.IsFailure())
    {
        return EmitDiagnosticsFailure(Result.GetDiagnostics(), Format);
    }

    const std::string Hash = Result.GetCandidate().GetReadHandle().GetContentHash();
    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"content_hash\":";
        WriteJsonEscapedString(std::cout, Hash);
        std::cout << "}\n";
    }
    else
    {
        std::cout << Hash << "\n";
    }
    return static_cast<int>(EExitCode::Success);
}
} // namespace

int main(int argc, char** argv)
{
    const std::vector<std::string> Args(argv + 1, argv + argc);
    if (Args.empty())
    {
        PrintUsage(std::cerr);
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::string& Command = Args[0];
    if (Command == "--help" || Command == "-h" || Command == "help")
    {
        PrintUsage(std::cout);
        return static_cast<int>(EExitCode::Success);
    }

    EOutputFormat Format = EOutputFormat::Text;
    bool bProvenance = false;
    std::vector<std::string> Positional;
    for (std::size_t Index = 1; Index < Args.size(); ++Index)
    {
        const std::string& Arg = Args[Index];
        if (Arg.rfind("--format=", 0) == 0)
        {
            const std::string Value = Arg.substr(9);
            if (Value == "text")
            {
                Format = EOutputFormat::Text;
            }
            else if (Value == "json")
            {
                Format = EOutputFormat::Json;
            }
            else
            {
                std::cerr << "gv2-content: unknown --format value '" << Value << "'\n";
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
        else if (Arg == "--provenance")
        {
            bProvenance = true;
        }
        else if (!Arg.empty() && Arg[0] == '-')
        {
            std::cerr << "gv2-content: unknown option '" << Arg << "'\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        else
        {
            Positional.push_back(Arg);
        }
    }

    if (bProvenance && Command != "inspect")
    {
        std::cerr << "gv2-content: --provenance is only supported for 'inspect'\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (Command == "validate")
    {
        return RunValidate(Positional, Format);
    }
    if (Command == "inspect")
    {
        return RunInspect(Positional, Format, bProvenance);
    }
    if (Command == "hash")
    {
        return RunHash(Positional, Format);
    }

    std::cerr << "gv2-content: unknown command '" << Command << "'\n";
    PrintUsage(std::cerr);
    return static_cast<int>(EExitCode::ToolFailure);
}
