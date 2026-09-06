#if WITH_DEV_AUTOMATION_TESTS

#include "Application/GV2PackageClosure.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"

// DCA-21 (PackageBoundary M5): `core`-level Source/GV2 code must not name a higher
// package's content (ADR-0035 §5) -- the set of higher packages comes from the pinned
// closure (GameData/mods.lock.json5 via GV2PackageClosure, the same source DCA-18/19
// already read), not a hand-written pair like the pre-existing CORE_DECOUPLING_RULE
// gate's `(rh|sample)` regex (Tools/Content/validate_core_decoupling.py), which does not
// know about `textsystem` at all and therefore never caught
// GV2UiCapabilityObservability.cpp's hardcoded `textsystem:resource.ui.missing_*` probe
// candidates. That gate's own scope (Scripts/, GameData/core/, all of Source/ including
// tests, ids prefixed by "rh" or "sample" only) is left untouched -- DCA-21's own text
// puts test files out of scope (that pair is already governed there separately).
// This gate instead covers Source/GV2 production .cpp only, every non-core package_id
// from the closure, and both a `<pkg>:`-prefixed id and a bare `<pkg>` name literal.
namespace
{
struct FGV2PackageBoundaryException
{
    const TCHAR* FileNameSubstring;
    const TCHAR* Reason;
};

const TArray<FGV2PackageBoundaryException>& GetPackageBoundaryExceptions()
{
    static const TArray<FGV2PackageBoundaryException> Exceptions = {
        {
            TEXT("GV2ScreenRegistry.cpp"),
            TEXT("FindOwningPackageForAssetPath's content-root-to-package naming convention "
                 "cannot be derived from mods.lock.json5, which has no notion of /Game/ paths (DCA-18).")
        },
        {
            TEXT("GV2RuntimeSubsystem.cpp"),
            TEXT("Test-only fixture branch, compiled only under WITH_DEV_AUTOMATION_TESTS, that "
                 "opts a test session into core+textsystem+sample instead of the default "
                 "core+textsystem+rh (both packages bind the same textsystem: travel action).")
        },
        {
            TEXT("GV2SessionCoordinator.cpp"),
            TEXT("Sibling test-only fixture branch to GV2RuntimeSubsystem.cpp's, same reason.")
        },
    };
    return Exceptions;
}

FString FindExceptionReason(const FString& FileName)
{
    for (const FGV2PackageBoundaryException& Exception : GetPackageBoundaryExceptions())
    {
        if (FileName.Contains(Exception.FileNameSubstring))
        {
            return Exception.Reason;
        }
    }
    return FString();
}

// Crude but sufficient for this codebase's style: production Source/GV2 never puts "//"
// inside a string literal, so truncating at the first "//" reliably drops line comments
// (including ones that happen to quote a package-prefixed id as documentation) without
// touching real code.
FString StripLineComment(const FString& Line)
{
    int32 CommentIndex = INDEX_NONE;
    if (Line.FindChar(TEXT('/'), CommentIndex))
    {
        const int32 SlashSlashIndex = Line.Find(TEXT("//"));
        if (SlashSlashIndex != INDEX_NONE)
        {
            return Line.Left(SlashSlashIndex);
        }
    }
    return Line;
}

bool LineNamesPackageLiterally(const FString& CodeOnlyLine, const FString& PackageId)
{
    // Matches the package id as a whole word inside a quoted string -- covers both a
    // `<pkg>:something` prefixed id and a bare `<pkg>` name used as a literal (a
    // directory component like "/game/<pkg>/" also satisfies the word-boundary match).
    const FString Pattern = FString::Printf(TEXT("\"[^\"]*\\b%s\\b[^\"]*\""), *PackageId);
    FRegexPattern CompiledPattern(Pattern, ERegexPatternFlags::CaseInsensitive);
    FRegexMatcher Matcher(CompiledPattern, CodeOnlyLine);
    return Matcher.FindNext();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PackageNameLiteralGateTest,
    "GV2.Runtime.UIKit.PackageNameLiteralGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PackageNameLiteralGateTest::RunTest(const FString& Parameters)
{
    TArray<FString> NonCorePackages;
    for (const GV2PackageClosure::FEntry& Entry : GV2PackageClosure::DiscoverFromGameData())
    {
        if (!Entry.PackageId.Equals(TEXT("core"), ESearchCase::IgnoreCase))
        {
            NonCorePackages.Add(Entry.PackageId);
        }
    }
    TestTrue(TEXT("Pinned closure has at least one non-core package to guard against"), NonCorePackages.Num() > 0);

    const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GV2"));
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceRoot, TEXT("*.cpp"), true, false, false);

    int32 ScannedFileCount = 0;
    TSet<FString> ExceptionsHit;
    for (const FString& FilePath : SourceFiles)
    {
        const FString NormalizedPath = FilePath.Replace(TEXT("\\"), TEXT("/"));
        if (NormalizedPath.Contains(TEXT("/Tests/")))
        {
            continue;
        }
        ++ScannedFileCount;

        const FString FileName = FPaths::GetCleanFilename(FilePath);
        const FString ExceptionReason = FindExceptionReason(FileName);

        FString FileContent;
        if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
        {
            continue;
        }
        TArray<FString> Lines;
        FileContent.ParseIntoArrayLines(Lines, false);
        for (int32 LineIdx = 0; LineIdx < Lines.Num(); ++LineIdx)
        {
            const FString CodeOnlyLine = StripLineComment(Lines[LineIdx]);
            for (const FString& Package : NonCorePackages)
            {
                if (!LineNamesPackageLiterally(CodeOnlyLine, Package))
                {
                    continue;
                }
                if (ExceptionReason.IsEmpty())
                {
                    AddError(FString::Printf(
                        TEXT("Source/GV2 production file names higher package '%s' literally: %s:%d"),
                        *Package, *FileName, LineIdx + 1));
                }
                else
                {
                    ExceptionsHit.Add(FileName);
                }
            }
        }
    }

    TestTrue(TEXT("At least one production .cpp file was scanned"), ScannedFileCount > 0);
    TestEqual(
        TEXT("Every declared exception was actually exercised by the scan"),
        ExceptionsHit.Num(),
        GetPackageBoundaryExceptions().Num());

    return true;
}

#endif
