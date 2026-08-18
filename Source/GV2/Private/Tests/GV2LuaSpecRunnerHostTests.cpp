#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "GV2RuntimeCore/GV2HostServices.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2TestSupport/CommandValidatorFixture.h"
#include "GV2TestSupport/LuaSpecRunner.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <chrono>
#include <filesystem>

// TAS-04: one UE automation test represents the whole Lua spec runner, not
// one test per spec. Adding a spec under Tests/Lua/world/ is covered by
// this single test without any new C++ — the same generic runner headless
// calls from `--self-test` (Headless/Source/main.cpp). TAS-13: scoped to
// Tests/Lua/world specifically, since it is the only subtree that needs
// nothing more than the real production session — see
// GV2.Runtime.Lua.CommandValidatorSpecRunnerHost below for the isolated
// fixture session Tests/Lua/commands/ needs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaSpecRunnerHostTest,
    "GV2.Runtime.Lua.SpecRunnerHost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaSpecRunnerHostTest::RunTest(const FString& Parameters)
{
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const FString ScriptsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    const FString TestsLuaRootFString = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Lua"));
    const std::filesystem::path TestsLuaRoot(TCHAR_TO_UTF8(*TestsLuaRootFString));

    // TSL-15: Validate that all Tests/Lua subtrees have an assigned tier
    std::vector<std::string> UnregisteredSubtrees;
    if (!GV2TestSupport::ValidateAllSubtreesRegistered(TestsLuaRoot, UnregisteredSubtrees))
    {
        for (const std::string& Name : UnregisteredSubtrees)
        {
            AddError(FString::Printf(TEXT("Unregistered Lua spec subtree: %s"), UTF8_TO_TCHAR(Name.c_str())));
        }
        return false;
    }

    const std::vector<GV2TestSupport::ELuaSpecTier> StandardTiers = {
        GV2TestSupport::ELuaSpecTier::Core,
        GV2TestSupport::ELuaSpecTier::TextSystem,
        GV2TestSupport::ELuaSpecTier::FullGame,
    };

    for (const auto Tier : StandardTiers)
    {
        const std::vector<std::string> Subtrees = GV2TestSupport::GetSubtreesForTier(Tier);
        if (Subtrees.empty())
        {
            continue;
        }

        const std::vector<std::string> PackageNames = GV2TestSupport::GetPackageNamesForTier(Tier);
        TArray<FString> PackageRoots;
        for (const std::string& PkgName : PackageNames)
        {
            PackageRoots.Add(FPaths::Combine(GameDataDir, UTF8_TO_TCHAR(PkgName.c_str())));
        }

        const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectories(PackageRoots);
        if (!TestTrue(
                FString::Printf(TEXT("Build repository for tier %d"), static_cast<int>(Tier)),
                RepoBuild.IsSuccess()))
        {
            return false;
        }
        const GV2ContentCore::FRepositoryReadHandle RepoHandle = RepoBuild.GetCandidate().GetReadHandle();

        std::vector<GV2RuntimeCore::FRuntimeSource> TierRuntimeSources;
        FString NormScriptsDir = ScriptsDir;
        FPaths::NormalizeDirectoryName(NormScriptsDir);
        const FString ScriptsPrefix = NormScriptsDir + TEXT("/");
        TArray<FString> SourceFiles;
        IFileManager::Get().FindFilesRecursive(SourceFiles, *NormScriptsDir, TEXT("*.lua"), true, false, false);
        SourceFiles.Sort();
        for (const FString& FullPath : SourceFiles)
        {
            FString Text;
            if (!FFileHelper::LoadFileToString(Text, *FullPath))
            {
                TestTrue(TEXT("Load lua script"), false);
                return false;
            }
            FString Norm = FullPath;
            FPaths::NormalizeFilename(Norm);
            const FString Rel = Norm.RightChop(ScriptsPrefix.Len());
            const FTCHARToUTF8 Utf8(*Text);
            TierRuntimeSources.push_back({
                "@core/" + std::string(TCHAR_TO_UTF8(*Rel)),
                std::string(Utf8.Get(), Utf8.Length())});
        }

        for (const FString& PkgRoot : PackageRoots)
        {
            const FString PkgName = FPaths::GetCleanFilename(PkgRoot);
            if (PkgName == TEXT("core"))
            {
                continue;
            }
            FString PkgScriptsDir = FPaths::Combine(PkgRoot, TEXT("scripts"));
            FPaths::NormalizeDirectoryName(PkgScriptsDir);
            if (!FPaths::DirectoryExists(PkgScriptsDir))
            {
                PkgScriptsDir = FPaths::Combine(PkgRoot, TEXT("Scripts"));
                FPaths::NormalizeDirectoryName(PkgScriptsDir);
                if (!FPaths::DirectoryExists(PkgScriptsDir))
                {
                    continue;
                }
            }
            const FString PkgScriptsPrefix = PkgScriptsDir + TEXT("/");
            TArray<FString> PkgSourceFiles;
            IFileManager::Get().FindFilesRecursive(PkgSourceFiles, *PkgScriptsDir, TEXT("*.lua"), true, false, false);
            PkgSourceFiles.Sort();
            for (const FString& FullPath : PkgSourceFiles)
            {
                FString Text;
                if (!FFileHelper::LoadFileToString(Text, *FullPath))
                {
                    TestTrue(TEXT("Load lua script"), false);
                    return false;
                }
                FString Norm = FullPath;
                FPaths::NormalizeFilename(Norm);
                if (!Norm.StartsWith(PkgScriptsPrefix, ESearchCase::CaseSensitive))
                {
                    continue;
                }
                const FString Rel = Norm.RightChop(PkgScriptsPrefix.Len());
                const FTCHARToUTF8 Utf8(*Text);
                TierRuntimeSources.push_back({
                    "@" + std::string(TCHAR_TO_UTF8(*PkgName)) + "/" + std::string(TCHAR_TO_UTF8(*Rel)),
                    std::string(Utf8.Get(), Utf8.Length())});
            }
        }

        for (const std::string& Subtree : Subtrees)
        {
            const std::filesystem::path SpecRoot = TestsLuaRoot / Subtree;
            std::error_code SpecEc;
            if (!std::filesystem::is_directory(SpecRoot, SpecEc) || SpecEc)
            {
                continue;
            }

            GV2RuntimeCore::FRuntimeSession Session;
            GV2RuntimeCore::FRuntimeFault Fault;
            const bool bStarted = Session.Start(1, RepoHandle, TierRuntimeSources, Fault);
            if (!TestTrue(
                    FString::Printf(TEXT("Runtime session starts for %s: code=%s message=%s"),
                        UTF8_TO_TCHAR(Subtree.c_str()), UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str())),
                    bStarted))
            {
                return false;
            }

            const std::filesystem::path SaveSlotSpecRoot = std::filesystem::temp_directory_path()
                / ("gv2_ue_save_spec_slots_"
                    + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            GV2RuntimeCore::FFilesystemSaveSlotStorage SaveSlotSpecStorage(SaveSlotSpecRoot);
            Session.SetSaveSlotStorage(&SaveSlotSpecStorage);

            GV2TestSupport::FLuaSpecRunResult SpecResult;
            const bool bAllPassed = GV2TestSupport::RunLuaSpecs(SpecRoot, Session, SpecResult);
            Session.Stop();
            {
                std::error_code RemoveEc;
                std::filesystem::remove_all(SaveSlotSpecRoot, RemoveEc);
            }
            if (!bAllPassed)
            {
                for (const GV2ContentHostSupport::FLuaSpecFailure& Failure : SpecResult.Failures)
                {
                    AddError(FString::Printf(
                        TEXT("Lua spec failed: id=%s code=%s message=%s"),
                        UTF8_TO_TCHAR(Failure.Identifier.c_str()),
                        UTF8_TO_TCHAR(Failure.Code.c_str()),
                        UTF8_TO_TCHAR(Failure.Message.c_str())));
                }
                return false;
            }
        }
    }
    return true;
}

