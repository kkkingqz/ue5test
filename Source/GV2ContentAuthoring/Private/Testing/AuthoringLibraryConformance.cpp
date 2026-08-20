#include "GV2ContentAuthoring/Testing/AuthoringLibraryConformance.h"
#include "GV2ContentAuthoring/AuthoringService.h"
#include "GV2ContentAuthoring/Json5AstRewriter.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace GV2ContentAuthoring::Testing
{

namespace
{

std::filesystem::path CreateTempDir(const std::string& Prefix)
{
    const auto Now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::filesystem::path TempDir = std::filesystem::temp_directory_path() / (Prefix + "_" + std::to_string(Now));
    std::filesystem::create_directories(TempDir);
    return TempDir;
}

bool WriteFile(const std::filesystem::path& Path, const std::string& Content)
{
    std::filesystem::create_directories(Path.parent_path());
    std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
    if (!Stream.is_open()) return false;
    Stream.write(Content.data(), static_cast<std::streamsize>(Content.size()));
    return Stream.good();
}

std::string ReadFile(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream.is_open()) return "";
    return std::string(
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>());
}

void SetupTestPackage(const std::filesystem::path& PackageDir, const std::string& ItemsContent)
{
    // package.json5
    std::string PackageJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  package_id: 'core',\n"
        "  namespace: 'core',\n"
        "  version: '1.0.0',\n"
        "  dependencies: []\n"
        "}\n";
    WriteFile(PackageDir / "package.json5", PackageJson5);

    // schemas/item.schema.json5
    std::string SchemaJson5 =
        "{\n"
        "  schema_version: 1,\n"
        "  id: 'core:schema.definition.item.v1',\n"
        "  definition_type: 'item',\n"
        "  fields: {\n"
        "    name: { type: 'string', required: true },\n"
        "    weight: { type: 'number', required: false, default: 1.0 },\n"
        "    value: { type: 'integer', required: false, default: 0 }\n"
        "  }\n"
        "}\n";
    WriteFile(PackageDir / "schemas/item.schema.json5", SchemaJson5);

    // definitions/items.json5
    WriteFile(PackageDir / "definitions/items.json5", ItemsContent);
}

bool TestBatchSetAtomicFailureLeavesFileUntouched()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_test_atomic");
    std::string InitialItems =
        "// Header comment\n"
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      // Item comment\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestPackage(TempDir, InitialItems);
    std::filesystem::path ItemsFile = TempDir / "definitions/items.json5";

    FBatchSetFieldsParams Params;
    Params.PackageRoot = TempDir;
    Params.DefinitionId = "core:item.sword";
    Params.Changes = {
        { "/data/name", GV2ContentCore::FValue("Steel Sword") },
        { "/data/weight", GV2ContentCore::FValue(3.5) },
        { "/data/non_existent_pointer/invalid", GV2ContentCore::FValue(100) } // This must fail
    };

    FAuthoringResult Result = FAuthoringService::BatchSetFields(Params);
    if (Result.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false; // Expected failure!
    }

    // Verify file on disk is byte-for-byte identical to InitialItems
    std::string ContentAfter = ReadFile(ItemsFile);
    std::filesystem::remove_all(TempDir);

    return ContentAfter == InitialItems;
}

bool TestDuplicateDefinitionAtomic()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_test_duplicate");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestPackage(TempDir, InitialItems);
    std::filesystem::path ItemsFile = TempDir / "definitions/items.json5";

    FDuplicateDefinitionParams DupParams;
    DupParams.PackageRoot = TempDir;
    DupParams.SourceDefinitionId = "core:item.sword";
    DupParams.TargetDefinitionId = "core:item.sword_copy";

    FAuthoringResult Result = FAuthoringService::DuplicateDefinition(DupParams);
    if (!Result.IsSuccess())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::string ContentAfter = ReadFile(ItemsFile);
    if (ContentAfter.find("core:item.sword_copy") == std::string::npos ||
        ContentAfter.find("Iron Sword") == std::string::npos)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Try duplicating again with duplicate target id -> must fail
    FAuthoringResult Dup2 = FAuthoringService::DuplicateDefinition(DupParams);
    if (Dup2.IsSuccess() || Dup2.Status != EAuthoringStatus::DuplicateDefinitionId)
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    std::filesystem::remove_all(TempDir);
    return true;
}

