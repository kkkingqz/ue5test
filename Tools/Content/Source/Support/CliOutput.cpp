#include "Support/CliOutput.h"

#include <cstdio>
#include <iomanip>
#include <sstream>

namespace GV2ContentCli
{

void PrintUsage(std::ostream& Out)
{
    Out << "gv2-content <command> [options]\n"
        << "\n"
        << "Commands:\n"
        << "  validate <package-root> [--watch] [--poll-interval=MS] [--max-iterations=N] [--format=text|json]\n"
        << "  inspect  <package-root> <definition-id> [--provenance] [--format=text|json]\n"
        << "  describe <package-root> <definition-type> [--format=text|json]\n"
        << "  new      <package-root> <definition-type> <definition-id> [--format=text|json]\n"
        << "  refs     <package-root> <definition-id> [--format=text|json]\n"
        << "  rename   <package-root> <old-id> <new-id> [--format=text|json]\n"
        << "  index    <package-root> [--format=text|json]\n"
        << "  hash     <package-root> [--format=text|json]\n"
        << "  coverage <package-root> [--locale=LOCALE] [--format=text|json]\n"
        << "\n"
        << "Exit codes: 0 success, 1 invalid content, 2 tool/configuration failure.\n";
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

int EmitToolFailure(
    const std::string& Message,
    EOutputFormat Format,
    std::ostream& Out,
    std::ostream& Err)
{
    if (Format == EOutputFormat::Json)
    {
        Out << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":";
        WriteJsonEscapedString(Out, Message);
        Out << "}\n";
    }
    else
    {
        Err << "error: " << Message << "\n";
    }
    return static_cast<int>(EExitCode::ToolFailure);
}

int EmitDiagnosticsFailure(
    const std::vector<GV2ContentCore::FDiagnostic>& Diagnostics,
    EOutputFormat Format,
    std::ostream& Out)
{
    if (Format == EOutputFormat::Json)
    {
        Out << "{\"status\":\"invalid\",\"diagnostics\":[";
        for (std::size_t Index = 0; Index < Diagnostics.size(); ++Index)
        {
            if (Index != 0)
            {
                Out << ",";
            }
            WriteDiagnosticJson(Out, Diagnostics[Index]);
        }
        Out << "]}\n";
    }
    else
    {
        for (const auto& Diagnostic : Diagnostics)
        {
            WriteDiagnosticText(Out, Diagnostic);
        }
    }
    return static_cast<int>(EExitCode::InvalidContent);
}

} // namespace GV2ContentCli
