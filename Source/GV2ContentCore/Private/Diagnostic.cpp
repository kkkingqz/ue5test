#include "GV2ContentCore/Diagnostic.h"

#include <tuple>

namespace GV2ContentCore
{
    bool FSourceSpan::operator==(const FSourceSpan& Other) const
    {
        return StartLine == Other.StartLine &&
               StartColumn == Other.StartColumn &&
               EndLine == Other.EndLine &&
               EndColumn == Other.EndColumn;
    }

    bool FSourceSpan::operator!=(const FSourceSpan& Other) const
    {
        return !(*this == Other);
    }

    bool FSourceSpan::operator<(const FSourceSpan& Other) const
    {
        return std::tie(StartLine, StartColumn, EndLine, EndColumn) <
               std::tie(Other.StartLine, Other.StartColumn, Other.EndLine, Other.EndColumn);
    }

    bool FDiagnostic::operator==(const FDiagnostic& Other) const
    {
        return Code == Other.Code &&
               Severity == Other.Severity &&
               Message == Other.Message &&
               PackageId == Other.PackageId &&
               PackageLoadIndex == Other.PackageLoadIndex &&
               RelativeSource == Other.RelativeSource &&
               Span == Other.Span &&
               RelatedSpan == Other.RelatedSpan &&
               RelatedMessage == Other.RelatedMessage &&
               DefinitionId == Other.DefinitionId &&
               SchemaId == Other.SchemaId &&
               SchemaVersion == Other.SchemaVersion &&
               JsonPointer == Other.JsonPointer;
    }

    bool FDiagnostic::operator!=(const FDiagnostic& Other) const
    {
        return !(*this == Other);
    }

    bool FDiagnostic::operator<(const FDiagnostic& Other) const
    {
        return std::tie(PackageLoadIndex, RelativeSource, Span, Code, PackageId, DefinitionId, SchemaId, SchemaVersion, JsonPointer, RelatedSpan, RelatedMessage, Severity, Message) <
               std::tie(Other.PackageLoadIndex, Other.RelativeSource, Other.Span, Other.Code, Other.PackageId, Other.DefinitionId, Other.SchemaId, Other.SchemaVersion, Other.JsonPointer, Other.RelatedSpan, Other.RelatedMessage, Other.Severity, Other.Message);
    }
}
