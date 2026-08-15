#include "GV2ContentCore/Testing/PoParserConformance.h"

#include "GV2ContentCore/PoParser.h"

namespace GV2ContentCore::Testing
{
std::string RunPoParserConformance()
{
    // 1. Basic catalog with header and context entries
    {
        const std::string ValidPo = R"po(
msgid ""
msgstr ""
"Language: ru\n"
"Content-Type: text/plain; charset=UTF-8\n"

# A comment about the iron sword
#: core/definitions/items.json5:125
msgctxt "core:text.item.iron_sword.name"
msgid "Iron sword"
msgstr "Железный меч"

msgctxt "core:text.location.market.title"
msgid "Market"
msgstr "Рыночная площадь"
)po";

        std::vector<FDiagnostic> Diagnostics;
        FPoParseOptions Options;
        Options.PackageId = "core";
        Options.RelativeSource = "localization/ru.po";

        auto Catalog = ParsePo(ValidPo, Diagnostics, Options);
        if (!Catalog.has_value() || !Diagnostics.empty())
        {
            return "po_parser.valid_catalog_failed";
        }

        if (Catalog->GetHeader("Language") != "ru")
        {
            return "po_parser.header_language_mismatch";
        }
        if (Catalog->GetHeader("Content-Type") != "text/plain; charset=UTF-8")
        {
            return "po_parser.header_content_type_mismatch";
        }
        if (Catalog->Entries.size() != 2)
        {
            return "po_parser.entries_count_mismatch";
        }

        const FPoEntry* Sword = Catalog->FindByContext("core:text.item.iron_sword.name");
        if (!Sword || Sword->MsgId != "Iron sword" || Sword->MsgStr != "Железный меч")
        {
            return "po_parser.sword_entry_mismatch";
        }
        if (Sword->Comments.empty())
        {
            return "po_parser.sword_comments_missing";
        }

        const FPoEntry* Market = Catalog->FindByContext("core:text.location.market.title");
        if (!Market || Market->MsgId != "Market" || Market->MsgStr != "Рыночная площадь")
        {
            return "po_parser.market_entry_mismatch";
        }
    }

    // 2. Multiline string concatenation and escapes
    {
        const std::string MultilinePo = R"po(
msgctxt "core:text.multiline"
msgid ""
"First line\n"
"Second line\twith tab and \"quotes\" and \\backslash\\"
msgstr ""
"Первая строка\n"
"Вторая строка\tс табом и \"кавычками\" и \\слэшем\\"
)po";

        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(MultilinePo, Diagnostics);
        if (!Catalog.has_value() || !Diagnostics.empty())
        {
            return "po_parser.multiline_failed";
        }

        const FPoEntry* Entry = Catalog->FindByContext("core:text.multiline");
        if (!Entry)
        {
            return "po_parser.multiline_entry_not_found";
        }

        const std::string ExpectedMsgId = "First line\nSecond line\twith tab and \"quotes\" and \\backslash\\";
        const std::string ExpectedMsgStr = "Первая строка\nВторая строка\tс табом и \"кавычками\" и \\слэшем\\";

        if (Entry->MsgId != ExpectedMsgId || Entry->MsgStr != ExpectedMsgStr)
        {
            return "po_parser.multiline_content_mismatch";
        }
    }

    // 3. Negative: Unterminated string literal
    {
        const std::string UnterminatedPo = "msgid \"Hello world\nmsgstr \"Привет\"";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(UnterminatedPo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.unterminated_string")
        {
            return "po_parser.unterminated_string_not_rejected";
        }
        if (!Diagnostics.front().Span.has_value() || Diagnostics.front().Span->StartLine != 1)
        {
            return "po_parser.unterminated_string_line_mismatch";
        }
    }

    // 4. Negative: Invalid escape sequence
    {
        const std::string BadEscapePo = "msgid \"Hello \\q world\"\nmsgstr \"Привет\"";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(BadEscapePo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.invalid_escape")
        {
            return "po_parser.invalid_escape_not_rejected";
        }
        if (!Diagnostics.front().Span.has_value() || Diagnostics.front().Span->StartLine != 1)
        {
            return "po_parser.invalid_escape_line_mismatch";
        }
    }

    // 5. Negative: Missing msgstr
    {
        const std::string MissingMsgstrPo = "msgctxt \"core:text.test\"\nmsgid \"Hello\"\n\nmsgid \"Next\"";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(MissingMsgstrPo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.missing_msgstr")
        {
            return "po_parser.missing_msgstr_not_rejected";
        }
    }

    // 6. Negative: Missing msgid before msgstr
    {
        const std::string MissingMsgidPo = "msgctxt \"core:text.test\"\nmsgstr \"Привет\"";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(MissingMsgidPo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.missing_msgid")
        {
            return "po_parser.missing_msgid_not_rejected";
        }
    }

    // 7. Negative: Duplicate context
    {
        const std::string DuplicateContextPo = R"po(
msgctxt "core:text.duplicate"
msgid "One"
msgstr "Один"

msgctxt "core:text.duplicate"
msgid "Two"
msgstr "Два"
)po";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(DuplicateContextPo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.duplicate_context")
        {
            return "po_parser.duplicate_context_not_rejected";
        }
        if (!Diagnostics.front().RelatedSpan.has_value())
        {
            return "po_parser.duplicate_context_missing_related_span";
        }
    }

    // 8. Negative: Duplicate msgid without context
    {
        const std::string DuplicateMsgidPo = R"po(
msgid "Same"
msgstr "Один"

msgid "Same"
msgstr "Два"
)po";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(DuplicateMsgidPo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.duplicate_msgid")
        {
            return "po_parser.duplicate_msgid_not_rejected";
        }
    }

    // 9. Negative: Unexpected syntax / tokens
    {
        const std::string SyntaxErrorPo = "some random non-po text here\n";
        std::vector<FDiagnostic> Diagnostics;
        auto Catalog = ParsePo(SyntaxErrorPo, Diagnostics);
        if (Catalog.has_value() || Diagnostics.empty() || Diagnostics.front().Code != "core:diagnostic.po.syntax_error")
        {
            return "po_parser.syntax_error_not_rejected";
        }
    }

    return "";
}
} // namespace GV2ContentCore::Testing
