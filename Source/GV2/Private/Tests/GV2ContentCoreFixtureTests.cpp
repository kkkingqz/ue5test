#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/Testing/Json5Conformance.h"
#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PortableContentCoreSharedFixtures,
    "GV2.Runtime.ContentCore.SharedFixtureCorpus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PortableContentCoreSharedFixtures::RunTest(const FString& Parameters)
{
    const FString FixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Tests/Fixtures/PortableContentCore")));
    const FString IndexPath = FPaths::Combine(FixtureRoot, TEXT("fixtures.index"));
    FString IndexText;
    if (!TestTrue(
            TEXT("UBT automation can read the shared PCC fixture index"),
            FFileHelper::LoadFileToString(IndexText, *IndexPath)))
    {
        return false;
    }

    TArray<FString> IndexedPaths;
    IndexText.ParseIntoArrayLines(IndexedPaths, true);
    IndexedPaths.RemoveAll(
        [](FString& Line)
        {
            Line.TrimStartAndEndInline();
            return Line.IsEmpty() || Line.StartsWith(TEXT("#"));
        });

    TArray<FString> SortedPaths = IndexedPaths;
    SortedPaths.Sort();
    TestEqual(
        TEXT("Shared PCC fixture index is deterministically sorted"),
        FString::Join(IndexedPaths, TEXT("\n")),
        FString::Join(SortedPaths, TEXT("\n")));

    TSet<FString> IndexedSet;
    bool bHasCore = false;
    bool bHasTestMod = false;
    bool bHasInvalid = false;
    std::vector<GV2ContentCore::Testing::FJson5Fixture> ParserFixtures;
    for (const FString& RelativePath : IndexedPaths)
    {
        TestTrue(
            *FString::Printf(TEXT("Fixture path is project-relative: %s"), *RelativePath),
            FPaths::IsRelative(RelativePath) && !RelativePath.Contains(TEXT("..")));
        TestTrue(
            *FString::Printf(TEXT("Fixture path is unique: %s"), *RelativePath),
            !IndexedSet.Contains(RelativePath));
        IndexedSet.Add(RelativePath);

        const FString FullPath = FPaths::Combine(FixtureRoot, RelativePath);
        TArray<uint8> FileBytes;
        TestTrue(
            *FString::Printf(TEXT("UBT automation can read shared fixture: %s"), *RelativePath),
            FFileHelper::LoadFileToArray(FileBytes, *FullPath));
        TestFalse(
            *FString::Printf(TEXT("Shared fixture is non-empty: %s"), *RelativePath),
            FileBytes.Num() == 0);

        GV2ContentCore::Testing::FJson5Fixture ParserFixture;
        ParserFixture.RelativePath = TCHAR_TO_UTF8(*RelativePath);
        ParserFixture.Source.assign(
            reinterpret_cast<const char*>(FileBytes.GetData()), FileBytes.Num());
        if (RelativePath.StartsWith(TEXT("invalid/duplicate_key/")))
        {
            ParserFixture.ExpectedDiagnosticCode = "core:diagnostic.json5.duplicate_key";
        }
        ParserFixtures.push_back(std::move(ParserFixture));

        bHasCore |= RelativePath.StartsWith(TEXT("valid/core/"));
        bHasTestMod |= RelativePath.StartsWith(TEXT("valid/test_mod/"));
        bHasInvalid |= RelativePath.StartsWith(TEXT("invalid/"));
    }

    TArray<FString> DiscoveredFiles;
    IFileManager::Get().FindFilesRecursive(
        DiscoveredFiles,
        *FixtureRoot,
        TEXT("*.json5"),
        true,
        false,
        false);
    TestEqual(
        TEXT("Every shared JSON5 fixture is listed exactly once"),
        DiscoveredFiles.Num(),
        IndexedSet.Num());
    for (FString FullPath : DiscoveredFiles)
    {
        FPaths::MakePathRelativeTo(FullPath, *(FixtureRoot / TEXT("")));
        FPaths::NormalizeFilename(FullPath);
        TestTrue(
            *FString::Printf(TEXT("Discovered fixture is present in canonical index: %s"), *FullPath),
            IndexedSet.Contains(FullPath));
    }

    TestTrue(TEXT("Shared corpus contains the representative core package"), bHasCore);
    TestTrue(TEXT("Shared corpus contains the representative test mod"), bHasTestMod);
    TestTrue(TEXT("Shared corpus contains invalid inputs"), bHasInvalid);
    TestTrue(
        TEXT("Shared corpus contains the empty core descriptor fixture"),
        IFileManager::Get().DirectoryExists(*FPaths::Combine(FixtureRoot, TEXT("valid/empty_core"))));

    std::string ConformanceFailure;
    const bool bConformancePassed = GV2ContentCore::Testing::RunJson5FixtureConformance(
        ParserFixtures,
        ConformanceFailure);
    TestTrue(
        *FString::Printf(TEXT("Shared parser conformance: %s"), UTF8_TO_TCHAR(ConformanceFailure.c_str())),
        bConformancePassed);
    return true;
}

#endif
