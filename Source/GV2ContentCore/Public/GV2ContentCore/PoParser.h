#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Diagnostic.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GV2ContentCore
{
    /**
     * Single PO translation entry.
     */
    struct GV2_CONTENT_CORE_API FPoEntry final
    {
        std::string MsgCtxt; // Translation context (holds Stable ID text_id, e.g. "core:text.item.iron_sword.name")
        std::string MsgId;   // Source message in authoring language
        std::string MsgStr;  // Translated message
        std::vector<std::string> Comments;
        uint32_t SourceLine = 1;

        bool operator==(const FPoEntry& Other) const;
        bool operator!=(const FPoEntry& Other) const;
    };

    /**
     * In-memory parsed PO localization catalog.
     */
    struct GV2_CONTENT_CORE_API FPoCatalog final
    {
        std::vector<std::pair<std::string, std::string>> Headers; // e.g. {"Language", "ru"}, {"Content-Type", ...}
        std::vector<FPoEntry> Entries;
        std::unordered_map<std::string, std::size_t> IndexByContext; // MsgCtxt -> index in Entries
        std::unordered_map<std::string, std::size_t> IndexById;      // MsgId -> index in Entries

        const FPoEntry* FindByContext(std::string_view TextId) const;
        const FPoEntry* FindById(std::string_view MsgId) const;
        std::string GetHeader(std::string_view HeaderName) const;
    };

    /**
     * Options for parsing a PO document.
     */
    struct GV2_CONTENT_CORE_API FPoParseOptions final
    {
        std::string PackageId;
        std::string RelativeSource;
    };

    /**
     * Portable parser for GNU gettext PO (Portable Object) translation catalogs.
     * Pure in-memory parser: parses UTF-8 string input, validates entry structure and escapes,
     * tracks exact line numbers, and fails atomically without producing partial catalogs on error.
     */
    GV2_CONTENT_CORE_API std::optional<FPoCatalog> ParsePo(
        std::string_view PoContent,
        std::vector<FDiagnostic>& OutDiagnostics,
        const FPoParseOptions& Options = {});
}
