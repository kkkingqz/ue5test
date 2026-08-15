#include "Commands/RenameCommand.h"
#include "Support/CliOutput.h"
#include "Support/Json5AstRewriter.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace GV2ContentCli
{

namespace
{

struct FFileRenameWork
{
    std::string RelativeSource;
    std::filesystem::path AbsolutePath;
    std::string OriginalContent;
    std::string NewContent;
    std::size_t ReplacementsCount = 0;
};

} // namespace

int RunRename(
    const std::vector<std::string>& Positional,
    const EOutputFormat Format)
{
    if (Positional.size() != 3)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"rename requires <package-root>, <old-id>, and <new-id>\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename requires <package-root>, <old-id>, and <new-id>\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path PackageRoot = Positional[0];
    const std::string& OldId = Positional[1];
    const std::string& NewId = Positional[2];

    GV2ContentCore::FStableIdView OldIdView;
    GV2ContentCore::EStableIdError OldIdError = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(OldId, OldIdView, &OldIdError))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_definition_id\",\"message\":\"not a valid old definition id '"
                      << OldId << "'\",\"definition_id\":\"" << OldId << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: not a valid old definition id '" << OldId << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    GV2ContentCore::FStableIdView NewIdView;
    GV2ContentCore::EStableIdError NewIdError = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(NewId, NewIdView, &NewIdError))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_definition_id\",\"message\":\"not a valid new definition id '"
                      << NewId << "'\",\"definition_id\":\"" << NewId << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: not a valid new definition id '" << NewId << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (OldIdView.Kind != NewIdView.Kind)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"id_kind_mismatch\",\"message\":\"New definition ID kind '"
                      << NewIdView.Kind << "' does not match old definition ID kind '"
                      << OldIdView.Kind << "'\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: new definition ID kind '" << NewIdView.Kind
                      << "' does not match old definition ID kind '" << OldIdView.Kind << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (OldId == NewId)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"ok\",\"old_id\":";
            WriteJsonEscapedString(std::cout, OldId);
            std::cout << ",\"new_id\":";
            WriteJsonEscapedString(std::cout, NewId);
            std::cout << ",\"files_modified_count\":0,\"replacements_count\":0,\"modified_files\":[]}\n";
        }
        else
        {
            std::cout << "renamed " << OldId << " -> " << NewId << "\n";
            std::cout << "files modified: 0\n";
            std::cout << "replacements: 0\n";
        }
        return static_cast<int>(EExitCode::Success);
    }

    std::error_code RootEc;
    if (!std::filesystem::exists(PackageRoot, RootEc) || !std::filesystem::is_directory(PackageRoot, RootEc))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\"package root not found or not a directory\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: package root not found or not a directory\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::vector<GV2ContentCore::FDiagnostic> DiscoveryDiagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> PackageDescriptor =
        GV2ContentHostSupport::DiscoverPackageFromDirectory(PackageRoot, DiscoveryDiagnostics);
    if (!PackageDescriptor)
    {
        return EmitDiagnosticsFailure(DiscoveryDiagnostics, Format);
    }

    const GV2ContentCore::FParseLimits Limits;

    // Check if package is frozen/published
    const std::filesystem::path PackageDescriptorPath = PackageRoot / "package.json5";
    if (std::filesystem::exists(PackageDescriptorPath))
    {
        std::ifstream PackageStream(PackageDescriptorPath, std::ios::binary);
        if (PackageStream)
        {
            const std::string PkgContent((std::istreambuf_iterator<char>(PackageStream)), std::istreambuf_iterator<char>());
            std::vector<GV2ContentCore::FDiagnostic> PkgDiagnostics;
            const auto PkgDoc = GV2ContentCore::ParseJson5(PkgContent, Limits, PkgDiagnostics, PackageDescriptor->GetPackageId(), 0, "package.json5");
            if (PkgDoc.has_value() && PkgDoc->IsObject())
            {
                const GV2ContentCore::FValue* FrozenField = PkgDoc->FindField("frozen");
                const GV2ContentCore::FValue* PublishedField = PkgDoc->FindField("published");
                const bool bIsFrozen = (FrozenField && FrozenField->IsBoolean() && FrozenField->AsBoolean())
                                    || (PublishedField && PublishedField->IsBoolean() && PublishedField->AsBoolean());
                if (bIsFrozen)
                {
                    if (Format == EOutputFormat::Json)
                    {
                        std::cout << "{\"status\":\"error\",\"code\":\"package_frozen\",\"message\":\"package is published/frozen and cannot be modified in-place with rename. Use redirects instead.\",\"package_id\":\""
                                  << PackageDescriptor->GetPackageId() << "\"}\n";
                    }
                    else
                    {
                        std::cerr << "gv2-content: rename failed: package is published/frozen and cannot be modified in-place with rename. Use redirects instead.\n";
                    }
                    return static_cast<int>(EExitCode::ToolFailure);
                }
            }
        }
    }

    bool bOldIdFound = false;
    bool bNewIdExists = false;

    // Check if new ID is in redirects or tombstones
    for (const GV2ContentCore::FRedirectDescriptor& Redirect : PackageDescriptor->GetRedirects())
    {
        if (Redirect.GetSourceId() == NewId || Redirect.GetTargetId() == NewId)
        {
            bNewIdExists = true;
            break;
        }
    }
    for (const std::string& Tombstone : PackageDescriptor->GetTombstones())
    {
        if (Tombstone == NewId)
        {
            bNewIdExists = true;
            break;
        }
    }

    // Check definition files for OldId and NewId
    for (const std::string& RelativeSource : PackageDescriptor->GetRelativeSources())
    {
        const std::filesystem::path SourcePath = PackageRoot / RelativeSource;
        std::ifstream Stream(SourcePath, std::ios::binary);
        if (!Stream)
        {
            continue;
        }
        const std::string Content((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        const auto Doc = GV2ContentCore::ParseJson5Document(
            Content, Limits, Diagnostics, PackageDescriptor->GetPackageId(), 0, RelativeSource);
        if (!Doc.has_value())
        {
            continue;
        }

        const auto Envelope = GV2ContentCore::ParseDefinitionFileEnvelope(
            *Doc, PackageDescriptor->GetPackageId(), 0, RelativeSource, Diagnostics);
        if (Envelope.has_value())
        {
            for (const GV2ContentCore::FDefinitionEntry& Entry : Envelope->GetDefinitions())
            {
                if (Entry.GetId() == OldId)
                {
                    bOldIdFound = true;
                }
                if (Entry.GetId() == NewId)
                {
                    bNewIdExists = true;
                }
            }
        }
    }

    if (bNewIdExists)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"duplicate_definition_id\",\"message\":\"Definition ID '"
                      << NewId << "' already exists in package\",\"definition_id\":\"" << NewId << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: definition ID '" << NewId << "' already exists in package\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (!bOldIdFound)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"source_definition_not_found\",\"message\":\"Definition ID '"
                      << OldId << "' not found in package\",\"definition_id\":\"" << OldId << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: definition ID '" << OldId << "' not found in package\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    // Collect all candidate files to scan
    std::vector<std::string> FilesToScan = PackageDescriptor->GetRelativeSources();
    if (std::filesystem::exists(PackageRoot / "package.json5"))
    {
        FilesToScan.push_back("package.json5");
    }

    std::vector<FFileRenameWork> WorkItems;
    std::size_t TotalReplacements = 0;

    for (const std::string& RelativeSource : FilesToScan)
    {
        const std::filesystem::path SourcePath = PackageRoot / RelativeSource;
        std::ifstream Stream(SourcePath, std::ios::binary);
        if (!Stream)
        {
            continue;
        }
        const std::string RawContent((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());

        auto ReplaceResult = ReplaceStringTokens(
            RawContent, OldId, NewId, PackageDescriptor->GetPackageId(), RelativeSource);
        if (!ReplaceResult.bSuccess)
        {
            if (Format == EOutputFormat::Json)
            {
                std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":";
                WriteJsonEscapedString(std::cout, ReplaceResult.ErrorMessage);
                std::cout << "}\n";
            }
            else
            {
                std::cerr << "gv2-content: rename failed: " << ReplaceResult.ErrorMessage << "\n";
            }
            return static_cast<int>(EExitCode::ToolFailure);
        }

        if (ReplaceResult.ReplacementsCount == 0)
        {
            continue;
        }

        FFileRenameWork Work;
        Work.RelativeSource = RelativeSource;
        Work.AbsolutePath = SourcePath;
        Work.OriginalContent = RawContent;
        Work.NewContent = std::move(ReplaceResult.UpdatedContent);
        Work.ReplacementsCount = ReplaceResult.ReplacementsCount;
        TotalReplacements += Work.ReplacementsCount;
        WorkItems.push_back(std::move(Work));
    }

    if (WorkItems.empty())
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"source_definition_not_found\",\"message\":\"no tokens matching "
                      << OldId << " found in package files\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: rename failed: no tokens matching " << OldId << " found in package files\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    // Atomic write phase: write each file atomically
    for (const FFileRenameWork& Work : WorkItems)
    {
        const std::filesystem::path TempPath = Work.AbsolutePath.string() + ".tmp_rename";
        std::ofstream OutStream(TempPath, std::ios::binary | std::ios::trunc);
        if (!OutStream)
        {
            if (Format == EOutputFormat::Json)
            {
                std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\"failed to write temporary file for "
                          << Work.RelativeSource << "\"}\n";
            }
            else
            {
                std::cerr << "gv2-content: rename failed: failed to write temporary file for " << Work.RelativeSource << "\n";
            }
            return static_cast<int>(EExitCode::ToolFailure);
        }
        OutStream.write(Work.NewContent.data(), static_cast<std::streamsize>(Work.NewContent.size()));
        OutStream.close();

        std::error_code RenameEc;
        std::filesystem::rename(TempPath, Work.AbsolutePath, RenameEc);
        if (RenameEc)
        {
            if (Format == EOutputFormat::Json)
            {
                std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\"failed to atomically replace file "
                          << Work.RelativeSource << "\"}\n";
            }
            else
            {
                std::cerr << "gv2-content: rename failed: failed to atomically replace file " << Work.RelativeSource << "\n";
            }
            return static_cast<int>(EExitCode::ToolFailure);
        }
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"old_id\":";
        WriteJsonEscapedString(std::cout, OldId);
        std::cout << ",\"new_id\":";
        WriteJsonEscapedString(std::cout, NewId);
        std::cout << ",\"files_modified_count\":" << WorkItems.size()
                  << ",\"replacements_count\":" << TotalReplacements
                  << ",\"modified_files\":[";
        for (std::size_t Index = 0; Index < WorkItems.size(); ++Index)
        {
            if (Index > 0)
            {
                std::cout << ",";
            }
            const auto& Work = WorkItems[Index];
            std::cout << "{\"source_file\":";
            WriteJsonEscapedString(std::cout, Work.RelativeSource);
            std::cout << ",\"replacements_count\":" << Work.ReplacementsCount
                      << "}";
        }
        std::cout << "]}\n";
    }
    else
    {
        std::cout << "renamed " << OldId << " -> " << NewId << "\n";
        std::cout << "files modified: " << WorkItems.size() << "\n";
        std::cout << "replacements: " << TotalReplacements << "\n";
        for (const auto& Work : WorkItems)
        {
            std::cout << "  " << Work.RelativeSource << " (" << Work.ReplacementsCount << " replacement"
                      << (Work.ReplacementsCount == 1 ? "" : "s") << ")\n";
        }
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
