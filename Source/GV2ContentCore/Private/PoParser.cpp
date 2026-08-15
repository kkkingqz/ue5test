#include "GV2ContentCore/PoParser.h"

#include <algorithm>
#include <sstream>

namespace GV2ContentCore
{
bool FPoEntry::operator==(const FPoEntry& Other) const
{
    return MsgCtxt == Other.MsgCtxt
        && MsgId == Other.MsgId
        && MsgStr == Other.MsgStr
        && Comments == Other.Comments;
}

bool FPoEntry::operator!=(const FPoEntry& Other) const
{
    return !(*this == Other);
}

const FPoEntry* FPoCatalog::FindByContext(std::string_view TextId) const
{
    const auto It = IndexByContext.find(std::string(TextId));
    if (It != IndexByContext.end() && It->second < Entries.size())
    {
        return &Entries[It->second];
    }
    return nullptr;
}

const FPoEntry* FPoCatalog::FindById(std::string_view MsgId) const
{
    const auto It = IndexById.find(std::string(MsgId));
    if (It != IndexById.end() && It->second < Entries.size())
    {
        return &Entries[It->second];
    }
    return nullptr;
}

std::string FPoCatalog::GetHeader(std::string_view HeaderName) const
{
    for (const auto& [Key, Value] : Headers)
    {
        if (Key == HeaderName)
        {
            return Value;
        }
    }
    return "";
}

namespace
{
enum class EPoTargetField
{
    None,
    MsgCtxt,
    MsgId,
    MsgStr
};

void AddDiagnostic(
    std::vector<FDiagnostic>& Diagnostics,
    const std::string& Code,
    const std::string& Message,
    uint32_t Line,
    uint32_t Column,
    const FPoParseOptions& Options,
    std::optional<FSourceSpan> RelatedSpan = std::nullopt,
    std::optional<std::string> RelatedMessage = std::nullopt)
{
    FDiagnostic Diag;
    Diag.Code = Code;
    Diag.Severity = EDiagnosticSeverity::Error;
    Diag.Message = Message;
    Diag.Span = FSourceSpan{ Line, Column, Line, Column };
    if (!Options.PackageId.empty())
    {
        Diag.PackageId = Options.PackageId;
    }
    if (!Options.RelativeSource.empty())
    {
        Diag.RelativeSource = Options.RelativeSource;
    }
    Diag.RelatedSpan = RelatedSpan;
    Diag.RelatedMessage = RelatedMessage;
    Diagnostics.push_back(std::move(Diag));
}

bool ParseQuotedString(
    std::string_view Line,
    size_t& Pos,
    std::string& OutDecoded,
    uint32_t LineNum,
    std::vector<FDiagnostic>& Diagnostics,
    const FPoParseOptions& Options)
{
    while (Pos < Line.size() && (Line[Pos] == ' ' || Line[Pos] == '\t'))
    {
        ++Pos;
    }

    if (Pos >= Line.size() || Line[Pos] != '"')
    {
        AddDiagnostic(
            Diagnostics,
            "core:diagnostic.po.expected_string",
            "expected quoted string literal",
            LineNum,
            static_cast<uint32_t>(Pos + 1),
            Options);
        return false;
    }

    const uint32_t StringStartCol = static_cast<uint32_t>(Pos + 1);
    ++Pos; // skip opening quote

    std::string Decoded;
    bool bClosed = false;

    while (Pos < Line.size())
    {
        const char C = Line[Pos];
        if (C == '"')
        {
            ++Pos;
            bClosed = true;
            break;
        }

        if (C == '\\')
        {
            ++Pos;
            if (Pos >= Line.size())
            {
                AddDiagnostic(
                    Diagnostics,
                    "core:diagnostic.po.invalid_escape",
                    "unterminated escape sequence at end of line",
                    LineNum,
                    static_cast<uint32_t>(Pos),
                    Options);
                return false;
            }

            const char Esc = Line[Pos];
            ++Pos;
            switch (Esc)
            {
                case '\\': Decoded.push_back('\\'); break;
                case '"':  Decoded.push_back('"'); break;
                case 'n':  Decoded.push_back('\n'); break;
                case 'r':  Decoded.push_back('\r'); break;
                case 't':  Decoded.push_back('\t'); break;
                case 'a':  Decoded.push_back('\a'); break;
                case 'b':  Decoded.push_back('\b'); break;
                case 'v':  Decoded.push_back('\v'); break;
                case 'f':  Decoded.push_back('\f'); break;
                case '\'': Decoded.push_back('\''); break;
                default:
                    AddDiagnostic(
                        Diagnostics,
                        "core:diagnostic.po.invalid_escape",
                        std::string("invalid escape sequence '\\") + Esc + "'",
                        LineNum,
                        static_cast<uint32_t>(Pos),
                        Options);
                    return false;
            }
        }
        else
        {
            Decoded.push_back(C);
            ++Pos;
        }
    }

    if (!bClosed)
    {
        AddDiagnostic(
            Diagnostics,
            "core:diagnostic.po.unterminated_string",
            "unterminated string literal",
            LineNum,
            StringStartCol,
            Options);
        return false;
    }

    while (Pos < Line.size())
    {
        if (Line[Pos] != ' ' && Line[Pos] != '\t' && Line[Pos] != '\r')
        {
            AddDiagnostic(
                Diagnostics,
                "core:diagnostic.po.unexpected_token",
                "unexpected trailing characters after string literal",
                LineNum,
                static_cast<uint32_t>(Pos + 1),
                Options);
            return false;
        }
        ++Pos;
    }

    OutDecoded += Decoded;
    return true;
}
} // namespace

std::optional<FPoCatalog> ParsePo(
    std::string_view PoContent,
    std::vector<FDiagnostic>& OutDiagnostics,
    const FPoParseOptions& Options)
{
    // Strip leading UTF-8 BOM if present
    if (PoContent.size() >= 3
        && static_cast<unsigned char>(PoContent[0]) == 0xEF
        && static_cast<unsigned char>(PoContent[1]) == 0xBB
        && static_cast<unsigned char>(PoContent[2]) == 0xBF)
    {
        PoContent.remove_prefix(3);
    }

    FPoCatalog Catalog;
    std::vector<FDiagnostic> Diagnostics;

    std::string CurrentMsgCtxt;
    std::string CurrentMsgId;
    std::string CurrentMsgStr;
    std::vector<std::string> CurrentComments;

    bool bHasMsgCtxt = false;
    bool bHasMsgId = false;
    bool bHasMsgStr = false;
    EPoTargetField TargetField = EPoTargetField::None;
    uint32_t CurrentEntryStartLine = 0;

    auto CommitEntry = [&]() -> bool
    {
        if (!bHasMsgId && !bHasMsgCtxt && !bHasMsgStr)
        {
            CurrentComments.clear();
            return true;
        }

        if (bHasMsgId && !bHasMsgStr)
        {
            AddDiagnostic(
                Diagnostics,
                "core:diagnostic.po.missing_msgstr",
                "entry is missing msgstr",
                CurrentEntryStartLine,
                1,
                Options);
            return false;
        }

        if (!bHasMsgId && bHasMsgStr)
        {
            AddDiagnostic(
                Diagnostics,
                "core:diagnostic.po.missing_msgid",
                "entry is missing msgid",
                CurrentEntryStartLine,
                1,
                Options);
            return false;
        }

        // Header entry: empty msgid and empty msgctxt
        if (CurrentMsgId.empty() && CurrentMsgCtxt.empty() && Catalog.Headers.empty() && Catalog.Entries.empty())
        {
            std::string_view HeaderText = CurrentMsgStr;
            size_t HeaderOffset = 0;
            while (HeaderOffset < HeaderText.size())
            {
                size_t NextNewline = HeaderText.find('\n', HeaderOffset);
                std::string_view HeaderLine = (NextNewline == std::string_view::npos)
                    ? HeaderText.substr(HeaderOffset)
                    : HeaderText.substr(HeaderOffset, NextNewline - HeaderOffset);
                HeaderOffset = (NextNewline == std::string_view::npos)
                    ? HeaderText.size()
                    : NextNewline + 1;

                if (HeaderLine.empty()) continue;
                size_t Colon = HeaderLine.find(':');
                if (Colon != std::string_view::npos)
                {
                    std::string Key(HeaderLine.substr(0, Colon));
                    std::string Val(HeaderLine.substr(Colon + 1));
                    while (!Val.empty() && (Val.front() == ' ' || Val.front() == '\t'))
                    {
                        Val.erase(Val.begin());
                    }
                    while (!Val.empty() && (Val.back() == '\r' || Val.back() == ' ' || Val.back() == '\t'))
                    {
                        Val.pop_back();
                    }
                    Catalog.Headers.emplace_back(std::move(Key), std::move(Val));
                }
            }
        }
        else
        {
            if (!CurrentMsgCtxt.empty())
            {
                auto [It, Inserted] = Catalog.IndexByContext.emplace(CurrentMsgCtxt, Catalog.Entries.size());
                if (!Inserted)
                {
                    const uint32_t PrevLine = Catalog.Entries[It->second].SourceLine;
                    AddDiagnostic(
                        Diagnostics,
                        "core:diagnostic.po.duplicate_context",
                        "duplicate msgctxt '" + CurrentMsgCtxt + "' in catalog",
                        CurrentEntryStartLine,
                        1,
                        Options,
                        FSourceSpan{ PrevLine, 1, PrevLine, 1 },
                        "first defined here");
                    return false;
                }
            }
            else if (!CurrentMsgId.empty())
            {
                auto [It, Inserted] = Catalog.IndexById.emplace(CurrentMsgId, Catalog.Entries.size());
                if (!Inserted)
                {
                    const uint32_t PrevLine = Catalog.Entries[It->second].SourceLine;
                    AddDiagnostic(
                        Diagnostics,
                        "core:diagnostic.po.duplicate_msgid",
                        "duplicate msgid '" + CurrentMsgId + "' without context in catalog",
                        CurrentEntryStartLine,
                        1,
                        Options,
                        FSourceSpan{ PrevLine, 1, PrevLine, 1 },
                        "first defined here");
                    return false;
                }
            }

            FPoEntry Entry;
            Entry.MsgCtxt = std::move(CurrentMsgCtxt);
            Entry.MsgId = std::move(CurrentMsgId);
            Entry.MsgStr = std::move(CurrentMsgStr);
            Entry.Comments = std::move(CurrentComments);
            Entry.SourceLine = CurrentEntryStartLine;
            Catalog.Entries.push_back(std::move(Entry));
        }

        CurrentMsgCtxt.clear();
        CurrentMsgId.clear();
        CurrentMsgStr.clear();
        CurrentComments.clear();
        bHasMsgCtxt = false;
        bHasMsgId = false;
        bHasMsgStr = false;
        TargetField = EPoTargetField::None;
        CurrentEntryStartLine = 0;
        return true;
    };

    size_t Offset = 0;
    uint32_t LineNumber = 1;

    while (Offset < PoContent.size())
    {
        size_t NextNewline = PoContent.find('\n', Offset);
        std::string_view Line = (NextNewline == std::string_view::npos)
            ? PoContent.substr(Offset)
            : PoContent.substr(Offset, NextNewline - Offset);
        Offset = (NextNewline == std::string_view::npos)
            ? PoContent.size()
            : NextNewline + 1;

        if (!Line.empty() && Line.back() == '\r')
        {
            Line.remove_suffix(1);
        }

        size_t NonWs = 0;
        while (NonWs < Line.size() && (Line[NonWs] == ' ' || Line[NonWs] == '\t'))
        {
            ++NonWs;
        }

        if (NonWs >= Line.size())
        {
            // Empty line: entry boundary
            if (!CommitEntry())
            {
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }
            ++LineNumber;
            continue;
        }

        if (Line[NonWs] == '#')
        {
            CurrentComments.emplace_back(Line);
            ++LineNumber;
            continue;
        }

        std::string_view Rest = Line.substr(NonWs);

        if (Rest.starts_with("msgctxt"))
        {
            if (bHasMsgId)
            {
                if (!CommitEntry())
                {
                    OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                    return std::nullopt;
                }
            }
            if (bHasMsgCtxt)
            {
                AddDiagnostic(
                    Diagnostics,
                    "core:diagnostic.po.duplicate_keyword",
                    "duplicate msgctxt in single entry",
                    LineNumber,
                    static_cast<uint32_t>(NonWs + 1),
                    Options);
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }

            size_t Pos = NonWs + 7;
            if (!ParseQuotedString(Line, Pos, CurrentMsgCtxt, LineNumber, Diagnostics, Options))
            {
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }
            bHasMsgCtxt = true;
            TargetField = EPoTargetField::MsgCtxt;
            if (CurrentEntryStartLine == 0)
            {
                CurrentEntryStartLine = LineNumber;
            }
        }
        else if (Rest.starts_with("msgid"))
        {
            if (bHasMsgId)
            {
                if (!CommitEntry())
                {
                    OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                    return std::nullopt;
                }
            }

            size_t Pos = NonWs + 5;
            if (!ParseQuotedString(Line, Pos, CurrentMsgId, LineNumber, Diagnostics, Options))
            {
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }
            bHasMsgId = true;
            TargetField = EPoTargetField::MsgId;
            if (CurrentEntryStartLine == 0)
            {
                CurrentEntryStartLine = LineNumber;
            }
        }
        else if (Rest.starts_with("msgstr"))
        {
            if (!bHasMsgId)
            {
                AddDiagnostic(
                    Diagnostics,
                    "core:diagnostic.po.missing_msgid",
                    "msgstr without preceding msgid",
                    LineNumber,
                    static_cast<uint32_t>(NonWs + 1),
                    Options);
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }

            size_t Pos = NonWs + 6;
            // Handle optional plural index e.g. msgstr[0]
            if (Pos < Line.size() && Line[Pos] == '[')
            {
                size_t CloseBracket = Line.find(']', Pos);
                if (CloseBracket == std::string_view::npos)
                {
                    AddDiagnostic(
                        Diagnostics,
                        "core:diagnostic.po.syntax_error",
                        "unclosed bracket in msgstr index",
                        LineNumber,
                        static_cast<uint32_t>(Pos + 1),
                        Options);
                    OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                    return std::nullopt;
                }
                Pos = CloseBracket + 1;
            }

            if (!ParseQuotedString(Line, Pos, CurrentMsgStr, LineNumber, Diagnostics, Options))
            {
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }
            bHasMsgStr = true;
            TargetField = EPoTargetField::MsgStr;
        }
        else if (Line[NonWs] == '"')
        {
            // Continuation line
            size_t Pos = NonWs;
            std::string Continuation;
            if (!ParseQuotedString(Line, Pos, Continuation, LineNumber, Diagnostics, Options))
            {
                OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                return std::nullopt;
            }

            switch (TargetField)
            {
                case EPoTargetField::MsgCtxt: CurrentMsgCtxt += Continuation; break;
                case EPoTargetField::MsgId:   CurrentMsgId += Continuation; break;
                case EPoTargetField::MsgStr:  CurrentMsgStr += Continuation; break;
                default:
                    AddDiagnostic(
                        Diagnostics,
                        "core:diagnostic.po.syntax_error",
                        "continuation string without preceding keyword",
                        LineNumber,
                        static_cast<uint32_t>(NonWs + 1),
                        Options);
                    OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
                    return std::nullopt;
            }
        }
        else
        {
            AddDiagnostic(
                Diagnostics,
                "core:diagnostic.po.syntax_error",
                "unexpected token or keyword in PO line",
                LineNumber,
                static_cast<uint32_t>(NonWs + 1),
                Options);
            OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
            return std::nullopt;
        }

        ++LineNumber;
    }

    if (!CommitEntry())
    {
        OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
        return std::nullopt;
    }

    if (!Diagnostics.empty())
    {
        OutDiagnostics.insert(OutDiagnostics.end(), Diagnostics.begin(), Diagnostics.end());
        return std::nullopt;
    }

    return Catalog;
}
} // namespace GV2ContentCore
