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
    // 1. Build the real GameData repository (core and rh packages).
    TArray<FString> PackageRoots;
    PackageRoots.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core")));
    const FString RhRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/rh"));
    if (FPaths::DirectoryExists(RhRoot))
    {
        PackageRoots.Add(RhRoot);
    }
    const GV2ContentCore::FBuildResult RepoBuild = BuildGV2RepositoryFromDirectories(PackageRoots);
    if (!TestTrue(TEXT("UE host builds GameData packages repository"), RepoBuild.IsSuccess()))
    {
        return false;
    }
    const GV2ContentCore::FRepositoryReadHandle RepoHandle = RepoBuild.GetCandidate().GetReadHandle();

    // 2. Load the real Scripts/ module tree.
    std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources;
    FString ScriptsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    FPaths::NormalizeDirectoryName(ScriptsDir);
    const FString ScriptsPrefix = ScriptsDir + TEXT("/");
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(SourceFiles, *ScriptsDir, TEXT("*.lua"), true, false, false);
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
        RuntimeSources.push_back({
            "@core/" + std::string(TCHAR_TO_UTF8(*Rel)),
            std::string(Utf8.Get(), Utf8.Length())});
    }

    // 3. Run every subtree against its own fresh production session — the
    // same generic runner gv2-headless --self-test calls (Headless/Source/
    // main.cpp starts one FRuntimeSession per subtree, not a single shared
    // one). This matters beyond isolation hygiene: Tests/Lua/events/*.lua
    // specs call event_bus.clear_subscribers() as a setup/teardown helper,
    // which resets the subscriber registry's frozen flag
    // (subscriber_registry.lua's registry.clear()). Sharing one session
    // across subtrees let that leak into Tests/Lua/lifecycle/, which then
    // observed an unfrozen registry outside of any registration window —
    // a test-harness divergence from headless, not a product bug. A
    // missing directory is not an error (TAS-02). Only subtrees needing the
    // real production session run here (TAS-13: Tests/Lua/commands needs an
    // isolated fixture session instead, tested by
    // GV2.Runtime.Lua.CommandValidatorSpecRunnerHost).
    for (const TCHAR* Subtree : { TEXT("Tests/Lua/world"), TEXT("Tests/Lua/events"), TEXT("Tests/Lua/resources"), TEXT("Tests/Lua/lifecycle"), TEXT("Tests/Lua/save") })
    {
        const FString SpecRootFString = FPaths::Combine(FPaths::ProjectDir(), Subtree);
        const std::filesystem::path SpecRoot(TCHAR_TO_UTF8(*SpecRootFString));

        GV2RuntimeCore::FRuntimeSession Session;
        GV2RuntimeCore::FRuntimeFault Fault;
        const bool bStarted = Session.Start(1, RepoHandle, RuntimeSources, Fault);
        if (!TestTrue(
                FString::Printf(TEXT("Runtime session starts for %s: code=%s message=%s"),
                    Subtree, UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str())),
                bStarted))
        {
            return false;
        }

        // SAV-10/11: Tests/Lua/save/ specs exercise game.save_slots.write
        // through a real (throwaway, temp-dir-rooted) storage backing, not
        // a mock. Harmless to wire for every subtree since only save/
        // specs call it.
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
