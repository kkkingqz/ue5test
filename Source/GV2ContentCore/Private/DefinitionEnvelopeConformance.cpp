#include "GV2ContentCore/Testing/DefinitionEnvelopeConformance.h"

#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunDefinitionEnvelopeConformance()
{
    // 1. Valid definition file envelope
    std::vector<FDiagnostic> Diagnostics;
    auto Document = ParseJson5Document(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.sword', data: {}, tags: ['weapon'], deprecated: false },"
        "{ id: 'core:item.potion', data: null }"
        "] }",
        FParseLimits{}, Diagnostics);
    if (!Document.has_value() || !Diagnostics.empty())
    {
        return "definition_envelope.parse_valid_document";
    }

    auto File = ParseDefinitionFileEnvelope(
        *Document, "core", 0, "definitions/items.json5", Diagnostics);
    if (!File.has_value() || !Diagnostics.empty()
        || File->GetDefinitionType() != "item"
        || File->GetDefinitions().size() != 2
        || !File->GetDefinitions()[1].GetData().IsNull()
        || File->GetDefinitions()[1].IsDeprecated())
    {
        return "definition_envelope.valid_envelope_content";
    }

    // 2. Definition entry ID kind mismatch rejected
    Diagnostics.clear();
    auto InvalidDocument = ParseJson5Document(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:screen.bad_kind', data: {} }] }",
        FParseLimits{}, Diagnostics);
    if (!InvalidDocument.has_value() || !Diagnostics.empty())
    {
        return "definition_envelope.parse_invalid_document";
    }
    if (ParseDefinitionFileEnvelope(
            *InvalidDocument, "core", 0, "definitions/invalid.json5", Diagnostics).has_value()
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.definition.entry.id_kind_mismatch")
    {
        return "definition_envelope.id_kind_mismatch_rejected";
    }

    // 3. Duplicate definition ID across definition files in package rejected
    Diagnostics.clear();
    auto DuplicateDocument = ParseJson5Document(
        "{ schema_version: 1, type: 'item', definitions: ["
        "{ id: 'core:item.same', data: {} }, { id: 'core:item.same', data: {} }] }",
        FParseLimits{}, Diagnostics);
    if (!DuplicateDocument.has_value() || !Diagnostics.empty())
    {
        return "definition_envelope.parse_duplicate_document";
    }
    auto DuplicateFile = ParseDefinitionFileEnvelope(
        *DuplicateDocument, "core", 0, "definitions/duplicate.json5", Diagnostics);
    if (!DuplicateFile.has_value() || !Diagnostics.empty())
    {
        return "definition_envelope.parse_duplicate_envelope";
    }
    std::vector<FDefinitionFile> Files;
    Files.push_back(std::move(*DuplicateFile));
    if (ValidatePackageDefinitionIds(Files, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.back().Code != "core:diagnostic.definition.entry.duplicate_id")
    {
        return "definition_envelope.duplicate_definition_id_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