bool TestStaleFileStateDetection()
{
    std::filesystem::path TempDir = CreateTempDir("gv2_test_stale");
    std::string InitialItems =
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Iron Sword',\n"
        "        weight: 2.5,\n"
        "        value: 10\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    SetupTestPackage(TempDir, InitialItems);
    std::filesystem::path ItemsFile = TempDir / "definitions/items.json5";

    // 1. Get initial stamp
    FFileStateStamp Stamp = FAuthoringService::GetFileStateStamp(ItemsFile);
    if (Stamp.IsEmpty())
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // 2. Modify file externally
    std::string ExternalModified =
        "{\n"
        "  schema_version: 1,\n"
        "  definition_type: 'item',\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.sword',\n"
        "      data: {\n"
        "        name: 'Externally Modified Sword',\n"
        "        weight: 5.0,\n"
        "        value: 50\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";
    WriteFile(ItemsFile, ExternalModified);

    // 3. Attempt write with old stamp
    FSetFieldParams Params;
    Params.PackageRoot = TempDir;
    Params.DefinitionId = "core:item.sword";
    Params.JsonPointer = "/data/name";
    Params.NewValue = GV2ContentCore::FValue("Stale Write");
    Params.ExpectedStamp = Stamp;

    FAuthoringResult Result = FAuthoringService::SetField(Params);
    if (Result.IsSuccess() || Result.Status != EAuthoringStatus::StaleFileState || Result.ErrorCode != "stale_file_state")
    {
        std::filesystem::remove_all(TempDir);
        return false;
    }

    // Verify file content is unchanged (still contains ExternalModified)
    std::string ContentAfter = ReadFile(ItemsFile);
    std::filesystem::remove_all(TempDir);

    return ContentAfter == ExternalModified;
}

bool TestCommentAndFormattingPreservation()
{
    std::string Initial =
        "// Leading top-level comment\n"
        "{\n"
        "  // Schema version comment\n"
        "  schema_version: 1,\n"
        "  /* Multi-line\n"
        "     definitions comment */\n"
        "  definitions: [\n"
        "    {\n"
        "      id: 'core:item.apple',\n"
        "      data: {\n"
        "        // Apple name comment\n"
        "        name: 'Red Apple',\n"
        "        price: 5 // inline price comment\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    auto Res = SetFieldValue(Initial, "/definitions/0/data/price", GV2ContentCore::FValue(static_cast<std::int64_t>(10)));
    if (Res.Status != ESetFieldValueStatus::Success) return false;

    if (Res.UpdatedContent.find("// Leading top-level comment") == std::string::npos) return false;
    if (Res.UpdatedContent.find("// Schema version comment") == std::string::npos) return false;
    if (Res.UpdatedContent.find("/* Multi-line") == std::string::npos) return false;
    if (Res.UpdatedContent.find("// Apple name comment") == std::string::npos) return false;
    if (Res.UpdatedContent.find("// inline price comment") == std::string::npos) return false;
    if (Res.UpdatedContent.find("price: 10") == std::string::npos) return false;

    return true;
}

} // namespace

std::string RunAuthoringLibraryConformance()
{
    if (!TestBatchSetAtomicFailureLeavesFileUntouched())
    {
        return "TestBatchSetAtomicFailureLeavesFileUntouched failed";
    }
    if (!TestDuplicateDefinitionAtomic())
    {
        return "TestDuplicateDefinitionAtomic failed";
    }
    if (!TestStaleFileStateDetection())
    {
        return "TestStaleFileStateDetection failed";
    }
    if (!TestCommentAndFormattingPreservation())
    {
        return "TestCommentAndFormattingPreservation failed";
    }
    return "";
}

} // namespace GV2ContentAuthoring::Testing
