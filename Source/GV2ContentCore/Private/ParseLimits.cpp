#include "GV2ContentCore/ParseLimits.h"

namespace GV2ContentCore
{
    std::optional<FUtf8Source> ValidateUtf8AndLimits(
        std::string_view RawInput,
        const FParseLimits& Limits,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        if (RawInput.size() > Limits.MaxFileSizeBytes)
        {
            FDiagnostic Diag;
            Diag.Code = "core:diagnostic.json5.limit.file_size";
            Diag.Severity = EDiagnosticSeverity::Error;
            Diag.Message = "Source file size (" + std::to_string(RawInput.size()) +
                           " bytes) exceeds maximum allowed limit (" +
                           std::to_string(Limits.MaxFileSizeBytes) + " bytes)";
            Diag.PackageId = std::move(PackageId);
            Diag.PackageLoadIndex = PackageLoadIndex;
            Diag.RelativeSource = std::move(RelativeSource);
            OutDiagnostics.push_back(std::move(Diag));
            return std::nullopt;
        }

        if (Limits.MaxNestingDepth > FParseLimits::MaxSupportedNestingDepth)
        {
            FDiagnostic Diag;
            Diag.Code = "core:diagnostic.json5.limit.invalid_configuration";
            Diag.Message = "Configured nesting depth exceeds the portable safety ceiling";
            Diag.PackageId = std::move(PackageId);
            Diag.PackageLoadIndex = PackageLoadIndex;
            Diag.RelativeSource = std::move(RelativeSource);
            OutDiagnostics.push_back(std::move(Diag));
            return std::nullopt;
        }

        bool bHasBOM = false;
        std::size_t OffsetShift = 0;
        std::string_view ViewToValidate = RawInput;

        if (RawInput.size() >= 3 &&
            static_cast<unsigned char>(RawInput[0]) == 0xEF &&
            static_cast<unsigned char>(RawInput[1]) == 0xBB &&
            static_cast<unsigned char>(RawInput[2]) == 0xBF)
        {
            bHasBOM = true;
            OffsetShift = 3;
            ViewToValidate = RawInput.substr(3);
        }

        std::uint32_t Line = 1;
        std::uint32_t Column = 1;

        std::size_t Index = 0;
        const std::size_t TotalSize = ViewToValidate.size();

        while (Index < TotalSize)
        {
            const unsigned char Byte = static_cast<unsigned char>(ViewToValidate[Index]);
            std::size_t SequenceLen = 0;
            bool bValidSeq = true;

            if (Byte <= 0x7F)
            {
                SequenceLen = 1;
            }
            else if (Byte >= 0xC2 && Byte <= 0xDF)
            {
                SequenceLen = 2;
                if (Index + 1 >= TotalSize ||
                    (static_cast<unsigned char>(ViewToValidate[Index + 1]) & 0xC0) != 0x80)
                {
                    bValidSeq = false;
                }
            }
            else if (Byte >= 0xE0 && Byte <= 0xEF)
            {
                SequenceLen = 3;
                if (Index + 2 >= TotalSize)
                {
                    bValidSeq = false;
                }
                else
                {
                    const unsigned char B1 = static_cast<unsigned char>(ViewToValidate[Index + 1]);
                    const unsigned char B2 = static_cast<unsigned char>(ViewToValidate[Index + 2]);
                    if ((B1 & 0xC0) != 0x80 || (B2 & 0xC0) != 0x80)
                    {
                        bValidSeq = false;
                    }
                    else if (Byte == 0xE0 && B1 < 0xA0)
                    {
                        bValidSeq = false;
                    }
                    else if (Byte == 0xED && B1 >= 0xA0)
                    {
                        bValidSeq = false;
                    }
                }
            }
            else if (Byte >= 0xF0 && Byte <= 0xF4)
            {
                SequenceLen = 4;
                if (Index + 3 >= TotalSize)
                {
                    bValidSeq = false;
                }
                else
                {
                    const unsigned char B1 = static_cast<unsigned char>(ViewToValidate[Index + 1]);
                    const unsigned char B2 = static_cast<unsigned char>(ViewToValidate[Index + 2]);
                    const unsigned char B3 = static_cast<unsigned char>(ViewToValidate[Index + 3]);
                    if ((B1 & 0xC0) != 0x80 || (B2 & 0xC0) != 0x80 || (B3 & 0xC0) != 0x80)
                    {
                        bValidSeq = false;
                    }
                    else if (Byte == 0xF0 && B1 < 0x90)
                    {
                        bValidSeq = false;
                    }
                    else if (Byte == 0xF4 && B1 > 0x8F)
                    {
                        bValidSeq = false;
                    }
                }
            }
            else
            {
                bValidSeq = false;
            }

            if (!bValidSeq)
            {
                FDiagnostic Diag;
                Diag.Code = "core:diagnostic.json5.invalid_utf8";
                Diag.Severity = EDiagnosticSeverity::Error;
                Diag.Message = "Source file contains invalid UTF-8 byte sequence";
                Diag.PackageId = std::move(PackageId);
                Diag.PackageLoadIndex = PackageLoadIndex;
                Diag.RelativeSource = std::move(RelativeSource);
                Diag.Span = FSourceSpan{ Line, Column, Line, Column + 1 };
                OutDiagnostics.push_back(std::move(Diag));
                return std::nullopt;
            }

            if (Byte == '\n')
            {
                Line++;
                Column = 1;
            }
            else if (Byte == '\r')
            {
                if (Index + 1 < TotalSize && ViewToValidate[Index + 1] == '\n')
                {
                    SequenceLen = 2;
                }
                Line++;
                Column = 1;
            }
            else
            {
                Column++;
            }

            Index += SequenceLen;
        }

        FUtf8Source Source;
        Source.CleanedView = ViewToValidate;
        Source.bHasBOM = bHasBOM;
        Source.ByteOffsetShift = OffsetShift;
        return Source;
    }

