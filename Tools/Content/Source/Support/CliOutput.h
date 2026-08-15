#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCli
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

void PrintUsage(std::ostream& Out);

std::string FormatDouble(double Value);
void WriteJsonEscapedString(std::ostream& Out, const std::string& Text);
void WriteJsonValue(std::ostream& Out, const GV2ContentCore::FValue& Value);

const char* SeverityText(GV2ContentCore::EDiagnosticSeverity Severity);
void WriteDiagnosticText(std::ostream& Out, const GV2ContentCore::FDiagnostic& Diagnostic);
void WriteSpanJson(std::ostream& Out, const char* Key, const GV2ContentCore::FSourceSpan& Span);
void WriteDiagnosticJson(std::ostream& Out, const GV2ContentCore::FDiagnostic& Diagnostic);

void WriteProviderProvenanceJson(std::ostream& Out, const GV2ContentCore::FProviderProvenance& Provenance);
void WriteDefinitionProvenanceJson(std::ostream& Out, const GV2ContentCore::FDefinitionProvenance& Provenance);
void WriteProvenanceText(std::ostream& Out, const GV2ContentCore::FDefinitionProvenance& Provenance);

int EmitToolFailure(
    const std::string& Message,
    EOutputFormat Format,
    std::ostream& Out = std::cout,
    std::ostream& Err = std::cerr);

int EmitDiagnosticsFailure(
    const std::vector<GV2ContentCore::FDiagnostic>& Diagnostics,
    EOutputFormat Format,
    std::ostream& Out = std::cout);

} // namespace GV2ContentCli
