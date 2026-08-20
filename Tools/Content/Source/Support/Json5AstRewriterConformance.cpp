#include "Support/Json5AstRewriterConformance.h"
#include "Support/Json5AstRewriter.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCli::Testing
{

namespace
{

bool TestPositionToOffsetAscii()
{
    std::string Text = "abc\ndef\nghi";
    std::size_t Offset = 0;

    // (1, 1) -> 'a' (offset 0)
    if (!SourcePositionToByteOffset(Text, 1, 1, Offset) || Offset != 0) return false;
    // (1, 4) -> '\n' (offset 3)
    if (!SourcePositionToByteOffset(Text, 1, 4, Offset) || Offset != 3) return false;
    // (2, 1) -> 'd' (offset 4)
    if (!SourcePositionToByteOffset(Text, 2, 1, Offset) || Offset != 4) return false;
    // (3, 3) -> 'i' (offset 10)
    if (!SourcePositionToByteOffset(Text, 3, 3, Offset) || Offset != 10) return false;
    // (3, 4) -> EOF (offset 11)
    if (!SourcePositionToByteOffset(Text, 3, 4, Offset) || Offset != 11) return false;
    // Beyond EOF
    if (SourcePositionToByteOffset(Text, 4, 1, Offset)) return false;

    return true;
}

bool TestPositionToOffsetMultibyteUtf8()
{
    // "Привет, 🎮!" ->
    // 'П' is 2 bytes (0xD0 0x9F), col 1
    // 'р' is 2 bytes (0xD1 0x80), col 2
    // 'и' is 2 bytes (0xD0 0xB8), col 3
    // 'в' is 2 bytes (0xD0 0xB2), col 4
    // 'е' is 2 bytes (0xD0 0xB5), col 5
    // 'т' is 2 bytes (0xD1 0x82), col 6
    // ',' is 1 byte, col 7
    // ' ' is 1 byte, col 8
    // '🎮' is 4 bytes (0xF0 0x9F 0x8E 0xAE), col 9
    // '!' is 1 byte, col 10
    // newline is 1 byte, col 11
    std::string Text = "Привет, 🎮!\nВторая строка";
    std::size_t Offset = 0;

    // (1, 1) -> 'П' (offset 0)
    if (!SourcePositionToByteOffset(Text, 1, 1, Offset) || Offset != 0) return false;
    // (1, 2) -> 'р' (offset 2)
    if (!SourcePositionToByteOffset(Text, 1, 2, Offset) || Offset != 2) return false;
    // (1, 7) -> ',' (offset 12)
    if (!SourcePositionToByteOffset(Text, 1, 7, Offset) || Offset != 12) return false;
    // (1, 9) -> '🎮' (offset 14)
    if (!SourcePositionToByteOffset(Text, 1, 9, Offset) || Offset != 14) return false;
    // (1, 10) -> '!' (offset 18)
    if (!SourcePositionToByteOffset(Text, 1, 10, Offset) || Offset != 18) return false;
    // (1, 11) -> '\n' (offset 19)
    if (!SourcePositionToByteOffset(Text, 1, 11, Offset) || Offset != 19) return false;
    // (2, 1) -> 'В' (offset 20)
    if (!SourcePositionToByteOffset(Text, 2, 1, Offset) || Offset != 20) return false;

    return true;
}

bool TestPositionToOffsetLineEndings()
{
    // CRLF
    std::string TextCrlf = "line1\r\nline2\r\nline3";
    std::size_t Offset = 0;
    // (1, 1) -> 'l' (offset 0)
    if (!SourcePositionToByteOffset(TextCrlf, 1, 1, Offset) || Offset != 0) return false;
    // (2, 1) -> 'l' of line2 (offset 7)
    if (!SourcePositionToByteOffset(TextCrlf, 2, 1, Offset) || Offset != 7) return false;
    // (3, 1) -> 'l' of line3 (offset 14)
    if (!SourcePositionToByteOffset(TextCrlf, 3, 1, Offset) || Offset != 14) return false;

    // CR only (Classic Mac)
    std::string TextCr = "a\rb\rc";
    if (!SourcePositionToByteOffset(TextCr, 2, 1, Offset) || Offset != 2) return false;
    if (!SourcePositionToByteOffset(TextCr, 3, 1, Offset) || Offset != 4) return false;

    return true;
}

bool TestPositionToOffsetBom()
{
    std::string TextBom = "\xEF\xBB\xBF{\n  id: \"test\",\n}";
    std::size_t Offset = 0;
    // (1, 1) -> '{' (offset 3)
    if (!SourcePositionToByteOffset(TextBom, 1, 1, Offset) || Offset != 3) return false;
    // (2, 3) -> 'i' (offset 7)
    if (!SourcePositionToByteOffset(TextBom, 2, 3, Offset) || Offset != 7) return false;

    return true;
}

bool TestSetFieldValue()
{
    std::string Doc =
        "// File header comment\n"
        "{\n"
        "  schema_version: 1,\n"
        "  type: \"item\",\n"
        "  definitions: [\n"
        "    // Sword entry comment\n"
        "    {\n"
        "      id: \"core:item.sword\",\n"
        "      data: {\n"
        "        price: 100,\n"
        "        name: \"Iron Sword\",\n"
        "      },\n"
        "    },\n"
        "  ],\n"
        "}\n"
        "// File footer comment\n";

    // 1. Set scalar integer price
    auto ResInt = SetFieldValue(Doc, "/definitions/0/data/price", GV2ContentCore::FValue(static_cast<std::int64_t>(250)));
    if (ResInt.Status != ESetFieldValueStatus::Success) return false;
    if (ResInt.UpdatedContent.find("price: 250") == std::string::npos) return false;
    if (ResInt.UpdatedContent.find("// File header comment") == std::string::npos) return false;
    if (ResInt.UpdatedContent.find("// Sword entry comment") == std::string::npos) return false;
    if (ResInt.UpdatedContent.find("// File footer comment") == std::string::npos) return false;

    // 2. Set scalar string name
    auto ResStr = SetFieldValue(ResInt.UpdatedContent, "/definitions/0/data/name", GV2ContentCore::FValue("Steel Sword"));
    if (ResStr.Status != ESetFieldValueStatus::Success) return false;
    if (ResStr.UpdatedContent.find("name: \"Steel Sword\"") == std::string::npos) return false;

    // 3. Rejection: nonexistent pointer
    auto ResMissing = SetFieldValue(Doc, "/definitions/0/data/nonexistent", GV2ContentCore::FValue(static_cast<std::int64_t>(10)));
    if (ResMissing.Status != ESetFieldValueStatus::PointerNotFound) return false;
    if (ResMissing.ErrorCode != "pointer_not_found") return false;

    // 4. Container replacement is supported by the syntax-preserving
    // rewriter. Authoritative schema validation belongs to AuthoringService.
    GV2ContentCore::FValue::FObject Obj;
    Obj.emplace_back("a", GV2ContentCore::FValue(static_cast<std::int64_t>(1)));
    auto ResContainer = SetFieldValue(Doc, "/definitions/0/data", GV2ContentCore::FValue(std::move(Obj)));
    if (ResContainer.Status != ESetFieldValueStatus::Success) return false;
    if (ResContainer.UpdatedContent.find("data: {\n  a: 1,") == std::string::npos) return false;
    if (ResContainer.UpdatedContent.find("// Sword entry comment") == std::string::npos) return false;

    // 5. A container may also replace a scalar at the rewriter layer.
    // The service rejects the candidate before writing when its schema disallows it.
    GV2ContentCore::FValue::FArray Array;
    Array.emplace_back(static_cast<std::int64_t>(1));
    Array.emplace_back(static_cast<std::int64_t>(2));
    auto ResArray = SetFieldValue(Doc, "/definitions/0/data/price", GV2ContentCore::FValue(std::move(Array)));
    if (ResArray.Status != ESetFieldValueStatus::Success) return false;
    if (ResArray.UpdatedContent.find("price: [\n  1,\n  2,") == std::string::npos) return false;

    return true;
}

bool TestRemoveDefinitionEntry()
{
    std::string Doc =
        "// File header\n"
        "{\n"
        "  schema_version: 1,\n"
        "  type: \"item\",\n"
        "  definitions: [\n"
        "    // First entry comment\n"
        "    {\n"
        "      id: \"core:item.sword\",\n"
        "      data: { price: 10 },\n"
        "    },\n"
        "    // Second entry comment\n"
        "    {\n"
        "      id: \"core:item.shield\",\n"
        "      data: { price: 20 },\n"
        "    },\n"
        "  ],\n"
        "}\n";

    // 1. Remove first entry
    auto Res1 = RemoveDefinitionEntry(Doc, "core:item.sword");
    if (Res1.Status != ERemoveDefinitionStatus::Success) return false;
    if (Res1.UpdatedContent.find("core:item.sword") != std::string::npos) return false;
    if (Res1.UpdatedContent.find("core:item.shield") == std::string::npos) return false;
    if (Res1.UpdatedContent.find("// Second entry comment") == std::string::npos) return false;

    // 2. Remove second entry from modified doc (now only entry remaining)
    auto Res2 = RemoveDefinitionEntry(Res1.UpdatedContent, "core:item.shield");
    if (Res2.Status != ERemoveDefinitionStatus::Success) return false;
    if (Res2.UpdatedContent.find("core:item.shield") != std::string::npos) return false;
    // Verify document parses cleanly and definitions is empty
    GV2ContentCore::FParseLimits Limits;
    std::vector<GV2ContentCore::FDiagnostic> Diags;
    auto Parsed = GV2ContentCore::ParseJson5Document(Res2.UpdatedContent, Limits, Diags);
    if (!Parsed.has_value() || !Diags.empty()) return false;
    const auto* Defs = Parsed->GetRootValue().FindField("definitions");
    if (Defs == nullptr || !Defs->IsArray() || !Defs->AsArray().empty()) return false;

    // 3. Rejection: nonexistent definition
    auto ResMissing = RemoveDefinitionEntry(Doc, "core:item.nonexistent");
    if (ResMissing.Status != ERemoveDefinitionStatus::DefinitionNotFound) return false;
    if (ResMissing.ErrorCode != "definition_not_found") return false;

    return true;
}

} // namespace

std::string RunJson5AstRewriterConformance()
{
    if (!TestPositionToOffsetAscii()) return "json5_ast_rewriter.position_to_offset_ascii";
    if (!TestPositionToOffsetMultibyteUtf8()) return "json5_ast_rewriter.position_to_offset_multibyte_utf8";
    if (!TestPositionToOffsetLineEndings()) return "json5_ast_rewriter.position_to_offset_line_endings";
    if (!TestPositionToOffsetBom()) return "json5_ast_rewriter.position_to_offset_bom";
    if (!TestSetFieldValue()) return "json5_ast_rewriter.set_field_value";
    if (!TestRemoveDefinitionEntry()) return "json5_ast_rewriter.remove_definition_entry";

    return "";
}

} // namespace GV2ContentCli::Testing