// TAS-13: one UE automation test for the whole Tests/Lua/commands/ subtree,
// running on the isolated CommandValidatorSpecs fixture session (test-scoped
// validators registered before the registry freezes) instead of the real
// production session, whose validator registry is already frozen and empty
// by the time any spec runs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CommandValidatorSpecRunnerHostTest,
    "GV2.Runtime.Lua.CommandValidatorSpecRunnerHost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CommandValidatorSpecRunnerHostTest::RunTest(const FString& Parameters)
{
    const FString ScriptsRootFString = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    const FString FixtureRootFString = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Fixtures/CommandValidatorSpecs"));
    const FString SpecRootFString = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Lua/commands"));

    GV2RuntimeCore::FRuntimeSession Session;
    GV2RuntimeCore::FRuntimeFault Fault;
    const bool bStarted = GV2TestSupport::StartCommandValidatorFixtureSession(
        std::filesystem::path(TCHAR_TO_UTF8(*ScriptsRootFString)),
        std::filesystem::path(TCHAR_TO_UTF8(*FixtureRootFString)),
        Session,
        Fault);
    if (!TestTrue(
            FString::Printf(TEXT("Command validator fixture session starts: code=%s message=%s"),
                UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str())),
            bStarted))
    {
        return false;
    }

    const std::filesystem::path SpecRoot(TCHAR_TO_UTF8(*SpecRootFString));
    GV2TestSupport::FLuaSpecRunResult SpecResult;
    const bool bAllPassed = GV2TestSupport::RunLuaSpecs(SpecRoot, Session, SpecResult);
    Session.Stop();

    if (!bAllPassed)
    {
        for (const GV2ContentHostSupport::FLuaSpecFailure& Failure : SpecResult.Failures)
        {
            AddError(FString::Printf(
                TEXT("Lua spec failed: id=%s code=%s message=%s"),
                UTF8_TO_TCHAR(Failure.Identifier.c_str()),
                UTF8_TO_TCHAR(Failure.Code.c_str()),
                UTF8_TO_TCHAR(Failure.Message.c_str())));
        }
        return false;
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
