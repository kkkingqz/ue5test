#include "Commands/NewCommand.h"
#include "Support/CliOutput.h"
#include "Support/Json5AstRewriter.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/DefinitionEnvelope.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentCore/StableId.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace GV2ContentCli
{

int RunNew(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 3)
    {
        std::cerr << "usage: gv2-content new <package-root> <definition-type> <definition-id> [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path RawRoot = Positional[0];
    const std::string& DefinitionType = Positional[1];
    const std::string& DefinitionIdStr = Positional[2];

    GV2ContentCore::FStableIdView StableIdView;
    GV2ContentCore::EStableIdError IdError = GV2ContentCore::EStableIdError::None;
    if (!GV2ContentCore::FStableId::Parse(DefinitionIdStr, StableIdView, &IdError))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"invalid_definition_id\",\"message\":\"'"
                      << DefinitionIdStr << "' is not a valid definition id\",\"definition_id\":\""
                      << DefinitionIdStr << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: '" << DefinitionIdStr << "' is not a valid definition id\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    if (StableIdView.Kind != DefinitionType)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"id_kind_mismatch\",\"message\":\"Definition ID kind '"
                      << StableIdView.Kind << "' does not match definition type '" << DefinitionType
                      << "'\",\"definition_id\":\"" << DefinitionIdStr << "\",\"definition_type\":\""
                      << DefinitionType << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: definition ID kind '" << StableIdView.Kind
                      << "' does not match definition type '" << DefinitionType << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"tool_failure\",\"message\":\"package root not found or not a directory\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: package root not found or not a directory\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    // The new entry is written into the package that was asked for, but the schema describing it
    // usually belongs to that package's "core" dependency — resolve it across the whole set.
    FPackageSetDiscovery Set = DiscoverPackageSet({Root});
    if (Set.bToolFailure)
    {
        std::cerr << "gv2-content: " << Set.ToolFailureMessage << "\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }
    if (Set.bDiscoveryFailed || Set.Descriptors.empty())
    {
        return EmitDiagnosticsFailure(Set.Diagnostics, Format);
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    const GV2ContentCore::FPackageDescriptor* Descriptor = &Set.Descriptors[Set.TargetIndex()];

    std::size_t SchemaPackageIndex = 0;
    const GV2ContentCore::FSchemaBinding* MatchingSchemaBinding = nullptr;
    if (!FindSchemaBindingInSet(Set, DefinitionType, SchemaPackageIndex, MatchingSchemaBinding))
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"unknown_definition_type\",\"message\":\"Unknown definition type '"
                      << DefinitionType << "' in package '" << Descriptor->GetPackageId()
                      << "' or its dependencies\",\"definition_type\":\""
                      << DefinitionType << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: unknown definition type '" << DefinitionType
                      << "' in package '" << Descriptor->GetPackageId() << "' or its dependencies\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    FFilesystemContentSourceProvider Provider(Root, Descriptor->GetPackageId());

    const GV2ContentCore::FPackageDescriptor& SchemaDescriptor = Set.Descriptors[SchemaPackageIndex];
    FFilesystemContentSourceProvider SchemaProvider(
        Set.Roots[SchemaPackageIndex], SchemaDescriptor.GetPackageId());
    std::optional<std::string> SchemaSource = SchemaProvider.ReadSource(
        SchemaDescriptor.GetPackageId(), MatchingSchemaBinding->GetRelativePath());
    if (!SchemaSource)
    {
        std::cerr << "gv2-content: failed to read schema source '" << MatchingSchemaBinding->GetRelativePath() << "'\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    GV2ContentCore::FParseLimits Limits;
    auto SchemaDoc = GV2ContentCore::ParseJson5Document(
        *SchemaSource,
        Limits,
        Diagnostics,
        SchemaDescriptor.GetPackageId(),
        SchemaDescriptor.GetLoadIndex(),
        MatchingSchemaBinding->GetRelativePath());

    if (!SchemaDoc)
    {
        return EmitDiagnosticsFailure(Diagnostics, Format);
    }

    auto SchemaResourceOpt = GV2ContentCore::ParseSchemaResource(
        *SchemaDoc,
        *MatchingSchemaBinding,
        SchemaDescriptor.GetPackageId(),
        SchemaDescriptor.GetLoadIndex(),
        MatchingSchemaBinding->GetRelativePath(),
        Diagnostics);

    if (!SchemaResourceOpt || !Diagnostics.empty())
    {
        return EmitDiagnosticsFailure(Diagnostics, Format);
    }

    const GV2ContentCore::FSchemaResource& Schema = *SchemaResourceOpt;

    std::string TargetRelativeSource;
    for (const std::string& RelSource : Descriptor->GetRelativeSources())
    {
        std::optional<std::string> FileSource = Provider.ReadSource(Descriptor->GetPackageId(), RelSource);
        if (!FileSource) continue;

        std::vector<GV2ContentCore::FDiagnostic> FileDiagnostics;
        auto ParsedDoc = GV2ContentCore::ParseJson5Document(
            *FileSource, Limits, FileDiagnostics, Descriptor->GetPackageId(), Descriptor->GetLoadIndex(), RelSource);
        if (!ParsedDoc) continue;

        auto DefFileOpt = GV2ContentCore::ParseDefinitionFileEnvelope(
            *ParsedDoc, Descriptor->GetPackageId(), Descriptor->GetLoadIndex(), RelSource, FileDiagnostics);
        if (!DefFileOpt) continue;

        if (DefFileOpt->GetDefinitionType() == DefinitionType && TargetRelativeSource.empty())
        {
            TargetRelativeSource = RelSource;
        }

        for (const auto& Entry : DefFileOpt->GetDefinitions())
        {
            if (Entry.GetId() == DefinitionIdStr)
            {
                if (Format == EOutputFormat::Json)
                {
                    std::cout << "{\"status\":\"error\",\"code\":\"duplicate_definition_id\",\"message\":\"Definition ID '"
                              << DefinitionIdStr << "' already exists in package '" << Descriptor->GetPackageId()
                              << "'\",\"definition_id\":\"" << DefinitionIdStr << "\"}\n";
                }
                else
                {
                    std::cerr << "gv2-content: definition ID '" << DefinitionIdStr
                              << "' already exists in package '" << Descriptor->GetPackageId() << "'\n";
                }
                return static_cast<int>(EExitCode::ToolFailure);
            }
        }
    }

    if (TargetRelativeSource.empty())
    {
        TargetRelativeSource = "definitions/" + DefinitionType + "s.json5";
    }

    GV2ContentCore::FValue PlaceholderData;
    if (Schema.GetCompiledRootSpec() != nullptr)
    {
        PlaceholderData = GeneratePlaceholderValue(
            *Schema.GetCompiledRootSpec(),
            std::string(StableIdView.Namespace),
            DefinitionType,
            std::string(StableIdView.Path),
            "");
    }
    else
    {
        PlaceholderData = GV2ContentCore::FValue(GV2ContentCore::FValue::FObject{});
    }

    std::string FormattedEntry = FormatDefinitionEntry(DefinitionIdStr, PlaceholderData, 2);

    const std::filesystem::path TargetFilePath = Root / TargetRelativeSource;
    if (std::filesystem::exists(TargetFilePath))
    {
        std::ifstream InStream(TargetFilePath, std::ios::binary);
        if (!InStream)
        {
            std::cerr << "gv2-content: failed to read target file '" << TargetRelativeSource << "'\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        std::string ExistingContent((std::istreambuf_iterator<char>(InStream)), std::istreambuf_iterator<char>());
        InStream.close();

        std::string NewContent;
        std::string ErrorMessage;
        if (!InsertDefinitionEntryIntoJson5(ExistingContent, FormattedEntry, NewContent, ErrorMessage))
        {
            std::cerr << "gv2-content: failed to insert definition entry into '" << TargetRelativeSource
                      << "': " << ErrorMessage << "\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }

        std::ofstream OutStream(TargetFilePath, std::ios::binary | std::ios::trunc);
        if (!OutStream)
        {
            std::cerr << "gv2-content: failed to write to target file '" << TargetRelativeSource << "'\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        OutStream << NewContent;
        OutStream.close();
    }
    else
    {
        std::filesystem::create_directories(TargetFilePath.parent_path(), Ec);
        std::string NewFileContent = CreateNewDefinitionFileContent(
            Schema.GetKey().SchemaVersion,
            DefinitionType,
            FormattedEntry);

        std::ofstream OutStream(TargetFilePath, std::ios::binary | std::ios::trunc);
        if (!OutStream)
        {
            std::cerr << "gv2-content: failed to create target file '" << TargetRelativeSource << "'\n";
            return static_cast<int>(EExitCode::ToolFailure);
        }
        OutStream << NewFileContent;
        OutStream.close();
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"definition_id\":";
        WriteJsonEscapedString(std::cout, DefinitionIdStr);
        std::cout << ",\"definition_type\":";
        WriteJsonEscapedString(std::cout, DefinitionType);
        std::cout << ",\"relative_source\":";
        WriteJsonEscapedString(std::cout, TargetRelativeSource);
        std::cout << "}\n";
    }
    else
    {
        std::cout << "created definition " << DefinitionIdStr << " in " << TargetRelativeSource << "\n";
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