    bool CheckNestingDepth(
        std::size_t CurrentDepth,
        const FParseLimits& Limits,
        const FSourceSpan& Span,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        if (CurrentDepth > Limits.MaxNestingDepth)
        {
            FDiagnostic Diag;
            Diag.Code = "core:diagnostic.json5.limit.nesting_depth";
            Diag.Severity = EDiagnosticSeverity::Error;
            Diag.Message = "Nesting depth (" + std::to_string(CurrentDepth) +
                           ") exceeds maximum allowed limit (" +
                           std::to_string(Limits.MaxNestingDepth) + ")";
            Diag.PackageId = std::move(PackageId);
            Diag.PackageLoadIndex = PackageLoadIndex;
            Diag.RelativeSource = std::move(RelativeSource);
            Diag.Span = Span;
            OutDiagnostics.push_back(std::move(Diag));
            return false;
        }
        return true;
    }

    bool CheckStringLength(
        std::size_t StringLengthBytes,
        const FParseLimits& Limits,
        const FSourceSpan& Span,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        if (StringLengthBytes > Limits.MaxStringLengthBytes)
        {
            FDiagnostic Diag;
            Diag.Code = "core:diagnostic.json5.limit.string_length";
            Diag.Severity = EDiagnosticSeverity::Error;
            Diag.Message = "String length (" + std::to_string(StringLengthBytes) +
                           " bytes) exceeds maximum allowed limit (" +
                           std::to_string(Limits.MaxStringLengthBytes) + " bytes)";
            Diag.PackageId = std::move(PackageId);
            Diag.PackageLoadIndex = PackageLoadIndex;
            Diag.RelativeSource = std::move(RelativeSource);
            Diag.Span = Span;
            OutDiagnostics.push_back(std::move(Diag));
            return false;
        }
        return true;
    }

    bool CheckContainerEntries(
        std::size_t EntryCount,
        const FParseLimits& Limits,
        const FSourceSpan& Span,
        std::vector<FDiagnostic>& OutDiagnostics,
        std::optional<std::string> PackageId,
        std::optional<std::uint32_t> PackageLoadIndex,
        std::optional<std::string> RelativeSource)
    {
        if (EntryCount > Limits.MaxContainerEntries)
        {
            FDiagnostic Diag;
            Diag.Code = "core:diagnostic.json5.limit.container_entries";
            Diag.Severity = EDiagnosticSeverity::Error;
            Diag.Message = "Container entry count (" + std::to_string(EntryCount) +
                           ") exceeds maximum allowed limit (" +
                           std::to_string(Limits.MaxContainerEntries) + ")";
            Diag.PackageId = std::move(PackageId);
            Diag.PackageLoadIndex = PackageLoadIndex;
            Diag.RelativeSource = std::move(RelativeSource);
            Diag.Span = Span;
            OutDiagnostics.push_back(std::move(Diag));
            return false;
        }
        return true;
    }
}
