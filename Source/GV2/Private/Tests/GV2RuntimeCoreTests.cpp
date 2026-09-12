#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2RepositoryPublisher.h"
#include "Application/GV2SessionCoordinator.h"
#include "Application/GV2SessionContentSnapshot.h"
#include "Application/GV2ScreenFieldMaterializer.h"
#include "UI/GV2UiSchemaCache.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"
#include "Tests/GV2ForgeryTestWidgets.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "UI/GV2CentralStylePreparer.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2PresentationAuthorityProbe.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiTheme.h"
#include "Bridge/GV2UiBindingRegistry.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"
#include "GV2RuntimeCore/Testing/GV2LuaMarshallerConformance.h"
#include "GV2RuntimeCore/Testing/GV2LuaRepositoryConformance.h"
#include "GV2RuntimeCore/Testing/GV2ValidatorRegistryConformance.h"
#include "GV2RuntimeCore/Testing/GV2LuaSpecRunnerConformance.h"
#include "GV2RuntimeCore/Testing/GV2SaveSlotStorageConformance.h"
#include "GV2RuntimeCore/Testing/GV2ColdStartLoadConformance.h"
#include "GV2ContentHostSupport/Testing/PackageDiscoveryAndOrderConformance.h"
#include "GV2ContentHostSupport/Testing/PackageManifestConformance.h"
#include "GV2ContentCore/Testing/RepresentativeCore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <algorithm>

namespace
{
FGV2UiBindingDefinition MakeBindingDefinition(
    const FString& NodeKey,
    const FString& CommandId)
{
    FGV2UiBindingDefinition Definition;
    Definition.NodeKeyPath = {TEXT("route"), NodeKey};
    Definition.ElementId = NodeKey;
    Definition.CommandId = CommandId;
    return Definition;
}

std::vector<GV2RuntimeCore::FRuntimeSource> LoadTestRuntimeSources()
{
    std::vector<GV2RuntimeCore::FRuntimeSource> Sources;
    FString ScriptsDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts"));
    FPaths::NormalizeDirectoryName(ScriptsDirectory);
    const FString ScriptsPrefix = ScriptsDirectory + TEXT("/");
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(
        SourceFiles,
        *ScriptsDirectory,
        TEXT("*.lua"),
        true,
        false,
        false);
    SourceFiles.Sort();
    for (const FString& FullPath : SourceFiles)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *FullPath))
        {
            return {};
        }
        FString NormalizedFullPath = FullPath;
        FPaths::NormalizeFilename(NormalizedFullPath);
        if (!NormalizedFullPath.StartsWith(ScriptsPrefix, ESearchCase::CaseSensitive))
        {
            return {};
        }
        const FString RelativePath = NormalizedFullPath.RightChop(ScriptsPrefix.Len());
        const FTCHARToUTF8 Utf8(*Text);
        Sources.push_back({
            "@core/" + std::string(TCHAR_TO_UTF8(*RelativePath)),
            std::string(Utf8.Get(), Utf8.Length())});
    }
    return Sources;
}

// SafeEnvironmentAndProtectedEntry needs a "debug.start"-style command that
// publishes a generic 5-field screen and a command that raises a Lua error,
// to exercise the dispatcher/presentation/fault-handling round trip. That
// content used to live in Scripts/debug/start.lua before it moved into a
// non-core game data package during CoreBoundaryMigration; engine/core code
// (including this test — see Tools/Content/validate_core_decoupling.py)
// must not reference that package's namespace, so this test carries its own
// self-contained fixture module instead of loading it.
// Module graph discovery keys packages off the source name prefix
// ("@<pkg>/manifest.lua"), so this fixture uses its own synthetic
// "testfixture" package rather than declaring a module inside "core" itself
// (which would need a manifest.lua edit affecting every core consumer).
constexpr const char* TestFixtureManifestSource = R"(
return {
    modules = {
        {
            module_id = "testfixture:module.debug_start",
            source = "debug_start.lua",
            dependencies = {
                "core:module.presentation.screen_requests",
                "core:module.resources.text",
            },
        },
    },
}
)";

constexpr const char* TestFixtureDebugStartSource = R"(
local screens = require("core:module.presentation.screen_requests")
local text = require("core:module.resources.text")

local M = { id = "testfixture:module.debug_start" }

function M.register(_ctx)
    game.commands.handlers.register("testfixture:command.debug.start", function(_req)
        screens.publish(screens.create("testfixture:screen.test", {
            description = {
                schema_id = "core:schema.ui_field.rich_text.v3",
                value = { text = text.spec("testfixture:text.description", nil, "default"), spans = {} },
            },
            buttons = {
                schema_id = "core:schema.ui_field.button_list.v2",
                value = { items = {} },
            },
            class_select = {
                schema_id = "core:schema.ui_field.dropdown_select.v1",
                value = {
                    placeholder = text.spec("testfixture:text.player_name", nil, "default"),
                    selected_key = nil,
                    items = {},
                    binding = { command_id = "testfixture:command.noop", args = {} },
                },
            },
        }))
        return true
    end)

    game.commands.handlers.register("testfixture:command.force_error", function(_req)
        error("forced test runtime error")
    end)

    game.commands.handlers.register("testfixture:command.noop", function(_req)
        return true
    end)
end

return M
)";

std::vector<GV2RuntimeCore::FRuntimeSource> LoadTestRuntimeSourcesWithDebugStartFixture()
{
    std::vector<GV2RuntimeCore::FRuntimeSource> Sources = LoadTestRuntimeSources();
    Sources.push_back({"@testfixture/manifest.lua", TestFixtureManifestSource});
    Sources.push_back({"@testfixture/debug_start.lua", TestFixtureDebugStartSource});
    return Sources;
}

GV2ContentCore::FBuildResult MakeTestBuildResultFrom(const TCHAR* FixtureRelativePath)
{
    const FString PackageRoot = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore"), FixtureRelativePath);
    return BuildGV2RepositoryFromDirectory(PackageRoot);
}

GV2ContentCore::FRepositoryReadHandle RequirePinnedRepository(
    FAutomationTestBase& Test,
    const GV2ContentCore::FBuildResult& Result,
    const FString& Context)
{
    if (Result.IsFailure())
    {
        Test.AddError(FString::Printf(TEXT("%s repository build failed"), *Context));
        for (const GV2ContentCore::FDiagnostic& Diagnostic : Result.GetDiagnostics())
        {
            Test.AddError(FString::Printf(
                TEXT("[%s] %s (package=%s source=%s)"),
                UTF8_TO_TCHAR(Diagnostic.Code.c_str()),
                UTF8_TO_TCHAR(Diagnostic.Message.c_str()),
                Diagnostic.PackageId.has_value()
                    ? UTF8_TO_TCHAR(Diagnostic.PackageId->c_str()) : TEXT("<none>"),
                Diagnostic.RelativeSource.has_value()
                    ? UTF8_TO_TCHAR(Diagnostic.RelativeSource->c_str()) : TEXT("<none>")));
        }
        return GV2ContentCore::FRepositoryReadHandle();
    }
    return Result.GetCandidate().GetReadHandle();
}

GV2ContentCore::FRepositoryReadHandle MakeFrozenCoreFixturePinnedRepository(
    FAutomationTestBase& Test)
{
    const GV2ContentCore::FBuildResult Result = MakeTestBuildResultFrom(TEXT("valid/core"));
    return RequirePinnedRepository(Test, Result, TEXT("frozen valid/core fixture"));
}

class FTestMultiPackageSourceProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    std::map<std::string, FString> PackageRoots;

    std::optional<std::string> ReadSource(
        std::string_view RequestedPackageId,
        std::string_view RelativeSource) const override
    {
        auto Found = PackageRoots.find(std::string(RequestedPackageId));
        if (Found == PackageRoots.end()) return std::nullopt;
        const FString FullPath = FPaths::Combine(Found->second, UTF8_TO_TCHAR(std::string(RelativeSource).c_str()));
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *FullPath)) return std::nullopt;
        FTCHARToUTF8 Utf8(*Text);
        return std::string(Utf8.Get(), Utf8.Length());
    }
};

GV2ContentCore::FRepositoryReadHandle MakeModdedFixturePinnedRepository(
    FAutomationTestBase& Test)
{
    using namespace GV2ContentCore;
    const FString FixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore")));
    const FPackageDescriptor Core = Testing::MakeRepresentativeCorePackageDescriptor();
    const FPackageDescriptor TestMod = Testing::MakeRepresentativeTestModPackageDescriptor();

    FTestMultiPackageSourceProvider Provider;
    Provider.PackageRoots.emplace("core", FPaths::Combine(FixtureRoot, TEXT("valid/core")));
    Provider.PackageRoots.emplace("test_mod", FPaths::Combine(FixtureRoot, TEXT("valid/test_mod")));

    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult BuildResult = BuildRepository({ Core, TestMod }, Options);
    return RequirePinnedRepository(Test, BuildResult, TEXT("frozen core+test_mod fixture"));
}

// BuildRepository() requires a package literally named "core" at load_index 0
// (GameDataRepositoryContract.md); the shared "empty_core" fixture directory
// doesn't match that name, so build a distinct, minimal empty "core" package
// in-memory instead of going through the filesystem discovery convention.
GV2ContentCore::FBuildResult MakeEmptyCoreBuildResult()
{
    using namespace GV2ContentCore;
    const FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    const FBuildOptions Options;
    return BuildRepository({EmptyCore}, Options);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PortableRuntimeTest,
    "GV2.Runtime.Lua.SafeEnvironmentAndProtectedEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PortableRuntimeTest::RunTest(const FString& Parameters)
{
    GV2RuntimeCore::FRuntimeSession Host;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources = LoadTestRuntimeSourcesWithDebugStartFixture();
    TestTrue(TEXT("Manifest-driven Lua source tree is loadable"), RuntimeSources.size() >= 2);
    TestTrue(
        TEXT("Lua VM starts with the configured runtime sources"),
        Host.Start(23, MakeFrozenCoreFixturePinnedRepository(*this), RuntimeSources, Fault));
    TestTrue(TEXT("Lua VM reports started state"), Host.IsStarted());
    TestFalse(TEXT("Lua VM is idle after bootstrap"), Host.IsExecuting());
    if (!Host.IsStarted())
    {
        AddError(FString::Printf(
            TEXT("Lua start fault: %s: %s"),
            UTF8_TO_TCHAR(Fault.Code.c_str()),
            UTF8_TO_TCHAR(Fault.Message.c_str())));
        return false;
    }

    GV2RuntimeCore::FSemanticInput Item;
    Item.Sequence = 1;
    Item.SessionGeneration = 23;
    Item.UiInstanceId = "ui@23:1";
    Item.Revision = 1;
    Item.NodeKeyPath = {"route", "button"};
    Item.CommandId = "core:command.test.lua_round_trip";
    Item.Args.emplace("target_id", GV2RuntimeCore::FValue(std::string("core:item.test.target")));
    Item.Args.emplace("count", GV2RuntimeCore::FValue(std::int64_t{7}));

    TestTrue(
        TEXT("Fixed semantic input entry point accepts a value-only envelope"),
        Host.DispatchSemanticInput(Item, Fault));
    TestFalse(TEXT("Lua VM is idle after semantic input"), Host.IsExecuting());

    GV2RuntimeCore::FCommandRequest DirectRequest;
    DirectRequest.CommandId = "core:command.test.direct_round_trip";
    DirectRequest.Sequence = 2;
    DirectRequest.Args.emplace("count", GV2RuntimeCore::FValue(std::int64_t{8}));
    TestTrue(
        TEXT("Direct simulation ingress reaches the same fixed command dispatcher"),
        Host.DispatchCommand(DirectRequest, Fault));

    GV2RuntimeCore::FCommandRequest StartRequest;
    StartRequest.CommandId = "testfixture:command.debug.start";
    StartRequest.Sequence = 3;
    TestTrue(
        TEXT("Lua debug handler accepts the start command"),
        Host.DispatchCommand(StartRequest, Fault));

    std::optional<GV2RuntimeCore::FScreenRequest> PendingScreen;
    TestTrue(
        TEXT("Host copies the pending presentation after command dispatch"),
        Host.TakePendingScreen(PendingScreen, Fault));
    TestTrue(TEXT("Start publishes a Screen request"), PendingScreen.has_value());
    if (PendingScreen)
    {
        TestEqual(
            TEXT("Lua publishes generic Screen Fields"),
            static_cast<int32>(PendingScreen->Fields.size()),
            3);
        const GV2RuntimeCore::FScreenField* DescriptionField = nullptr;
        const GV2RuntimeCore::FScreenField* ButtonsField = nullptr;
        const GV2RuntimeCore::FScreenField* DropdownField = nullptr;
        for (const GV2RuntimeCore::FScreenField& Field : PendingScreen->Fields)
        {
            if (Field.FieldId == "description") DescriptionField = &Field;
            if (Field.FieldId == "buttons") ButtonsField = &Field;
            if (Field.FieldId == "class_select") DropdownField = &Field;
        }
        TestNotNull(TEXT("Generic request contains description field"), DescriptionField);
        TestNotNull(TEXT("Generic request contains buttons field"), ButtonsField);
        TestNotNull(TEXT("Generic request contains dropdown field"), DropdownField);
        TestEqual(
            TEXT("Description field keeps its schema identity"),
            FString(UTF8_TO_TCHAR(DescriptionField != nullptr ? DescriptionField->SchemaId.c_str() : "")),
            FString(TEXT("core:schema.ui_field.rich_text.v3")));
        TestEqual(
            TEXT("Buttons field keeps its schema identity"),
            FString(UTF8_TO_TCHAR(ButtonsField != nullptr ? ButtonsField->SchemaId.c_str() : "")),
            FString(TEXT("core:schema.ui_field.button_list.v2")));
        TestEqual(
            TEXT("Dropdown field keeps its schema identity"),
            FString(UTF8_TO_TCHAR(DropdownField != nullptr ? DropdownField->SchemaId.c_str() : "")),
            FString(TEXT("core:schema.ui_field.dropdown_select.v1")));

        // PCC-47: Verify definition read from repository reaches Screen Field
        if (ButtonsField != nullptr && std::holds_alternative<GV2RuntimeCore::FValue::FObject>(ButtonsField->Value.Data))
        {
            const auto& ButtonsObj = std::get<GV2RuntimeCore::FValue::FObject>(ButtonsField->Value.Data);
            auto ItemsIt = ButtonsObj.find("items");
            if (ItemsIt != ButtonsObj.end() && std::holds_alternative<GV2RuntimeCore::FValue::FArray>(ItemsIt->second.Data))
            {
                const auto& ItemsArray = std::get<GV2RuntimeCore::FValue::FArray>(ItemsIt->second.Data);
                if (!ItemsArray.empty() && std::holds_alternative<GV2RuntimeCore::FValue::FObject>(ItemsArray[0].Data))
                {
                    const auto& FirstBtnObj = std::get<GV2RuntimeCore::FValue::FObject>(ItemsArray[0].Data);
                    auto BindingIt = FirstBtnObj.find("binding");
                    if (BindingIt != FirstBtnObj.end() && std::holds_alternative<GV2RuntimeCore::FValue::FObject>(BindingIt->second.Data))
                    {
                        const auto& BindingObj = std::get<GV2RuntimeCore::FValue::FObject>(BindingIt->second.Data);
                        auto ArgsIt = BindingObj.find("args");
                        if (ArgsIt != BindingObj.end() && std::holds_alternative<GV2RuntimeCore::FValue::FObject>(ArgsIt->second.Data))
                        {
                            const auto& ArgsObj = std::get<GV2RuntimeCore::FValue::FObject>(ArgsIt->second.Data);
                            auto TargetIt = ArgsObj.find("target");
                            if (TargetIt != ArgsObj.end() && std::holds_alternative<std::string>(TargetIt->second.Data))
                            {
                                TestEqual(
                                    TEXT("First button binding target arg was read from repository definition"),
                                    FString(UTF8_TO_TCHAR(std::get<std::string>(TargetIt->second.Data).c_str())),
                                    FString(TEXT("testfixture:item.placeholder")));
                            }
                        }
                    }
                }
            }
        }
    }
    TestTrue(
        TEXT("Pending presentation is consumed exactly once"),
        Host.TakePendingScreen(PendingScreen, Fault));
    TestFalse(TEXT("No presentation remains after consumption"), PendingScreen.has_value());

    Item.CommandId = "testfixture:command.force_error";
    Item.Sequence = 4;
    TestFalse(
        TEXT("Lua runtime error is returned as a structured fault"),
        Host.DispatchSemanticInput(Item, Fault));
    TestEqual(
        TEXT("Runtime fault has stable code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaDispatchError")));
    TestFalse(TEXT("Lua VM is idle after failed protected call"), Host.IsExecuting());

    Item.CommandId = "core:command.test.lua_round_trip";
    Item.Sequence = 5;
    TestTrue(
        TEXT("Stack is restored and the next protected call can run"),
        Host.DispatchSemanticInput(Item, Fault));

    Host.Stop();
    TestFalse(TEXT("Lua VM stops deterministically"), Host.IsStarted());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaModuleGraphTest,
    "GV2.Runtime.Lua.ModuleManifestAndDeclaredDependencies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaModuleGraphTest::RunTest(const FString& Parameters)
{
    const std::vector<GV2RuntimeCore::FRuntimeSource> Sources = LoadTestRuntimeSources();
    GV2RuntimeCore::FRuntimeFault Fault;

    std::vector<GV2RuntimeCore::FRuntimeSource> MissingSource = Sources;
    MissingSource.erase(
        std::remove_if(
            MissingSource.begin(),
            MissingSource.end(),
            [](const GV2RuntimeCore::FRuntimeSource& Source)
            {
                return Source.Name == "@core/resources/service.lua";
            }),
        MissingSource.end());
    GV2RuntimeCore::FRuntimeSession MissingSourceHost;
    TestFalse(
        TEXT("Manifest rejects a missing declared module source"),
        MissingSourceHost.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), MissingSource, Fault));
    TestEqual(
        TEXT("Missing module source has a stable fault code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleSourceMissing")));

    std::vector<GV2RuntimeCore::FRuntimeSource> HiddenDependency = Sources;
    for (GV2RuntimeCore::FRuntimeSource& Source : HiddenDependency)
    {
        if (Source.Name == "@core/boundary/entrypoints.lua")
        {
            Source.Text.insert(
                0,
                "local hidden_dependency = require(\"core:module.resources.service\")\n");
        }
    }
    GV2RuntimeCore::FRuntimeSession HiddenDependencyHost;
    TestFalse(
        TEXT("Module cannot import an undeclared dependency"),
        HiddenDependencyHost.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), HiddenDependency, Fault));
    TestEqual(
        TEXT("Hidden dependency fails during module initialization"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleLoadError")));
    TestTrue(
        TEXT("Hidden dependency diagnostic identifies the manifest violation"),
        FString(UTF8_TO_TCHAR(Fault.Message.c_str())).Contains(TEXT("not declared")));

    std::vector<GV2RuntimeCore::FRuntimeSource> UnlistedSource = Sources;
    UnlistedSource.push_back({"@core/gameplay/unlisted.lua", "return {}"});
    GV2RuntimeCore::FRuntimeSession UnlistedSourceHost;
    TestFalse(
        TEXT("Unlisted Lua source is rejected"),
        UnlistedSourceHost.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), UnlistedSource, Fault));
    TestEqual(
        TEXT("Unlisted source has a stable fault code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleSourceUnlisted")));

    const std::vector<GV2RuntimeCore::FRuntimeSource> CyclicSources = {
        {
            "@core/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.runtime.a",
                modules = {
                    {
                        module_id = "core:module.runtime.a",
                        source = "runtime/a.lua",
                        dependencies = { "core:module.runtime.b" },
                    },
                    {
                        module_id = "core:module.runtime.b",
                        source = "runtime/b.lua",
                        dependencies = { "core:module.runtime.a" },
                    },
                },
            })lua"},
        {"@core/runtime/a.lua", "return {}"},
        {"@core/runtime/b.lua", "return {}"},
    };
    GV2RuntimeCore::FRuntimeSession CyclicHost;
    TestFalse(
        TEXT("Cyclic module dependencies are rejected"),
        CyclicHost.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), CyclicSources, Fault));
    TestEqual(
        TEXT("Dependency cycle has a stable fault code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleDependencyCycle")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaModulePackageOverrideTest,
    "GV2.Runtime.Lua.ModulePackageOverrideAndSealing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaModulePackageOverrideTest::RunTest(const FString& Parameters)
{
    GV2RuntimeCore::FRuntimeFault Fault;

    // 1. Successful module override with require_base()
    const std::vector<GV2RuntimeCore::FRuntimeSource> OverrideSources = {
        {
            "@core/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.gameplay.root",
                modules = {
                    {
                        module_id = "core:module.gameplay.root",
                        source = "gameplay/root.lua",
                        dependencies = {},
                        replaceable = true,
                    },
                },
            })lua"
        },
        {
            "@core/gameplay/root.lua",
            R"lua(
                local M = {
                    id = "core:module.gameplay.root",
                    base_version = 1,
                }
                function M.compute(val)
                    return val * 2
                end
                return M
            )lua"
        },
        {
            "@test_mod/manifest.lua",
            R"lua(return {
                modules = {
                    {
                        module_id = "core:module.gameplay.root",
                        source = "gameplay/root.lua",
                        dependencies = {},
                        replaceable = true,
                    },
                },
            })lua"
        },
        {
            "@test_mod/gameplay/root.lua",
            R"lua(
                local base = require_base()
                local M = setmetatable({
                    id = "core:module.gameplay.root",
                    mod_version = 2,
                }, { __index = base })
                function M.compute(val)
                    return base.compute(val) + 10
                end
                return M
            )lua"
        },
    };

    GV2RuntimeCore::FRuntimeSession OverrideSession;
    TestTrue(
        TEXT("Runtime starts with valid multi-package module override"),
        OverrideSession.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), OverrideSources, Fault));

    const std::string OverrideHash = OverrideSession.GetScriptSetHash();
    TestEqual(TEXT("Override session script set hash length is 64"), OverrideHash.length(), static_cast<std::size_t>(64));

    const auto ReplacedModules = OverrideSession.GetReplacedModules();
    TestEqual(TEXT("Replaced modules count is 1"), ReplacedModules.size(), static_cast<std::size_t>(1));
    if (ReplacedModules.size() == 1)
    {
        TestEqual(TEXT("Replaced module id is core:module.gameplay.root"),
            FString(UTF8_TO_TCHAR(ReplacedModules[0].ModuleId.c_str())),
            FString(TEXT("core:module.gameplay.root")));
        TestEqual(TEXT("Replaced module provider count is 2"), ReplacedModules[0].Providers.size(), static_cast<std::size_t>(2));
        if (ReplacedModules[0].Providers.size() == 2)
        {
            TestEqual(TEXT("Provider 0 is core"), FString(UTF8_TO_TCHAR(ReplacedModules[0].Providers[0].c_str())), FString(TEXT("core")));
            TestEqual(TEXT("Provider 1 is test_mod"), FString(UTF8_TO_TCHAR(ReplacedModules[0].Providers[1].c_str())), FString(TEXT("test_mod")));
        }
    }

    // Base sources without override produces a different ScriptSetHash
    const std::vector<GV2RuntimeCore::FRuntimeSource> BaseOnlySources = { OverrideSources[0], OverrideSources[1] };
    GV2RuntimeCore::FRuntimeSession BaseSession;
    TestTrue(TEXT("Base session starts"), BaseSession.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), BaseOnlySources, Fault));
    const std::string BaseHash = BaseSession.GetScriptSetHash();
    TestTrue(TEXT("Override changes ScriptSetHash"), BaseHash != OverrideHash);
    TestEqual(TEXT("Base session has 0 replaced modules"), BaseSession.GetReplacedModules().size(), static_cast<std::size_t>(0));

    // CheckScripts also reports ScriptSetHash and ReplacedModules
    std::size_t CheckedCount = 0;
    std::string CheckHash;
    std::vector<GV2RuntimeCore::FReplacedModuleInfo> CheckReplaced;
    GV2RuntimeCore::FRuntimeSession CheckSession;
    TestTrue(TEXT("CheckScripts succeeds"), CheckSession.CheckScripts(1, MakeFrozenCoreFixturePinnedRepository(*this), OverrideSources, &CheckedCount, &CheckHash, &CheckReplaced, Fault));
    TestEqual(TEXT("CheckScripts hash matches started session"), CheckHash, OverrideHash);
    TestEqual(TEXT("CheckScripts replaced modules count is 1"), CheckReplaced.size(), static_cast<std::size_t>(1));

    // 2. Replacing sealed module triggers LuaModuleSealed
    const std::vector<GV2RuntimeCore::FRuntimeSource> SealedSources = {
        {
            "@core/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.runtime.sealed",
                modules = {
                    {
                        module_id = "core:module.runtime.sealed",
                        source = "runtime/sealed.lua",
                        dependencies = {},
                        replaceable = false,
                    },
                },
            })lua"
        },
        {
            "@core/runtime/sealed.lua",
            "return { id = 'core:module.runtime.sealed' }"
        },
        {
            "@test_mod/manifest.lua",
            R"lua(return {
                modules = {
                    {
                        module_id = "core:module.runtime.sealed",
                        source = "runtime/sealed.lua",
                        dependencies = {},
                    },
                },
            })lua"
        },
        {
            "@test_mod/runtime/sealed.lua",
            "return { id = 'core:module.runtime.sealed' }"
        },
    };

    GV2RuntimeCore::FRuntimeSession SealedSession;
    TestFalse(
        TEXT("Overriding sealed module is rejected"),
        SealedSession.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), SealedSources, Fault));
    TestEqual(
        TEXT("Sealed module override error code is LuaModuleSealed"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleSealed")));

    // 3. Mod declaring foreign new module ID triggers LuaModuleForeignNewId
    const std::vector<GV2RuntimeCore::FRuntimeSource> ForeignNewSources = {
        {
            "@core/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.bootstrap.main",
                modules = {
                    {
                        module_id = "core:module.bootstrap.main",
                        source = "bootstrap/main.lua",
                        dependencies = {},
                    },
                },
            })lua"
        },
        {
            "@core/bootstrap/main.lua",
            "return {}"
        },
        {
            "@test_mod/manifest.lua",
            R"lua(return {
                modules = {
                    {
                        module_id = "core:module.gameplay.foreign_new",
                        source = "gameplay/foreign.lua",
                        dependencies = {},
                    },
                },
            })lua"
        },
        {
            "@test_mod/gameplay/foreign.lua",
            "return {}"
        },
    };

    GV2RuntimeCore::FRuntimeSession ForeignNewSession;
    TestFalse(
        TEXT("Mod cannot introduce a new core module ID"),
        ForeignNewSession.Start(1, MakeFrozenCoreFixturePinnedRepository(*this), ForeignNewSources, Fault));
    TestEqual(
        TEXT("Foreign new ID error code is LuaModuleForeignNewId"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleForeignNewId")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiBindingRegistryPublicationTest,
    "GV2.Runtime.Bindings.AtomicPublicationAndLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiBindingRegistryPublicationTest::RunTest(const FString& Parameters)
{
    FGV2UiBindingRegistry Registry;
    Registry.BeginSession(11);

    TArray<FGV2UiBindingHandle> RevisionOneHandles;
    TestTrue(
        TEXT("Initial binding set is published"),
        Registry.PublishBindings(
            TEXT("ui@11:1"),
            1,
            {MakeBindingDefinition(TEXT("first"), TEXT("core:command.test.first"))},
            RevisionOneHandles));
    TestEqual(TEXT("Initial revision has one binding"), RevisionOneHandles.Num(), 1);
    if (RevisionOneHandles.Num() != 1)
    {
        return false;
    }

    FGV2PreparedBindingSet NonMonotonicCandidate;
    TestFalse(
        TEXT("Current UI instance rejects a non-monotonic revision"),
        Registry.PrepareBindings(
            TEXT("ui@11:1"),
            1,
            {MakeBindingDefinition(TEXT("repeated"), TEXT("core:command.test.repeated"))},
            NonMonotonicCandidate));
    TestEqual(
        TEXT("Rejected non-monotonic revision preserves current revision"),
        Registry.GetRevision(),
        int64{1});
    TestEqual(
        TEXT("Rejected non-monotonic revision preserves current bindings"),
        Registry.Num(),
        1);

    FGV2UiBindingDefinition DuplicatePath = MakeBindingDefinition(
        TEXT("duplicate"),
        TEXT("core:command.test.duplicate"));
    TArray<FGV2UiBindingHandle> RejectedHandles;
    TestFalse(
        TEXT("Invalid candidate publication is rejected atomically"),
        Registry.PublishBindings(
            TEXT("ui@11:1"),
            2,
            {DuplicatePath, DuplicatePath},
            RejectedHandles));
    TestEqual(TEXT("Rejected publication returns no handles"), RejectedHandles.Num(), 0);
    TestEqual(TEXT("Rejected publication preserves revision"), Registry.GetRevision(), int64{1});
    TestEqual(TEXT("Rejected publication preserves bindings"), Registry.Num(), 1);

    FGV2UiBindingRecord Record;
    TestEqual(
        TEXT("Old binding remains resolvable after rejected publication"),
        Registry.Resolve(RevisionOneHandles[0], Record),
        EGV2BindingResolveResult::Found);

    FGV2PreparedBindingSet PreparedRevisionTwo;
    TestTrue(
        TEXT("Valid binding candidate can be prepared without publication"),
        Registry.PrepareBindings(
            TEXT("ui@11:1"),
            2,
            {MakeBindingDefinition(TEXT("second"), TEXT("core:command.test.second"))},
            PreparedRevisionTwo));
    TestEqual(TEXT("Preparation does not advance current revision"), Registry.GetRevision(), int64{1});
    TestEqual(
        TEXT("Old binding remains current while candidate Widgets are prepared"),
        Registry.Resolve(RevisionOneHandles[0], Record),
        EGV2BindingResolveResult::Found);
    TArray<FGV2UiBindingHandle> RevisionTwoHandles = PreparedRevisionTwo.Handles;
    TestTrue(
        TEXT("Prepared binding set becomes current only at commit"),
        Registry.CommitPreparedBindings(MoveTemp(PreparedRevisionTwo)));
    TestEqual(
        TEXT("Superseded binding is invalid in the same session"),
        Registry.Resolve(RevisionOneHandles[0], Record),
        EGV2BindingResolveResult::Invalid);

    Registry.BeginSession(12);
    TestEqual(
        TEXT("Binding from a previous generation is stale"),
        Registry.Resolve(RevisionTwoHandles[0], Record),
        EGV2BindingResolveResult::Stale);
    return true;
}

// UPP-29: FGV2SessionCoordinator presentation preparation and atomic single-commit;
// failure injection during presentation apply leaves previous revision and bindings untouched;
// handles from previous revisions resolve to Invalid, while handles from previous generations resolve to Stale.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionCoordinatorPreparedCommitAndFailureInjectionTest,
    "GV2.Runtime.Session.PreparedCommitAndFailureInjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionCoordinatorPreparedCommitAndFailureInjectionTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;

    FGV2UiDocumentViewModel LastCapturedDoc;
    bool bSinkShouldSucceed = true;
    int32 DocumentSinkCallCount = 0;

    Coordinator.SetDocumentSink(
        [&LastCapturedDoc, &bSinkShouldSucceed, &DocumentSinkCallCount](const FGV2UiDocumentViewModel& Doc) -> bool
        {
            ++DocumentSinkCallCount;
            LastCapturedDoc = Doc;
            return bSinkShouldSucceed;
        });

    // 1. Session start -> Prepares candidate document and atomically commits revision 1
    TestTrue(
        TEXT("Coordinator starts session"),
        Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Lua VM started"), Coordinator.IsLuaVmStarted());
    TestTrue(TEXT("Session is ready"), Coordinator.GetStatus().bIsReady);
    TestEqual(TEXT("DocumentSink was invoked during StartSession"), DocumentSinkCallCount, 1);
    TestEqual(TEXT("Initial UI revision is 1"), Coordinator.GetUiRevision(), int64{1});
    TestEqual(TEXT("Initial binding registry revision is 1"), Coordinator.GetBindingRegistry().GetRevision(), int64{1});
    TestTrue(TEXT("Initial document model has route"), LastCapturedDoc.bHasRoute);

    // 2. Publish additional verified bindings for revision 2
    TArray<FGV2UiBindingHandle> Rev2Handles;
    const FGV2UiBindingDefinition DefRev2 = MakeBindingDefinition(
        TEXT("btn_rev2"),
        TEXT("core:command.test.step"));

    TestTrue(
        TEXT("Publishing revision 2 candidate succeeds"),
        Coordinator.PublishUiBindings(TEXT("ui@1:1"), 2, {DefRev2}, Rev2Handles));
    TestEqual(TEXT("1 handle returned for revision 2"), Rev2Handles.Num(), 1);
    if (Rev2Handles.Num() != 1)
    {
        return false;
    }
    const FGV2UiBindingHandle HandleRev2 = Rev2Handles[0];
    TestEqual(TEXT("Binding registry revision is 2"), Coordinator.GetBindingRegistry().GetRevision(), int64{2});

    FGV2UiBindingRecord Record;
    TestEqual(
        TEXT("Rev 2 handle resolves to Found"),
        Coordinator.ResolveBinding(HandleRev2, Record),
        EGV2BindingResolveResult::Found);
    TestEqual(TEXT("Record command matches"), Record.CommandId, FString(TEXT("core:command.test.step")));

    // 3. Failure injection between prepare and commit:
    // Prepare candidate revision 3 bindings into FGV2PreparedBindingSet off-tree
    FGV2PreparedBindingSet CandidateRev3;
    const FGV2UiBindingDefinition DefRev3 = MakeBindingDefinition(
        TEXT("btn_rev3"),
        TEXT("core:command.test.step"));
    TestTrue(
        TEXT("Prepare candidate revision 3 bindings succeeds"),
        Coordinator.GetBindingRegistry().PrepareBindings(
            TEXT("ui@1:1"),
            3,
            {DefRev3},
            CandidateRev3));
    TestEqual(TEXT("Candidate rev 3 has 1 handle"), CandidateRev3.Handles.Num(), 1);
    const FGV2UiBindingHandle HandleRev3 = CandidateRev3.Handles.Num() > 0 ? CandidateRev3.Handles[0] : FGV2UiBindingHandle();

    // While candidate is prepared but NOT committed:
    // a) Current revision remains at 2
    TestEqual(TEXT("Revision is still 2 before commit"), Coordinator.GetBindingRegistry().GetRevision(), int64{2});
    // b) Old handle (Rev 2) remains valid and Found
    TestEqual(
        TEXT("Rev 2 handle remains Found before commit"),
        Coordinator.ResolveBinding(HandleRev2, Record),
        EGV2BindingResolveResult::Found);
    // c) Uncommitted candidate handle resolves to Invalid
    TestEqual(
        TEXT("Uncommitted candidate handle resolves to Invalid"),
        Coordinator.ResolveBinding(HandleRev3, Record),
        EGV2BindingResolveResult::Invalid);

    // Simulated failure injection: Presentation fails off-tree -> Candidate is discarded without commit
    CandidateRev3 = {}; // Discard candidate
    TestEqual(TEXT("Revision remains at 2 after discarded candidate"), Coordinator.GetBindingRegistry().GetRevision(), int64{2});
    TestEqual(
        TEXT("Rev 2 handle is still Found after discarded candidate"),
        Coordinator.ResolveBinding(HandleRev2, Record),
        EGV2BindingResolveResult::Found);

    // 4. Successful recovery: Prepare and commit revision 3 in one atomic step
    FGV2PreparedBindingSet SuccessfulCandidateRev3;
    TestTrue(
        TEXT("Prepare candidate revision 3 again"),
        Coordinator.GetBindingRegistry().PrepareBindings(
            TEXT("ui@1:1"),
            3,
            {DefRev3},
            SuccessfulCandidateRev3));
    const FGV2UiBindingHandle SuccessfulHandleRev3 = SuccessfulCandidateRev3.Handles[0];

    // Atomically commit prepared bindings
    TestTrue(
        TEXT("Commit prepared candidate succeeds"),
        Coordinator.PublishUiBindings(
            TEXT("ui@1:1"),
            3,
            {DefRev3},
            Rev2Handles));
    TestEqual(TEXT("Revision advances to 3 on commit"), Coordinator.GetBindingRegistry().GetRevision(), int64{3});

    // Rev 3 handle is now Found
    TestEqual(
        TEXT("Committed Rev 3 handle resolves to Found"),
        Coordinator.ResolveBinding(Rev2Handles[0], Record),
        EGV2BindingResolveResult::Found);

    // Rev 2 handle from previous revision is now Invalid (superseded in same session)
    TestEqual(
        TEXT("Superseded Rev 2 handle in same session resolves to Invalid"),
        Coordinator.ResolveBinding(HandleRev2, Record),
        EGV2BindingResolveResult::Invalid);

    // 5. Test session generation change -> Stale handle resolution
    Coordinator.EndSession();
    TestTrue(
        TEXT("Coordinator restarts with new session generation"),
        Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestEqual(TEXT("Session generation is 2"), Coordinator.GetStatus().SessionGeneration, 2);

    // Resolving HandleRev2 (minted in generation 1) must return Stale (distinct from Invalid)
    TestEqual(
        TEXT("Handle from previous generation resolves to Stale"),
        Coordinator.ResolveBinding(HandleRev2, Record),
        EGV2BindingResolveResult::Stale);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeIngressDispatchTest,
    "GV2.Runtime.Ingress.FifoAndNonReentrantDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeIngressDispatchTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts its Lua VM"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Lua VM belongs to the active session"), Coordinator.IsLuaVmStarted());

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Two bindings are published"),
        Coordinator.PublishUiBindings(
            TEXT("ui@1:1"),
            2,
            {
                MakeBindingDefinition(TEXT("first"), TEXT("core:command.test.first")),
                MakeBindingDefinition(TEXT("second"), TEXT("core:command.test.second"))
            },
            Handles));
    TestEqual(TEXT("Two handles are returned"), Handles.Num(), 2);
    if (Handles.Num() != 2)
    {
        return false;
    }

    TArray<FString> Commands;
    TArray<int64> Sequences;
    int32 DispatchDepth = 0;
    int32 MaxDispatchDepth = 0;
    EGV2SubmitUiInteractionResult NestedResult = EGV2SubmitUiInteractionResult::RuntimeNotReady;
    Coordinator.SetInteractionSink(
        [this, &Coordinator, &Handles, &Commands, &Sequences, &DispatchDepth, &MaxDispatchDepth, &NestedResult](
            const FGV2UiIngressItem& Item)
        {
            ++DispatchDepth;
            MaxDispatchDepth = FMath::Max(MaxDispatchDepth, DispatchDepth);
            TestFalse(TEXT("Host sink runs only after Lua returns"), Coordinator.IsExecutingRuntime());
            Commands.Add(Item.Binding.CommandId);
            Sequences.Add(Item.Sequence);

            if (Commands.Num() == 1)
            {
                NestedResult = Coordinator.SubmitUiInteraction(Handles[1], {});
            }
            --DispatchDepth;
        });

    TestEqual(
        TEXT("First interaction is accepted"),
        Coordinator.SubmitUiInteraction(Handles[0], {}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestEqual(
        TEXT("Interaction submitted by the sink is queued"),
        NestedResult,
        EGV2SubmitUiInteractionResult::Accepted);
    TestEqual(TEXT("Both commands are dispatched"), Commands.Num(), 2);
    TestEqual(TEXT("Dispatch remains non-reentrant"), MaxDispatchDepth, 1);
    TestEqual(TEXT("Ingress is drained"), Coordinator.GetQueuedIngressCount(), 0);
    if (Commands.Num() == 2 && Sequences.Num() == 2)
    {
        TestEqual(TEXT("First command retains FIFO order"), Commands[0], FString(TEXT("core:command.test.first")));
        TestEqual(TEXT("Second command retains FIFO order"), Commands[1], FString(TEXT("core:command.test.second")));
        TestEqual(TEXT("First sequence is one"), Sequences[0], int64{1});
        TestEqual(TEXT("Second sequence is two"), Sequences[1], int64{2});
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeInputSchemaTest,
    "GV2.Runtime.Ingress.InputSchemaValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeInputSchemaTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts for schema validation"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));

    FGV2UiBindingDefinition Definition = MakeBindingDefinition(
        TEXT("form"),
        TEXT("core:command.test.form"));
    Definition.InputFields = {
        {TEXT("choice"), EGV2UiControlValueType::String, true},
        {TEXT("preview"), EGV2UiControlValueType::Boolean, false}
    };

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Typed input binding is published"),
        Coordinator.PublishUiBindings(TEXT("ui@1:1"), 2, {Definition}, Handles));
    if (Handles.Num() != 1)
    {
        return false;
    }

    FGV2UiControlValue Choice;
    Choice.Name = TEXT("choice");
    Choice.Type = EGV2UiControlValueType::String;
    Choice.StringValue = TEXT("alpha");

    FGV2UiControlValue Preview;
    Preview.Name = TEXT("preview");
    Preview.Type = EGV2UiControlValueType::Boolean;
    Preview.BooleanValue = true;

    FGV2UiControlValue Extra;
    Extra.Name = TEXT("extra");
    Extra.Type = EGV2UiControlValueType::String;

    FGV2UiControlValue WrongType = Choice;
    WrongType.Type = EGV2UiControlValueType::Integer;

    TestEqual(
        TEXT("Missing required field is rejected"),
        Coordinator.SubmitUiInteraction(Handles[0], {Preview}),
        EGV2SubmitUiInteractionResult::InvalidInputValues);
    TestEqual(
        TEXT("Extra field is rejected"),
        Coordinator.SubmitUiInteraction(Handles[0], {Choice, Extra}),
        EGV2SubmitUiInteractionResult::InvalidInputValues);
    TestEqual(
        TEXT("Wrong field type is rejected"),
        Coordinator.SubmitUiInteraction(Handles[0], {WrongType}),
        EGV2SubmitUiInteractionResult::InvalidInputValues);
    TestEqual(
        TEXT("Duplicate field is rejected"),
        Coordinator.SubmitUiInteraction(Handles[0], {Choice, Choice}),
        EGV2SubmitUiInteractionResult::InvalidInputValues);

    TArray<int64> Sequences;
    Coordinator.SetInteractionSink(
        [&Sequences](const FGV2UiIngressItem& Item) { Sequences.Add(Item.Sequence); });
    TestEqual(
        TEXT("Required field alone is accepted"),
        Coordinator.SubmitUiInteraction(Handles[0], {Choice}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestEqual(
        TEXT("Declared optional field is accepted"),
        Coordinator.SubmitUiInteraction(Handles[0], {Choice, Preview}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestEqual(TEXT("Only valid inputs are dispatched"), Sequences.Num(), 2);
    if (Sequences.Num() == 2)
    {
        TestEqual(TEXT("Rejected inputs do not consume sequence"), Sequences[0], int64{1});
        TestEqual(TEXT("Accepted sequence remains contiguous"), Sequences[1], int64{2});
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeIngressCapacityTest,
    "GV2.Runtime.Ingress.BoundedCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeIngressCapacityTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator(0);
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts for capacity validation"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Binding is published for capacity test"),
        Coordinator.PublishUiBindings(
            TEXT("ui@1:1"),
            2,
            {MakeBindingDefinition(TEXT("only"), TEXT("core:command.test.only"))},
            Handles));
    if (Handles.Num() != 1)
    {
        return false;
    }

    int32 SinkCalls = 0;
    Coordinator.SetInteractionSink([&SinkCalls](const FGV2UiIngressItem&) { ++SinkCalls; });
    TestEqual(
        TEXT("Full ingress reports backpressure"),
        Coordinator.SubmitUiInteraction(Handles[0], {}),
        EGV2SubmitUiInteractionResult::IngressQueueFull);
    TestEqual(TEXT("Rejected ingress is not dispatched"), SinkCalls, 0);
    TestEqual(TEXT("Rejected ingress is not retained"), Coordinator.GetQueuedIngressCount(), 0);
    return true;
}

// PCC-36/PCC-37 milestone check: an active session never switches its pinned
// repository handle when the Application-level current snapshot is
// republished; a new snapshot only takes effect for the *next* session,
// created via controlled restart (BootstrapAndSessionLifecycle.md
// "Active session никогда не переключает pinned handle" /
// GameDataRepositoryContract.md "Reload").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionRepositoryPinningAcrossRestartTest,
    "GV2.Runtime.ContentCore.SessionRepositoryPinningAcrossRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionRepositoryPinningAcrossRestartTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    const FString CorePackageRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData/core"));
    const GV2ContentCore::FBuildResult ResultA = BuildGV2RepositoryFromDirectory(CorePackageRoot);

    using namespace GV2ContentCore;
    const FString FixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore")));
    const FPackageDescriptor CoreDesc = Testing::MakeRepresentativeCorePackageDescriptor();
    const FPackageDescriptor TestModDesc = Testing::MakeRepresentativeTestModPackageDescriptor();

    FTestMultiPackageSourceProvider Provider;
    // MakeRepresentativeCorePackageDescriptor() lists definitions/*.json5 files
    // that only exist in the "valid/core" fixture, not in the real
    // GameData/core (its entity content moved to GameData/rh long ago), so
    // this multi-package build must read from the fixture, not CorePackageRoot.
    Provider.PackageRoots.emplace("core", FPaths::Combine(FixtureRoot, TEXT("valid/core")));
    Provider.PackageRoots.emplace("test_mod", FPaths::Combine(FixtureRoot, TEXT("valid/test_mod")));

    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult ResultB = BuildRepository({ CoreDesc, TestModDesc }, Options);

    // This test intentionally mixes two explicitly named sources. ResultA is
    // the live GameData/core integration gate; ResultB is a frozen repository
    // fixture used only to obtain a distinct snapshot. Neither may substitute
    // for the other when its build fails.
    const FRepositoryReadHandle ValidatedGameDataCore = RequirePinnedRepository(
        *this, ResultA, TEXT("live GameData/core"));
    const FRepositoryReadHandle ValidatedModdedFixture = RequirePinnedRepository(
        *this, ResultB, TEXT("frozen core+test_mod fixture"));
    if (!ValidatedGameDataCore.IsValid() || !ValidatedModdedFixture.IsValid())
    {
        return false;
    }

    FGV2RepositoryPublisher Publisher;
    TestTrue(TEXT("Publish candidate A"), Publisher.PublishCandidate(ResultA));
    const GV2ContentCore::FRepositoryReadHandle ReadHandleA = Publisher.GetCurrent();
    const FString HashA(UTF8_TO_TCHAR(ReadHandleA.GetContentHash().c_str()));

    TestTrue(TEXT("Publish candidate B"), Publisher.PublishCandidate(ResultB));
    const GV2ContentCore::FRepositoryReadHandle ReadHandleB = Publisher.GetCurrent();
    const FString HashB(UTF8_TO_TCHAR(ReadHandleB.GetContentHash().c_str()));

    if (!TestNotEqual(TEXT("The two published repositories have distinct content hashes"), HashA, HashB))
    {
        return false;
    }

    FGV2SessionCoordinator Coordinator(4);
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) { return true; });
    TestTrue(TEXT("Session starts pinned to repository A"), Coordinator.StartSession(ReadHandleA, 1));
    TestEqual(
        TEXT("Active session is pinned to A's content hash"),
        FString(UTF8_TO_TCHAR(Coordinator.GetPinnedRepository().GetContentHash().c_str())),
        HashA);

    // Active session keeps A's pinned handle even though Publisher.GetCurrent() was updated to B!
    TestEqual(
        TEXT("Publisher current has updated to B"),
        FString(UTF8_TO_TCHAR(Publisher.GetCurrent().GetContentHash().c_str())),
        HashB);

    TestEqual(
        TEXT("Active session keeps A's pinned handle after Publisher updated to B"),
        FString(UTF8_TO_TCHAR(Coordinator.GetPinnedRepository().GetContentHash().c_str())),
        HashA);

    // Controlled restart: end the session, then start a new one against Publisher.GetCurrent() (which is B).
    Coordinator.EndSession();
    TestTrue(
        TEXT("Restarted session starts pinned to Publisher current (B)"),
        Coordinator.StartSession(Publisher.GetCurrent(), Publisher.GetVersion()));
    TestEqual(
        TEXT("Restarted session is pinned to B's content hash"),
        FString(UTF8_TO_TCHAR(Coordinator.GetPinnedRepository().GetContentHash().c_str())),
        HashB);

    return true;
}

// BootstrapAndSessionLifecycle.md "Mandatory tests": repository failure
// before VM. A missing/invalid pinned repository must refuse to start the
// Lua VM entirely, not just fail some later step.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionRejectsInvalidRepositoryTest,
    "GV2.Runtime.ContentCore.SessionRejectsInvalidRepository",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionRejectsInvalidRepositoryTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator(4);
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });

    // 1. Initial StartSession with invalid handle
    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    TestFalse(
        TEXT("StartSession rejects a default-constructed (invalid) repository handle"),
        Coordinator.StartSession(GV2ContentCore::FRepositoryReadHandle(), 0));
    TestFalse(TEXT("No Lua VM was started"), Coordinator.IsLuaVmStarted());
    TestFalse(TEXT("Session is not ready"), Coordinator.GetStatus().bIsReady);
    TestEqual(
        TEXT("Session state is Failed"),
        Coordinator.GetStatus().SessionState,
        EGV2SessionState::Failed);

    // 2. Start valid session, then call StartSession with invalid handle -- PSC-05
    // (BootstrapAndSessionLifecycle.md "Целевое правило"): a failure before StartSession
    // commits to replacing the active session (repository validity is the very first such
    // check) must leave that active session completely untouched, not tear it down for a
    // replacement attempt that never got far enough to justify destroying it.
    TestTrue(TEXT("Start valid session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Lua VM is started for valid session"), Coordinator.IsLuaVmStarted());
    TestTrue(TEXT("Session is ready"), Coordinator.GetStatus().bIsReady);
    const GV2ContentCore::FRepositoryReadHandle ActivePinnedRepository = Coordinator.GetPinnedRepository();
    const int32 ActiveSessionGeneration = Coordinator.GetStatus().SessionGeneration;

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    TestFalse(
        TEXT("StartSession rejects invalid handle on active session"),
        Coordinator.StartSession(GV2ContentCore::FRepositoryReadHandle(), 0));
    TestTrue(TEXT("Active VM keeps running -- the replacement attempt never committed"), Coordinator.IsLuaVmStarted());
    TestTrue(TEXT("Coordinator remains ready -- the prior session is untouched"), Coordinator.GetStatus().bIsReady);
    TestTrue(TEXT("Pinned repository handle is unchanged"), Coordinator.GetPinnedRepository().IsValid());
    TestEqual(
        TEXT("Pinned repository handle is the SAME handle the active session pinned"),
        Coordinator.GetPinnedRepository().GetContentHash(),
        ActivePinnedRepository.GetContentHash());
    TestEqual(
        TEXT("Session state is still Ready -- no replacement session was ever created"),
        Coordinator.GetStatus().SessionState,
        EGV2SessionState::Ready);
    TestEqual(
        TEXT("Session generation did not advance -- StartSession never reached the commit boundary"),
        Coordinator.GetStatus().SessionGeneration,
        ActiveSessionGeneration);

    return true;
}

// PSC-05 (ADR-0043 D1, BootstrapAndSessionLifecycle.md "Целевое правило"): a content
// candidate builder failure (Screen Registry/Image Catalog/UI schemas/Theme -- not just
// the repository-handle precondition FGV2SessionRejectsInvalidRepositoryTest covers) during
// a REPLACEMENT attempt must also leave a prior active session completely untouched, since
// StartSession has not yet committed to tearing anything down at that point.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionReplacementContentBuilderFailurePreservesActiveSessionTest,
    "GV2.Runtime.Session.ReplacementContentBuilderFailurePreservesActiveSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionReplacementContentBuilderFailurePreservesActiveSessionTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });

    TestTrue(TEXT("Start valid session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Session is ready"), Coordinator.GetStatus().bIsReady);
    const FGV2SessionContentSnapshot* ActiveSnapshot = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("Active session published a content snapshot"), ActiveSnapshot);
    const int32 ActiveSessionGeneration = Coordinator.GetStatus().SessionGeneration;

    // An empty (but non-null) ResolvedPackageSet resolves zero package/schema roots --
    // FGV2SessionContentCandidate::Build's Screen Registry step fails closed on an empty
    // closure (UGV2ScreenRegistry::Build's own "empty package load order" check), giving a
    // deterministic ScreenRegistryNotReady without needing to corrupt any real content file.
    const GV2ContentHostSupport::FResolvedPackageSet EmptyResolvedSet;
    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=ScreenRegistryNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    TestFalse(
        TEXT("Replacement attempt with an empty package set fails at the content candidate stage"),
        Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 2, EmptyResolvedSet));

    TestTrue(TEXT("Active VM keeps running"), Coordinator.IsLuaVmStarted());
    TestTrue(TEXT("Coordinator remains ready"), Coordinator.GetStatus().bIsReady);
    TestEqual(TEXT("Session state is still Ready"), Coordinator.GetStatus().SessionState, EGV2SessionState::Ready);
    TestEqual(
        TEXT("Session generation did not advance"),
        Coordinator.GetStatus().SessionGeneration,
        ActiveSessionGeneration);
    TestEqual(
        TEXT("Content snapshot is the SAME instance the active session published"),
        Coordinator.GetContentSnapshot(),
        ActiveSnapshot);

    return true;
}

// PSC-05 (ADR-0043 D1): the content snapshot must never become observable before the
// session it belongs to actually reaches Ready -- a failure AFTER the Lua VM starts but
// BEFORE the initial document commits (here: a DocumentSink that rejects the apply) must
// leave GetContentSnapshot() null, not a snapshot for a session that never became Ready.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionContentSnapshotNotPublishedBeforeReadyTest,
    "GV2.Runtime.Session.ContentSnapshotNotPublishedBeforeReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionContentSnapshotNotPublishedBeforeReadyTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    bool bSnapshotWasNullDuringDocumentSink = false;
    bool bDocumentSinkRan = false;
    // The DocumentSink runs synchronously from inside StartSession, strictly before
    // Status.bIsReady is ever set true -- reading GetContentSnapshot() from here is the
    // only way a black-box test can observe the mid-flight state directly (a post-failure
    // check alone can't distinguish "never published" from "published early, then reset by
    // FailRuntime" -- both look identical after the fact).
    Coordinator.SetDocumentSink(
        [&Coordinator, &bSnapshotWasNullDuringDocumentSink, &bDocumentSinkRan](const FGV2UiDocumentViewModel&) -> bool
        {
            bDocumentSinkRan = true;
            bSnapshotWasNullDuringDocumentSink = Coordinator.GetContentSnapshot() == nullptr;
            return false;
        });

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=InitialPresentationApplyFailed"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    TestFalse(
        TEXT("StartSession fails when the initial document cannot be applied"),
        Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("DocumentSink actually ran"), bDocumentSinkRan);
    TestTrue(
        TEXT("Content snapshot was still null when observed from inside the DocumentSink -- not published before Ready"),
        bSnapshotWasNullDuringDocumentSink);
    TestFalse(TEXT("Session is not ready"), Coordinator.GetStatus().bIsReady);
    TestNull(
        TEXT("Content snapshot was never published for a session that never reached Ready"),
        Coordinator.GetContentSnapshot());

    return true;
}

// PSC-04/05: the snapshot's Image Catalog is a fresh transient instance the candidate owns
// via TStrongObjectPtr (not a shared config asset, unlike Screen Registry/Theme) -- once
// EndSession() drops the snapshot and nothing else references it, it must actually become
// collectible, not linger pinned by some other forgotten strong reference.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionContentSnapshotImageCatalogGcLifetimeTest,
    "GV2.Runtime.Session.ContentSnapshotImageCatalogGcLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionContentSnapshotImageCatalogGcLifetimeTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    TWeakObjectPtr<UGV2ImageResourceCatalog> WeakCatalog;
    {
        FGV2SessionCoordinator Coordinator;
        Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
        TestTrue(TEXT("Start session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
        const FGV2SessionContentSnapshot* Snapshot = Coordinator.GetContentSnapshot();
        TestNotNull(TEXT("Session published a content snapshot"), Snapshot);
        if (Snapshot == nullptr)
        {
            return false;
        }
        WeakCatalog = Snapshot->GetImageCatalog().Catalog.Get();
        TestTrue(TEXT("Weak reference to the snapshot's Image Catalog is valid while the session is active"), WeakCatalog.IsValid());

        Coordinator.EndSession();
        TestNull(TEXT("EndSession clears the content snapshot"), Coordinator.GetContentSnapshot());
        // Coordinator (and its snapshot's TStrongObjectPtr pin) is destroyed here, at scope exit.
    }

    CollectGarbage(RF_NoFlags);
    TestFalse(
        TEXT("Image Catalog is collectible once the snapshot that pinned it is gone"),
        WeakCatalog.IsValid());

    return true;
}

// PSC-06 (ADR-0043 D1): a session that already has a published content snapshot, and a
// genuinely different candidate built afterward on the SAME coordinator, must not leak
// into each other -- the second candidate's resolved content reflects only its own input,
// never session 1's already-active snapshot. Session 1 runs through the coordinator's real
// StartSession() (proving a genuine Ready session exists and owns a snapshot); the second,
// differently-composed candidate is built via the exact same production
// FGV2SessionContentCandidate::Build() StartSession() itself calls (PSC-04's own equivalent
// test already proved this specific function doesn't leak between two independently-built
// candidates -- this test adds that the ALREADY-ACTIVE session's own published snapshot
// doesn't interfere either). A full second StartSession() success with rh's real content
// isn't used here: rh's gameplay Lua expects repository content the available frozen test
// fixtures don't provide, unrelated to what this test needs to demonstrate.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SequentialSessionsDoNotShareAuthoritiesTest,
    "GV2.Runtime.Session.SequentialSessionsDoNotShareAuthorities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SequentialSessionsDoNotShareAuthoritiesTest::RunTest(const FString& Parameters)
{
    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });

    // Session 1: sample override -- core+textsystem+sample -- a real, Ready session with
    // its own published snapshot.
    {
        FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true;
        TestTrue(TEXT("Session 1 starts"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
        FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false;
    }
    const FGV2SessionContentSnapshot* Snapshot1 = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("Session 1 published a content snapshot"), Snapshot1);
    if (Snapshot1 == nullptr)
    {
        return false;
    }
    const FString PackageIds1 = FString::Join(Snapshot1->GetOrderedPackageIds(), TEXT(","));

    // A genuinely different candidate -- core+textsystem+rh -- built directly via the same
    // production function, while session 1's snapshot is still the coordinator's active one.
    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> RhSet =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(
            {
                std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
                std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
                std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("rh")))),
            },
            ResolveDiagnostics);
    TestTrue(TEXT("core+textsystem+rh package set resolves"), RhSet.has_value());
    if (!RhSet.has_value())
    {
        return false;
    }

    TArray<FGV2SchemaPackageRoot> SchemaPackageRoots;
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : RhSet->OrderedSources)
    {
        SchemaPackageRoots.Add(FGV2SchemaPackageRoot{
            UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
            UTF8_TO_TCHAR(Source.Root.string().c_str())});
    }

    FGV2SessionContentSnapshot RhCandidate;
    GV2RuntimeCore::FRuntimeFault CandidateFault;
    TestTrue(
        TEXT("A differently-composed candidate builds successfully alongside session 1's active snapshot"),
        FGV2SessionContentCandidate::Build(
            MakeFrozenCoreFixturePinnedRepository(*this),
            *RhSet,
            SchemaPackageRoots,
            {},
            RhCandidate,
            CandidateFault));

    const FString PackageIdsRh = FString::Join(RhCandidate.GetOrderedPackageIds(), TEXT(","));
    TestNotEqual(TEXT("The new candidate's package ids differ from session 1's (rh vs sample)"), PackageIdsRh, PackageIds1);

    // Session 1's own snapshot is completely unaffected by building the second candidate.
    TestEqual(TEXT("Session 1's snapshot is still the same instance"), Coordinator.GetContentSnapshot(), Snapshot1);
    TestEqual(
        TEXT("Session 1's own package ids are unchanged"),
        FString::Join(Snapshot1->GetOrderedPackageIds(), TEXT(",")),
        PackageIds1);

    // The new candidate's own resolved screen identities reflect ITS OWN closure, not
    // session 1's -- the exact PAH-R3-class check PSC-04's equivalent test already proved
    // for two independently-built candidates.
    FGV2ResolvedScreenDescriptor Descriptor;
    FGV2ScreenResolutionRejection Rejection;
    const FGV2PresentationPrepareContext RhPrepareContext(RhCandidate);
    TestTrue(
        TEXT("The rh-composed candidate's own registered screen resolves through its own snapshot"),
        RhPrepareContext.ResolveScreen(TEXT("core:screen.test"), FGV2ScreenPlacement::TopLevel(TEXT("location_content")), Descriptor, Rejection));

    return true;
}

// PCC-39: FGV2LuaMarshaller unified marshalling conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaMarshallerConformanceTest,
    "GV2.Runtime.Lua.MarshallerConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaMarshallerConformanceTest::RunTest(const FString& Parameters)
{
    const std::string Failure = GV2RuntimeCore::Testing::RunLuaMarshallerConformance();
    TestTrue(
        *FString::Printf(
            TEXT("Lua marshaller conformance passes%s%s"),
            Failure.empty() ? TEXT("") : TEXT(": "),
            Failure.empty() ? TEXT("") : UTF8_TO_TCHAR(Failure.c_str())),
        Failure.empty());
    return true;
}

// PCC-41: Pinned Read Handle transmission and lifetime in FRuntimeSession
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeSessionPinnedHandleTest,
    "GV2.Runtime.Session.PinnedHandleLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeSessionPinnedHandleTest::RunTest(const FString& Parameters)
{
    GV2RuntimeCore::FRuntimeSession Session;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources = LoadTestRuntimeSources();

    // 1. Invalid handle is rejected before Lua VM creation with RepositoryNotReady
    TestFalse(
        TEXT("Start rejects uninitialized read handle"),
        Session.Start(1, GV2ContentCore::FRepositoryReadHandle(), RuntimeSources, Fault));
    TestEqual(
        TEXT("Fault code is RepositoryNotReady"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("RepositoryNotReady")));
    TestFalse(TEXT("VM is not started on invalid handle"), Session.IsStarted());
    TestFalse(TEXT("Pinned handle remains invalid"), Session.GetPinnedRepository().IsValid());

    // 2. Valid handle starts session and stores handle
    const GV2ContentCore::FRepositoryReadHandle PinnedHandle = MakeFrozenCoreFixturePinnedRepository(*this);
    TestTrue(TEXT("Test pinned repository is valid"), PinnedHandle.IsValid());
    TestTrue(
        TEXT("Start succeeds with valid pinned handle"),
        Session.Start(1, PinnedHandle, RuntimeSources, Fault));
    TestTrue(TEXT("Session is started"), Session.IsStarted());
    TestTrue(TEXT("Session stores valid pinned handle"), Session.GetPinnedRepository().IsValid());
    TestEqual(
        TEXT("Session retains exact pinned content hash"),
        FString(UTF8_TO_TCHAR(Session.GetPinnedRepository().GetContentHash().c_str())),
        FString(UTF8_TO_TCHAR(PinnedHandle.GetContentHash().c_str())));

    // 3. Stop releases the pinned handle
    TestTrue(TEXT("Stop succeeds"), Session.Stop());
    TestFalse(TEXT("Session is not started after Stop"), Session.IsStarted());
    TestFalse(TEXT("Stop clears pinned handle"), Session.GetPinnedRepository().IsValid());

    return true;
}

// PCC-42: game.repository Lua query API test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaRepositoryAccessTest,
    "GV2.Runtime.Lua.RepositoryAccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaRepositoryAccessTest::RunTest(const FString& Parameters)
{
    GV2RuntimeCore::FRuntimeSession Host;
    GV2RuntimeCore::FRuntimeFault Fault;

    const std::vector<GV2RuntimeCore::FRuntimeSource> TestSources = {
        {
            "@core/bootstrap/manifest.lua",
            R"lua(return {
                entry_module_id = "core:module.test.repository",
                modules = {
                    {
                        module_id = "core:module.test.repository",
                        source = "test/repository.lua",
                        dependencies = {},
                    },
                },
            })lua"
        },
        {
            "@core/test/repository.lua",
            R"lua(
                local M = {}

                -- 1. game.repository exists and has 4 functions
                assert(type(game.repository) == "table", "game.repository must be table")
                assert(type(game.repository.get) == "function", "get must be function")
                assert(type(game.repository.require) == "function", "require must be function")
                assert(type(game.repository.list) == "function", "list must be function")
                assert(type(game.repository.exists) == "function", "exists must be function")

                -- 2. game.repository is read-only
                local ok, err = pcall(function() game.repository.foo = 123 end)
                assert(not ok and string.find(tostring(err), "read%-only table"), "game.repository must be read-only")

                -- 3. game.data alias is absent
                assert(game.data == nil, "game.data alias must be absent")

                -- 4. exists query
                assert(game.repository.exists("core:item.weapon.iron_sword") == true, "iron_sword must exist")
                assert(game.repository.exists("core:item.non_existent") == false, "non_existent must not exist")
                assert(game.repository.exists("invalid_id_grammar") == false, "invalid id must not exist")

                -- 5. get query (happy path)
                local item, get_err = game.repository.get("core:item.weapon.iron_sword")
                assert(item ~= nil, "item must not be nil")
                assert(get_err == nil, "get_err must be nil")
                assert(item.id == "core:item.weapon.iron_sword", "item id must match")
                assert(type(item.data) == "table", "item data must be table")
                assert(item.data.price == 10, "item price must match")

                -- 6. Detached deep copy: mutating returned table does not affect subsequent query
                item.data.price = 999
                local item2, _ = game.repository.get("core:item.weapon.iron_sword")
                assert(item2.data.price == 10, "repository data must remain immutable across queries")

                -- 7. require query (happy path)
                local req_item = game.repository.require("core:item.weapon.iron_sword")
                assert(req_item.id == "core:item.weapon.iron_sword", "require item id must match")
                assert(req_item.data.price == 10, "require item price must match")

                -- 8. get negative queries (typed errors)
                local missing, miss_err = game.repository.get("core:item.non_existent")
                assert(missing == nil, "missing item must be nil")
                assert(type(miss_err) == "table", "miss_err must be table")
                assert(miss_err.code == "not_found", "miss_err code must be not_found")
                assert(miss_err.requested_id == "core:item.non_existent", "requested_id must match")

                local bad_id, bad_err = game.repository.get("not_a_stable_id")
                assert(bad_id == nil, "bad id must return nil")
                assert(type(bad_err) == "table" and bad_err.code == "invalid_id", "bad_id error code must be invalid_id")

                -- 9. require negative queries (throws with stable code as first token)
                local req_ok, req_err = pcall(function() game.repository.require("core:item.non_existent") end)
                assert(not req_ok, "require non_existent must throw error")
                assert(string.find(tostring(req_err), "not_found:") ~= nil, "require error message must contain not_found code token")

                local req_bad_ok, req_bad_err = pcall(function() game.repository.require("bad_id") end)
                assert(not req_bad_ok, "require bad_id must throw error")
                assert(string.find(tostring(req_bad_err), "invalid_id:") ~= nil, "require bad_id error message must contain invalid_id code token")

                -- 10. list query (canonical byte order, membership; TAS-11:
                -- not a pinned count/full listing — the frozen test corpus
                -- (TAS-06) may still gain an id when the subject of the
                -- change is content-resolution rules themselves)
                local function assert_sorted(list, kind)
                    for i = 2, #list do
                        assert(list[i - 1] < list[i], "list('" .. kind .. "') must be in canonical byte order")
                    end
                end
                local function list_contains(list, id)
                    for _, value in ipairs(list) do
                        if value == id then return true end
                    end
                    return false
                end

                local screen_ids = game.repository.list("screen")
                assert(type(screen_ids) == "table", "list('screen') must return table")
                assert_sorted(screen_ids, "screen")
                assert(list_contains(screen_ids, "core:screen.inventory"), "screen_ids must contain core:screen.inventory")
                assert(list_contains(screen_ids, "core:screen.main"), "screen_ids must contain core:screen.main")
                assert(list_contains(screen_ids, "test_mod:screen.codex_lab"), "screen_ids must contain test_mod:screen.codex_lab")

                local text_ids = game.repository.list("text")
                assert(type(text_ids) == "table", "list('text') must return table")
                assert_sorted(text_ids, "text")
                assert(list_contains(text_ids, "core:text.item.iron_sword.name"), "text_ids must contain core:text.item.iron_sword.name")
                assert(list_contains(text_ids, "test_mod:text.screen.codex_lab.title"), "text_ids must contain test_mod:text.screen.codex_lab.title")

                local actor_ids = game.repository.list("actor")
                assert(type(actor_ids) == "table", "list('actor') must return table")
                assert_sorted(actor_ids, "actor")
                assert(list_contains(actor_ids, "core:actor.character.hero"), "actor_ids must contain core:actor.character.hero")

                local item_ids = game.repository.list("item")
                assert(type(item_ids) == "table", "list('item') must return table")
                assert_sorted(item_ids, "item")
                assert(list_contains(item_ids, "core:item.weapon.iron_sword"), "item_ids must contain core:item.weapon.iron_sword")

                local empty_list = game.repository.list("non_existent_kind")
                assert(type(empty_list) == "table" and #empty_list == 0, "unknown kind must return empty table")

                local bad_param_list = game.repository.list(12345)
                assert(type(bad_param_list) == "table" and #bad_param_list == 0, "non-string kind must return empty table")

                local no_param_list = game.repository.list()
                assert(type(no_param_list) == "table" and #no_param_list == 0, "missing kind param must return empty table")

                -- 11. tombstoned definition query (tombstone code token)
                local tomb_item, tomb_err = game.repository.get("test_mod:screen.retired")
                assert(tomb_item == nil, "tombstoned item must be nil")
                assert(type(tomb_err) == "table" and tomb_err.code == "tombstoned", "tombstone error code must be tombstoned")
                assert(tomb_err.requested_id == "test_mod:screen.retired", "tombstone requested_id must match")

                local req_tomb_ok, req_tomb_err = pcall(function() game.repository.require("test_mod:screen.retired") end)
                assert(not req_tomb_ok, "require tombstoned must throw error")
                assert(string.find(tostring(req_tomb_err), "tombstoned:") ~= nil, "require tombstoned message must contain tombstoned code token")

                -- 12. redirect source query resolves to final active definition
                local redir_item, redir_err = game.repository.get("test_mod:screen.codex_archive")
                assert(redir_item ~= nil and redir_err == nil, "redirect source must resolve")
                assert(redir_item.id == "test_mod:screen.codex_lab", "redirect source must resolve to target definition ID")

                local redir_req = game.repository.require("test_mod:screen.codex_archive")
                assert(redir_req.id == "test_mod:screen.codex_lab", "require redirect source must resolve to target ID")

                -- 13. Absence of provenance / authoring metadata in Lua surface
                local inv_item = game.repository.require("core:screen.inventory")
                assert(inv_item.provenance == nil, "provenance must not leak into Lua")
                assert(inv_item.package_id == nil, "package_id must not leak into Lua")
                assert(inv_item.package == nil, "package must not leak into Lua")
                assert(inv_item.source == nil, "source must not leak into Lua")
                assert(inv_item.file == nil, "file must not leak into Lua")
                assert(inv_item.line == nil, "line must not leak into Lua")
                assert(inv_item.path == nil, "path must not leak into Lua")
                assert(inv_item.load_index == nil, "load_index must not leak into Lua")
                assert(inv_item.shadowed_providers == nil, "shadowed_providers must not leak into Lua")

                -- game.repository must only expose exactly 4 functions
                local repo_func_count = 0
                for k, v in pairs(game.repository) do
                    repo_func_count = repo_func_count + 1
                end
                assert(repo_func_count == 4, "game.repository must expose exactly 4 functions")
                assert(type(game.repository.get) == "function", "get must exist")
                assert(type(game.repository.require) == "function", "require must exist")
                assert(type(game.repository.list) == "function", "list must exist")
                assert(type(game.repository.exists) == "function", "exists must exist")

                return M
            )lua"
        }
    };

    const GV2ContentCore::FRepositoryReadHandle PinnedHandle = MakeModdedFixturePinnedRepository(*this);
    TestTrue(TEXT("Modded pinned repository is valid"), PinnedHandle.IsValid());
    TestTrue(
        TEXT("Start succeeds with test repository sources"),
        Host.Start(1, PinnedHandle, TestSources, Fault));
    if (!Host.IsStarted())
    {
        AddError(FString::Printf(
            TEXT("Host start failed: %s: %s"),
            UTF8_TO_TCHAR(Fault.Code.c_str()),
            UTF8_TO_TCHAR(Fault.Message.c_str())));
        return false;
    }

    return true;
}

// PCC-46: Cross-host Lua repository access conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaRepositoryConformanceCrossHostTest,
    "GV2.Runtime.Lua.RepositoryConformanceCrossHost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaRepositoryConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunLuaRepositoryAccessConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Lua repository access cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// GEW-01: Cross-host command validator registry conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ValidatorRegistryConformanceCrossHostTest,
    "GV2.Runtime.Lua.ValidatorRegistryConformanceCrossHost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ValidatorRegistryConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunValidatorRegistryConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Validator registry cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// TAS-12: GEW-04/GEW-05 conformance migrated to Tests/Lua/world/{domain_object,current_location}.lua,
// executed by GV2.Runtime.Lua.SpecRunnerHost (TAS-04) — no per-spec C++ wrapper needed.
// TAS-13: GEW-02/GEW-03 conformance migrated to
// Tests/Lua/commands/{validator_invocation,refusal_semantics}.lua, executed
// by GV2.Runtime.Lua.CommandValidatorSpecRunnerHost (GV2LuaSpecRunnerHostTests.cpp).

// TAS-02: Cross-host Lua spec runner mechanism conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LuaSpecRunnerConformanceCrossHostTest,
    "GV2.Runtime.Lua.SpecRunnerConformanceCrossHost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LuaSpecRunnerConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunLuaSpecRunnerConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Lua spec runner cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// SAV-07: Cross-host FFilesystemSaveSlotStorage conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveSlotStorageConformanceCrossHostTest,
    "GV2.Runtime.SaveAndLoad.SaveSlotStorageConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveSlotStorageConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunSaveSlotStorageConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Save slot storage cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// SAV-12/17: Cross-host cold-start load conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ColdStartLoadConformanceCrossHostTest,
    "GV2.Runtime.SaveAndLoad.ColdStartLoadConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ColdStartLoadConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2RuntimeCore::Testing::RunColdStartLoadConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Cold start load cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// PKG-01/02/03: Cross-host package manifest conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PackageManifestConformanceCrossHostTest,
    "GV2.Runtime.ContentCore.PackageManifestConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PackageManifestConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentHostSupport::Testing::RunPackageManifestConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Package manifest cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// PKG-05…09: Cross-host package discovery and order conformance test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PackageDiscoveryAndOrderConformanceCrossHostTest,
    "GV2.Runtime.ContentCore.PackageDiscoveryAndOrderConformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PackageDiscoveryAndOrderConformanceCrossHostTest::RunTest(const FString& Parameters)
{
    const std::string Error = GV2ContentHostSupport::Testing::RunPackageDiscoveryAndOrderConformance();
    if (!Error.empty())
    {
        AddError(FString::Printf(
            TEXT("Package discovery and order cross-host conformance failed: %s"),
            UTF8_TO_TCHAR(Error.c_str())));
        return false;
    }
    return true;
}

// PSC-04 (ADR-0043 D1): StartSession() builds one FGV2SessionContentSnapshot -- this test
// proves it is genuinely populated (repository/package identities/Lua sources/eagerly
// compiled schemas/Screen Registry/Image Catalog/Theme/GameShell class all resolved, no
// absolute filesystem roots leaked into it) and that its four identity hashes are real
// 64-lowercase-hex SHA-256 values, not empty placeholders.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionContentSnapshotContract,
    "GV2.Runtime.Session.ContentSnapshotContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionContentSnapshotContract::RunTest(const FString& Parameters)
{
    // MakeFrozenCoreFixturePinnedRepository pins a core-only synthetic repository; the
    // default (non-override) fallback closure would load the real rh package's gameplay
    // Lua, whose start hook expects rh-specific repository content this frozen fixture
    // doesn't have. The sample override switches the fallback closure to core+textsystem+
    // sample instead (CBM-03), matching what the fixture repository can actually satisfy --
    // the same override FGV2SessionCoordinatorPreparedCommitAndFailureInjectionTest already
    // uses with this exact repository fixture.
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Session is ready"), Coordinator.GetStatus().bIsReady);

    const FGV2SessionContentSnapshot* Snapshot = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("StartSession publishes a content snapshot"), Snapshot);
    if (Snapshot == nullptr)
    {
        return false;
    }

    TestTrue(TEXT("Snapshot's repository handle is valid"), Snapshot->GetRepository().IsValid());

    // core+textsystem+sample -- the sample-override fallback closure (CBM-03), unrelated
    // to which repository handle MakeFrozenCoreFixturePinnedRepository pinned.
    TArray<FString> ExpectedPackageIds = {TEXT("core"), TEXT("textsystem"), TEXT("sample")};
    TestEqual(
        TEXT("Snapshot's ordered package ids match the canonical closure"),
        FString::Join(Snapshot->GetOrderedPackageIds(), TEXT(",")),
        FString::Join(ExpectedPackageIds, TEXT(",")));

    TestTrue(TEXT("Snapshot's Lua source set is non-empty"), !Snapshot->GetLuaSources().empty());
    TestTrue(TEXT("Snapshot's script_set_hash is populated after RuntimeSession::Start"), !Snapshot->GetScriptSetHash().IsEmpty());

    // Eagerly compiled: a known ui_field schema resolves instantly (no discovery, no
    // filesystem access) through the snapshot's own cache.
    FString SchemaError;
    TestNotNull(
        TEXT("A known ui_field schema is already compiled in the snapshot's schema cache"),
        Snapshot->GetSchemaCache().GetCompiledSchema("core:schema.ui_field.text.v1", SchemaError).get());

    TestTrue(TEXT("Snapshot owns a resolved Screen Registry"), Snapshot->GetScreenRegistry().Registry.IsValid());
    TestTrue(TEXT("Snapshot owns a resolved Image Catalog"), Snapshot->GetImageCatalog().Catalog.IsValid());
    TestTrue(TEXT("Snapshot owns a resolved Theme"), Snapshot->GetTheme().Theme.IsValid());

    auto IsLowercaseHex = [](const FString& Value)
    {
        if (Value.IsEmpty())
        {
            return false;
        }
        for (const TCHAR Ch : Value)
        {
            const bool bDigit = Ch >= TEXT('0') && Ch <= TEXT('9');
            const bool bLowerHexLetter = Ch >= TEXT('a') && Ch <= TEXT('f');
            if (!bDigit && !bLowerHexLetter)
            {
                return false;
            }
        }
        return true;
    };
    TestTrue(TEXT("repository_content_hash is lowercase hex"), IsLowercaseHex(Snapshot->GetRepositoryContentHash()));
    TestTrue(TEXT("package_set_fingerprint is lowercase hex"), IsLowercaseHex(Snapshot->GetPackageSetFingerprint()));
    TestTrue(TEXT("presentation_hash is lowercase hex"), IsLowercaseHex(Snapshot->GetPresentationHash()));
    TestTrue(TEXT("session_content_id is lowercase hex"), IsLowercaseHex(Snapshot->GetSessionContentId()));

    // No absolute filesystem root leaks: OrderedPackageIds carries bare package_id strings
    // only, never a '/' (which an absolute path would always contain).
    for (const FString& PackageId : Snapshot->GetOrderedPackageIds())
    {
        TestFalse(
            *FString::Printf(TEXT("Package id '%s' is not a filesystem path"), *PackageId),
            PackageId.Contains(TEXT("/")));
    }

    Coordinator.EndSession();
    TestNull(TEXT("EndSession clears the content snapshot"), Coordinator.GetContentSnapshot());

    return true;
}

// PSC-10B (ADR-0043 D3): central style reaches a widget ONLY as a prepared operation.
//
// This is the production path, not a shape check: a real coordinator session publishes a
// real snapshot, FGV2PresentationPrepareContext reads THAT snapshot's theme during Prepare,
// and the physical write happens through the same Apply pair every other operation kind
// goes through. The widget itself is a plain UGV2SeparatorWidgetBase (the test subclass only
// populates the BindWidget members a Widget Blueprint would have populated).
//
// The source-derived boundary gate separately proves that no widget-local style entry point
// exists; this production-path test proves that the replacement transaction performs the
// physical write.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CentralStyleThroughPreparedTransactionTest,
    "GV2.Runtime.Presentation.CentralStyleThroughPreparedTransaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CentralStyleThroughPreparedTransactionTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));

    const FGV2SessionContentSnapshot* Snapshot = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("Session publishes a content snapshot"), Snapshot);
    if (Snapshot == nullptr)
    {
        return false;
    }
    const UGV2UiTheme* SnapshotTheme = Snapshot->GetTheme().Theme.Get();
    TestNotNull(TEXT("Snapshot owns a resolved Theme"), SnapshotTheme);
    if (SnapshotTheme == nullptr)
    {
        Coordinator.EndSession();
        return false;
    }

    // A value the theme provably does not carry, so neither half of this test can pass by
    // the widget happening to already sit on the expected value.
    constexpr float Sentinel = -73.5f;
    TestNotEqual(TEXT("Sentinel thickness differs from the snapshot theme's own thickness"),
        SnapshotTheme->SeparatorThickness, Sentinel);

    UGV2SeparatorBoundTestWidget* Separator = NewObject<UGV2SeparatorBoundTestWidget>();
    Separator->BuildBoundSubWidgets();
    Separator->SetTestOrientation(Orient_Horizontal);
    Separator->ApplySeparatorStyleValues(FSlateBrush(), Sentinel, /*bHorizontal=*/true);
    TestEqual(TEXT("Widget starts on the sentinel thickness"), Separator->ReadAppliedThickness(), Sentinel);

    // The preparer must find the separator by WALKING a subtree, not by being handed it --
    // that is what makes a styled widget nested anywhere below a screen reachable without
    // the widget pulling anything itself. Both descent rules are exercised at once: the
    // root is a UUserWidget (its own WidgetTree) whose tree root is a panel (its children).
    UGV2NewHostAddedOnlyInTestWidget* Root = NewObject<UGV2NewHostAddedOnlyInTestWidget>();
    Root->WidgetTree = NewObject<UWidgetTree>(Root);
    UVerticalBox* RootBox = Root->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
    Root->WidgetTree->RootWidget = RootBox;
    RootBox->AddChild(Separator);

    // A second styled class in the same subtree, carrying a DIFFERENT role, so the walk is
    // shown to route by role rather than emitting one shape for everything.
    UGV2ProgressBarBoundTestWidget* ProgressBar = NewObject<UGV2ProgressBarBoundTestWidget>();
    ProgressBar->BuildBoundSubWidgets();
    ProgressBar->ApplyProgressBarStyleValues(FProgressBarStyle(), FLinearColor::Transparent);
    RootBox->AddChild(ProgressBar);

    // RichText is the structurally different third path: its Slate decorator asks for
    // run/interactive styles later, during rendering. The widget must retain only the
    // prepared values delivered here, never a Theme or a resolver callback.
    UGV2RichTextWidgetBase* RichText = NewObject<UGV2RichTextWidgetBase>();
    RootBox->AddChild(RichText);

    const FGV2PresentationPrepareContext PrepareContext(*Snapshot);
    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    FString StyleError;
    TestTrue(TEXT("Subtree walk succeeds against a snapshot that carries a Theme"),
        GV2CentralStylePreparer::PrepareForSubtree(Root, PrepareContext, Transaction, StyleError));

    TestEqual(TEXT("Subtree walk emitted one central-style operation per styled widget"), Transaction.GetOperations().Num(), 3);
    if (Transaction.GetOperations().Num() != 3)
    {
        Coordinator.EndSession();
        return false;
    }
    for (const GV2PresentationApply::FGV2PreparedOperationVariant& Operation : Transaction.GetOperations())
    {
        TestEqual(TEXT("Every emitted operation's kind is CentralStyle"),
            static_cast<uint8>(GV2PresentationApply::GetPreparedOperationKind(Operation)),
            static_cast<uint8>(GV2PresentationApply::EGV2PreparedOperationKind::CentralStyle));
    }

    // Prepare resolved the theme; nothing has been written yet.
    TestEqual(TEXT("Preparing does not mutate the widget"), Separator->ReadAppliedThickness(), Sentinel);

    const uint64 ResolveCountBeforeApply = GV2PresentationAuthorityProbe::GetResolveCount();
    // PSC-11: one call applies the whole transaction. Before it, this assertion had to be
    // split across two entry points, and "applied" was not a single fact.
    FString ApplyError;
    TestTrue(TEXT("The single Apply facade applies the central-style operation"),
        GV2PresentationTestFixtures::ApplyPreparedTransaction(Transaction, ApplyError));
    const uint64 ResolveCountAfterApply = GV2PresentationAuthorityProbe::GetResolveCount();
    TestEqual(
        TEXT("Applying prepared central style resolves no presentation authority"),
        static_cast<int64>(ResolveCountAfterApply - ResolveCountBeforeApply),
        static_cast<int64>(0));
    TestEqual(TEXT("Applied thickness is the snapshot theme's own SeparatorThickness"),
        Separator->ReadAppliedThickness(), SnapshotTheme->SeparatorThickness);
    TestEqual(TEXT("Applied brush is the snapshot theme's own SeparatorBrush"),
        Separator->ReadAppliedBrush().GetResourceName(), SnapshotTheme->SeparatorBrush.GetResourceName());
    TestEqual(TEXT("The second role reached its own target: fill colour is the theme's ProgressFillColor"),
        ProgressBar->ReadAppliedFillColor(), SnapshotTheme->ProgressFillColor);
    TestTrue(TEXT("RichText retained a fully resolved style payload"),
        RichText->GetPreparedRichTextStyle().bIsResolved);
    const FTextBlockStyle ResolvedRun = RichText->ResolveRunTextStyle(
        RichText->GetPreparedRichTextStyle().DefaultTokenName,
        NAME_None,
        NAME_None);
    TestEqual(TEXT("RichText run style uses the prepared default font object"),
        ResolvedRun.Font.FontObject,
        RichText->GetPreparedRichTextStyle().DefaultToken.BaseStyle.Font.FontObject);
    TestEqual(TEXT("RichText run style uses the prepared default typeface"),
        ResolvedRun.Font.TypefaceFontName,
        RichText->GetPreparedRichTextStyle().DefaultToken.BaseStyle.Font.TypefaceFontName);
    const FHyperlinkStyle ResolvedInteractive = RichText->ResolveInteractiveTextStyle(ResolvedRun);
    TestEqual(TEXT("RichText interactive style uses the prepared underline brush"),
        ResolvedInteractive.UnderlineStyle.Normal.GetResourceName(),
        SnapshotTheme->RichTextInteractiveStyle.UnderlineStyle.Normal.GetResourceName());

    // A role delivered to the wrong class is rejected, not applied to whatever the widget
    // happens to be. This is what the closed variant buys: the mismatch is impossible to
    // express in Prepare and diagnosable if a future preparer ever gets it wrong.
    {
        GV2PresentationApply::FPreparedCentralStyleOperation Mismatched;
        Mismatched.TargetWidget = ProgressBar;
        Mismatched.Payload.Set<GV2PresentationApply::FPreparedSeparatorStyle>(GV2PresentationApply::FPreparedSeparatorStyle());
        GV2PresentationApply::FGV2PreparedPresentationTransaction MismatchTransaction;
        MismatchTransaction.AddCentralStyleOperation(Mismatched);

        FString MismatchError;
        TestFalse(TEXT("A style role delivered to the wrong widget class is rejected"),
            GV2PresentationTestFixtures::ApplyPreparedTransaction(MismatchTransaction, MismatchError));
        TestTrue(TEXT("The rejection names the mismatch"),
            MismatchError.Contains(TEXT("central_style_target_mismatch")));
    }

    Coordinator.EndSession();
    return true;
}

// PSC-10B: the actual set of central-style targets is the set of classes implementing
// IGV2UiStyleConsumer -- enumerated from the reflection system here, never typed into this
// test. Each is instantiated and offered to the real preparer. A new style consumer with
// no prepared role fails immediately; there is no hand-maintained exemption list.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2CentralStyleImplementationInventoryTest,
    "GV2.Runtime.Presentation.CentralStyleImplementationInventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2CentralStyleImplementationInventoryTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    const FGV2SessionContentSnapshot* Snapshot = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("Session publishes a content snapshot"), Snapshot);
    if (Snapshot == nullptr)
    {
        return false;
    }
    const FGV2PresentationPrepareContext PrepareContext(*Snapshot);

    int32 InspectedCount = 0;
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
    {
        UClass* Class = *ClassIt;
        if (!Class->ImplementsInterface(UGV2UiStyleConsumer::StaticClass()))
        {
            continue;
        }
        if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            continue;
        }
        // Native production classes only: Blueprint-generated subclasses and this suite's
        // own test seams inherit their base's role and would double-count it.
        if (!Class->HasAnyClassFlags(CLASS_Native) || Class->HasMetaData(TEXT("GV2TestOnly")))
        {
            continue;
        }
        if (!Class->IsChildOf(UWidget::StaticClass()))
        {
            continue;
        }

        ++InspectedCount;
        UWidget* Instance = NewObject<UWidget>(GetTransientPackage(), Class);
        GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
        FString StyleError;
        TestTrue(
            *FString::Printf(TEXT("'%s' subtree walk succeeds"), *Class->GetName()),
            GV2CentralStylePreparer::PrepareForSubtree(Instance, PrepareContext, Transaction, StyleError));
        TestEqual(
            *FString::Printf(
                TEXT("'%s' implements IGV2UiStyleConsumer and emits exactly one prepared central-style operation"),
                *Class->GetName()),
            Transaction.GetOperations().Num(),
            1);
        if (Transaction.GetOperations().Num() == 1)
        {
            const GV2PresentationApply::FGV2PreparedOperationVariant& Operation =
                Transaction.GetOperations()[0];
            TestEqual(
                *FString::Printf(TEXT("'%s' emits CentralStyle kind"), *Class->GetName()),
                static_cast<uint8>(GV2PresentationApply::GetPreparedOperationKind(Operation)),
                static_cast<uint8>(GV2PresentationApply::EGV2PreparedOperationKind::CentralStyle));
            TestTrue(
                *FString::Printf(TEXT("'%s' central-style operation uses the inspected instance as target"), *Class->GetName()),
                Operation.Get<GV2PresentationApply::FPreparedCentralStyleOperation>().TargetWidget.Get() == Instance);
        }
    }

    TestTrue(TEXT("The reflection walk actually found style consumers to inspect"), InspectedCount > 0);

    Coordinator.EndSession();
    return true;
}

// PSC-10B: the hover popover is the one styled surface created OUTSIDE a screen's
// prepare/commit cycle. It must still receive finished values only -- resolved with its
// owner during Prepare, delivered on creation, and written through the same Apply facade.
// This drives the production entry point (UGV2RichTextPopoverWidgetBase::InitializePopover)
// against a real session snapshot and reads back the physical result.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2HoverPopoverStyledFromPreparedValuesTest,
    "GV2.Runtime.Presentation.HoverPopoverStyledFromPreparedValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2HoverPopoverStyledFromPreparedValuesTest::RunTest(const FString& Parameters)
{
    struct FSampleOverrideScope
    {
        FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = true; }
        ~FSampleOverrideScope() { FGV2SessionCoordinator::bTestForceIncludeSamplePackage = false; }
    } Scope;

    FGV2SessionCoordinator Coordinator;
    Coordinator.SetDocumentSink([](const FGV2UiDocumentViewModel&) -> bool { return true; });
    TestTrue(TEXT("Coordinator starts session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    const FGV2SessionContentSnapshot* Snapshot = Coordinator.GetContentSnapshot();
    TestNotNull(TEXT("Session publishes a content snapshot"), Snapshot);
    if (Snapshot == nullptr)
    {
        return false;
    }
    const FGV2PresentationPrepareContext PrepareContext(*Snapshot);
    const UGV2UiTheme* Theme = PrepareContext.GetTheme().Theme.Get();
    TestNotNull(TEXT("Snapshot owns a resolved Theme"), Theme);
    if (Theme == nullptr)
    {
        Coordinator.EndSession();
        return false;
    }

    // The owner's role, prepared exactly the way a screen's subtree walk prepares it.
    UGV2RichTextWidgetBase* Owner = NewObject<UGV2RichTextWidgetBase>();
    GV2PresentationApply::FGV2PreparedPresentationTransaction OwnerTransaction;
    FString StyleError;
    TestTrue(TEXT("Owner rich text subtree prepares"),
        GV2CentralStylePreparer::PrepareForSubtree(Owner, PrepareContext, OwnerTransaction, StyleError));
    TestEqual(TEXT("Owner rich text emits one central-style operation"), OwnerTransaction.GetOperations().Num(), 1);
    if (OwnerTransaction.GetOperations().Num() != 1)
    {
        Coordinator.EndSession();
        return false;
    }
    const GV2PresentationApply::FPreparedCentralStyleOperation& OwnerOperation =
        OwnerTransaction.GetOperations()[0].Get<GV2PresentationApply::FPreparedCentralStyleOperation>();
    const GV2PresentationApply::FPreparedRichTextStyle& OwnerStyle =
        OwnerOperation.Payload.Get<GV2PresentationApply::FPreparedRichTextStyle>();

    // The popover renderer class is a value the SNAPSHOT resolved, not something the hover
    // path loads: an unloaded soft reference here would mean a synchronous load at hover.
    TestEqual(TEXT("The prepared popover class is the one the snapshot resolved"),
        OwnerStyle.PopoverClass.Get(), PrepareContext.GetTheme().RichTextPopoverClass.Get());

    UGV2RichTextPopoverBoundTestWidget* Popover = NewObject<UGV2RichTextPopoverBoundTestWidget>();
    Popover->BuildBoundSubWidgets();

    FGV2RichTextHoverViewModel Model;
    FString TextError;
    TestTrue(TEXT("Hover title resolves through the pipeline"),
        UGV2TextPipeline::ResolveLiteralForAutomationTest(Theme, TEXT("Hover title"), NAME_None, Model.Title, TextError));
    TestTrue(TEXT("Hover description resolves through the pipeline"),
        UGV2TextPipeline::ResolveLiteralForAutomationTest(Theme, TEXT("Hover description"), NAME_None, Model.Description, TextError));

    TestTrue(TEXT("InitializePopover accepts a model plus its owner's prepared style"),
        Popover->InitializePopover(Model, OwnerStyle));

    TestEqual(TEXT("Popover background comes from the snapshot Theme"),
        Popover->ReadAppliedBackground().GetResourceName(), Theme->RichTextPopoverBackground.GetResourceName());
    TestEqual(TEXT("Popover padding comes from the snapshot Theme"),
        Popover->ReadAppliedPadding().Left, Theme->RichTextPopoverPadding.Left);
    const float ExpectedScale = Theme->EvaluateTextScale(Theme->ReferenceViewportHeight);
    TestEqual(TEXT("Popover max width follows the same viewport-derived scale as its text"),
        Popover->ReadAppliedMaxWidth(), Theme->RichTextPopoverMaxWidth * ExpectedScale);

    // A popover offered no prepared style must refuse rather than render unstyled.
    UGV2RichTextPopoverBoundTestWidget* Unstyled = NewObject<UGV2RichTextPopoverBoundTestWidget>();
    Unstyled->BuildBoundSubWidgets();
    TestFalse(TEXT("A popover with no prepared style refuses to initialize"),
        Unstyled->InitializePopover(Model, GV2PresentationApply::FPreparedRichTextStyle()));

    Coordinator.EndSession();
    return true;
}

// PSC-10B: the snapshot's Theme contract, in both directions.
//
// (a) A session whose configured Theme cannot be resolved must FAIL to build. The retired
//     UGV2UiThemeSettings::GetConfiguredTheme() silently substituted the core-minimal Theme,
//     which turned a misconfiguration into a session that runs on stand-in presentation.
//     ADR-0043 D1: the snapshot carries the authored authority or there is no session, and
//     the cold-start recovery screen is what the player sees instead.
//
// (b) The core-minimal Theme is still the TEXT fallback -- but as a value the snapshot
//     resolved and pinned, reachable only through FGV2PresentationPrepareContext. A text id
//     the authored Theme does not carry still resolves; nothing on the Commit-facing side
//     reaches UGV2UiTheme::GetCoreMinimalTheme() to make that happen.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SnapshotThemeResolutionContractTest,
    "GV2.Runtime.Presentation.SnapshotThemeResolutionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SnapshotThemeResolutionContractTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    const UGV2UiTheme* SessionTheme = PrepareContext->GetTheme().Theme.Get();
    const UGV2UiTheme* FallbackTheme = PrepareContext->GetTheme().FallbackTheme.Get();
    TestNotNull(TEXT("The snapshot pins the authored session Theme"), SessionTheme);
    TestNotNull(TEXT("The snapshot pins the core-minimal Theme as the text fallback"), FallbackTheme);
    if (SessionTheme == nullptr || FallbackTheme == nullptr)
    {
        return false;
    }

    // (b) An id the authored Theme genuinely does not carry -- asserted, not assumed, so
    // this cannot pass vacuously against a Theme that happens to define everything.
    FString FallbackOnlyId;
    for (const TPair<FString, FText>& Entry : FallbackTheme->TextCatalog)
    {
        if (!SessionTheme->TextCatalog.Contains(Entry.Key) && !SessionTheme->FallbackTextCatalog.Contains(Entry.Key))
        {
            FallbackOnlyId = Entry.Key;
            break;
        }
    }
    TestTrue(TEXT("The core-minimal Theme carries at least one text id the authored Theme does not"),
        !FallbackOnlyId.IsEmpty());
    if (!FallbackOnlyId.IsEmpty())
    {
        FGV2TextViewModel Resolved;
        FString ResolveError;
        TestTrue(
            *FString::Printf(TEXT("'%s' resolves through the snapshot's pinned fallback [Error: %s]"), *FallbackOnlyId, *ResolveError),
            UGV2TextPipeline::Resolve(FallbackOnlyId, {}, NAME_None, Resolved, ResolveError, PrepareContext));
        TestEqual(TEXT("The resolved text is the core-minimal entry"),
            Resolved.Text.ToString(), FallbackTheme->TextCatalog[FallbackOnlyId].ToString());
        TestTrue(TEXT("The fallback-resolved value still carries a full resolved presentation"),
            Resolved.bHasResolvedPresentation);
    }

    // Resolving without a context is refused outright -- there is no process-global Theme to
    // fall back to any more.
    {
        FGV2TextViewModel Unresolved;
        FString ResolveError;
        TestFalse(TEXT("Resolve without a prepare context is refused"),
            UGV2TextPipeline::Resolve(TEXT("core:text.common.ok"), {}, NAME_None, Unresolved, ResolveError, nullptr));
        TestTrue(*FString::Printf(TEXT("The refusal names the missing context [Error: %s]"), *ResolveError),
            ResolveError.Contains(TEXT("missing_prepare_context")));
    }

    // (a) No configured Theme -> no session. Restores the setting on every exit path.
    {
        struct FWithoutConfiguredTheme
        {
            FWithoutConfiguredTheme()
                : Settings(GetMutableDefault<UGV2UiThemeSettings>())
            {
                if (Settings != nullptr)
                {
                    Saved = Settings->ThemeAsset;
                    Settings->ThemeAsset = nullptr;
                }
            }
            ~FWithoutConfiguredTheme()
            {
                if (Settings != nullptr)
                {
                    Settings->ThemeAsset = Saved;
                }
            }
            UGV2UiThemeSettings* Settings = nullptr;
            TSoftObjectPtr<UGV2UiTheme> Saved;
        } NoTheme;

        GV2PresentationTestFixtures::FPrepareContextFixture ThemelessFixture;
        FString ThemelessError;
        TestFalse(TEXT("A session whose configured Theme is unset does not build"),
            ThemelessFixture.Initialize(ThemelessError));
        TestTrue(
            *FString::Printf(TEXT("The build fault is ThemeNotReady, not a core-minimal substitution [Error: %s]"), *ThemelessError),
            ThemelessError.Contains(TEXT("ThemeNotReady")));
    }

    return true;
}

// PSC-11 (ADR-0043 D2/D4): the direction of authority reads, measured on the production
// path rather than argued. Prepare reads the session snapshot; the window around the single
// Apply facade reads it zero times, for every operation kind a real screen produces --
// central style, image host, text, keyed collection and tabs alike.
//
// This is SECONDARY evidence and says so: the module graph is what makes an authority type
// unnameable inside GV2PresentationApply, and no counter can prove absence of a capability
// the module cannot link in the first place. What it adds is that the runtime agrees with
// the graph on a real document, not just on a synthetic transaction.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ApplyReadsNoAuthorityTest,
    "GV2.Runtime.Presentation.ApplyReadsNoAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ApplyReadsNoAuthorityTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(*FString::Printf(TEXT("Prepare context fixture is available: %s"), *ContextError), bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    // A subtree with a styled widget and a widget-default content reference, so Prepare has
    // both a Theme read and a catalog read to perform.
    UGV2NewHostAddedOnlyInTestWidget* Root = NewObject<UGV2NewHostAddedOnlyInTestWidget>();
    Root->WidgetTree = NewObject<UWidgetTree>(Root);
    UVerticalBox* RootBox = Root->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
    Root->WidgetTree->RootWidget = RootBox;
    UGV2SeparatorBoundTestWidget* Separator = NewObject<UGV2SeparatorBoundTestWidget>();
    Separator->BuildBoundSubWidgets();
    RootBox->AddChild(Separator);
    UGV2ProgressBarBoundTestWidget* ProgressBar = NewObject<UGV2ProgressBarBoundTestWidget>();
    ProgressBar->BuildBoundSubWidgets();
    RootBox->AddChild(ProgressBar);

    FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();

    GV2PresentationApply::FGV2PreparedPresentationTransaction Transaction;
    FString PrepareError;
    TestTrue(*FString::Printf(TEXT("The subtree prepares [Error: %s]"), *PrepareError),
        GV2CentralStylePreparer::PrepareForSubtree(Root, *PrepareContext, Transaction, PrepareError));

    const int32 PrepareAccesses = FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();
    TestTrue(
        *FString::Printf(TEXT("Prepare reads the session snapshot (%d accesses)"), PrepareAccesses),
        PrepareAccesses > 0);
    TestTrue(TEXT("Prepare produced operations to apply"), !Transaction.IsEmpty());

    // The measured window: nothing but the facade runs between the two reads of the counter.
    FGV2PresentationApplyResult ApplyResult;
    const bool bApplied = FGV2PresentationApply::Apply(Transaction, ApplyResult);
    const int32 ApplyAccesses = FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();

    TestTrue(*FString::Printf(TEXT("The facade applies the transaction [Error: %s]"), *ApplyResult.Error), bApplied);
    TestEqual(TEXT("Applying reads the session snapshot zero times"), ApplyAccesses, 0);
    TestEqual(TEXT("Every prepared operation was applied"),
        ApplyResult.AppliedOperationCount, Transaction.GetOperations().Num());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2DesignTimePreviewNoRuntimeAuthorityTest,
    "GV2.Runtime.Presentation.DesignTimePreviewNoRuntimeAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2DesignTimePreviewNoRuntimeAuthorityTest::RunTest(const FString& Parameters)
{
    UGV2SeparatorBoundTestWidget* Widget = NewObject<UGV2SeparatorBoundTestWidget>();
    Widget->BuildBoundSubWidgets();
    USizeBox* SizeBox = Cast<USizeBox>(Widget->WidgetTree->RootWidget);
    TestNotNull(TEXT("Design-time fixture has its real separator size box"), SizeBox);
    if (SizeBox == nullptr)
    {
        return false;
    }

    constexpr float SerializedThickness = 23.0f;
    SizeBox->SetHeightOverride(SerializedThickness);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct);
    TestTrue(TEXT("Fixture is executing as a UMG design-time preview"), Widget->IsDesignTime());

    FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();
    Widget->TakeWidget(); // real UUserWidget rebuild -> NativePreConstruct design-time branch
    const int32 AuthorityAccesses = FGV2PresentationPrepareContext::ConsumeAuthorityAccessCount();

    TestEqual(TEXT("Design-time NativePreConstruct reads no session authority"), AuthorityAccesses, 0);
    TestEqual(
        TEXT("Design-time preview keeps the widget's serialized physical value"),
        Widget->ReadAppliedThickness(),
        SerializedThickness);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionUiSchemaSnapshotIsolationTest,
    "GV2.Runtime.Session.UiSchemaSnapshotIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// CFC-04: the snapshot is the sole UI schema authority.
// This test builds Snapshot A with schema package root containing schema version A.
// Then it mutates the files on disk and builds Snapshot B with distinct constraints.
// It verifies:
// 1. Snapshot A and Snapshot B exhibit distinct schema-driven outcomes (top-level, binding, nested).
// 2. Re-processing against Snapshot A does not perform filesystem discovery (delta == 0).
bool FGV2SessionUiSchemaSnapshotIsolationTest::RunTest(const FString& Parameters)
{
    const FString TestSchemaDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CFC04IsolationTest/SchemaRoot"));
    IFileManager::Get().DeleteDirectory(*TestSchemaDir, false, true);
    IFileManager::Get().MakeDirectory(*TestSchemaDir, true);

    struct FDirectoryCleaner
    {
        FString Path;
        ~FDirectoryCleaner()
        {
            IFileManager::Get().DeleteDirectory(*Path, false, true);
        }
    } Cleaner{TestSchemaDir};

    auto BuildSnapshotWithExtraRoot = [this](
        const FString& ExtraSchemaDir,
        FGV2SessionContentSnapshot& OutSnapshot,
        FString& OutError) -> bool
    {
        const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
        const std::vector<std::filesystem::path> PackageRoots = {
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
            std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("rh")))),
        };

        std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
        const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
            GV2ContentHostSupport::ResolvePackageSetFromDirectories(PackageRoots, ResolveDiagnostics);
        if (!ResolvedSet.has_value())
        {
            OutError = TEXT("Unable to resolve package set");
            return false;
        }

        const GV2ContentCore::FBuildResult RepositoryBuild =
            BuildGV2RepositoryFromResolvedPackageSet(*ResolvedSet);
        if (!RepositoryBuild.IsSuccess())
        {
            OutError = TEXT("Unable to build repository");
            return false;
        }

        TArray<FGV2SchemaPackageRoot> SchemaRoots;
        SchemaRoots.Reserve(static_cast<int32>(ResolvedSet->OrderedSources.size()) + 1);
        for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedSet->OrderedSources)
        {
            SchemaRoots.Add(FGV2SchemaPackageRoot{
                UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
                UTF8_TO_TCHAR(Source.Root.string().c_str())});
        }
        SchemaRoots.Add(FGV2SchemaPackageRoot{TEXT("core"), ExtraSchemaDir});

        GV2RuntimeCore::FRuntimeFault Fault;
        if (!FGV2SessionContentCandidate::Build(
                RepositoryBuild.GetCandidate().GetReadHandle(),
                *ResolvedSet,
                SchemaRoots,
                {},
                OutSnapshot,
                Fault))
        {
            OutError = FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str()));
            return false;
        }
        return true;
    };

    const FString TopSchemaFile = FPaths::Combine(TestSchemaDir, TEXT("cfc04_isolation_top.schema.json5"));
    const FString NestedSchemaFile = FPaths::Combine(TestSchemaDir, TEXT("cfc04_isolation_nested.schema.json5"));
    const FString CompositeSchemaFile = FPaths::Combine(TestSchemaDir, TEXT("cfc04_isolation_composite.schema.json5"));

    const FString TopSchemaJsonA = TEXT("{\n")
        TEXT("  id: \"core:schema.ui_field.cfc04_isolation_top.v1\",\n")
        TEXT("  schema_domain: \"ui_field\",\n")
        TEXT("  schema_version: 1,\n")
        TEXT("  root: {\n")
        TEXT("    kind: \"object\",\n")
        TEXT("    fields: {\n")
        TEXT("      number_val: { kind: \"number\", required: true, min: 10.0, max: 20.0 },\n")
        TEXT("      items: {\n")
        TEXT("        kind: \"array\",\n")
        TEXT("        required: true,\n")
        TEXT("        keyed_by: \"key\",\n")
        TEXT("        items: {\n")
        TEXT("          kind: \"object\",\n")
        TEXT("          fields: {\n")
        TEXT("            key: { kind: \"key\", required: true },\n")
        TEXT("            binding: { kind: \"binding\", required: true }\n")
        TEXT("          }\n")
        TEXT("        }\n")
        TEXT("      }\n")
        TEXT("    }\n")
        TEXT("  }\n")
        TEXT("}\n");

    const FString NestedSchemaJsonA = TEXT("{\n")
        TEXT("  id: \"core:schema.ui_field.cfc04_isolation_nested.v1\",\n")
        TEXT("  schema_domain: \"ui_field\",\n")
        TEXT("  schema_version: 1,\n")
        TEXT("  root: {\n")
        TEXT("    kind: \"object\",\n")
        TEXT("    fields: {\n")
        TEXT("      tag: { kind: \"key\", required: true },\n")
        TEXT("      flag_a: { kind: \"bool\", required: true }\n")
        TEXT("    }\n")
        TEXT("  }\n")
        TEXT("}\n");

    const FString CompositeSchemaJson = TEXT("{\n")
        TEXT("  id: \"core:schema.ui_field.cfc04_isolation_composite.v1\",\n")
        TEXT("  schema_domain: \"ui_field\",\n")
        TEXT("  schema_version: 1,\n")
        TEXT("  root: {\n")
        TEXT("    kind: \"object\",\n")
        TEXT("    fields: {\n")
        TEXT("      fields: { kind: \"screen_fields\", required: true }\n")
        TEXT("    }\n")
        TEXT("  }\n")
        TEXT("}\n");

    TestTrue(TEXT("Write TopSchemaJsonA"), FFileHelper::SaveStringToFile(TopSchemaJsonA, *TopSchemaFile));
    TestTrue(TEXT("Write NestedSchemaJsonA"), FFileHelper::SaveStringToFile(NestedSchemaJsonA, *NestedSchemaFile));
    TestTrue(TEXT("Write CompositeSchemaJson"), FFileHelper::SaveStringToFile(CompositeSchemaJson, *CompositeSchemaFile));

    FGV2SessionContentSnapshot SnapshotA;
    FString ErrorA;
    const bool bBuiltA = BuildSnapshotWithExtraRoot(TestSchemaDir, SnapshotA, ErrorA);
    TestTrue(*FString::Printf(TEXT("Build Snapshot A [Error: %s]"), *ErrorA), bBuiltA);
    if (!bBuiltA)
    {
        return false;
    }
    const FGV2PresentationPrepareContext PrepareContextA(SnapshotA);

    const FString TopSchemaJsonB = TEXT("{\n")
        TEXT("  id: \"core:schema.ui_field.cfc04_isolation_top.v1\",\n")
        TEXT("  schema_domain: \"ui_field\",\n")
        TEXT("  schema_version: 1,\n")
        TEXT("  root: {\n")
        TEXT("    kind: \"object\",\n")
        TEXT("    fields: {\n")
        TEXT("      number_val: { kind: \"number\", required: true, min: 100.0, max: 200.0 },\n")
        TEXT("      items: {\n")
        TEXT("        kind: \"array\",\n")
        TEXT("        required: true,\n")
        TEXT("        keyed_by: \"key\",\n")
        TEXT("        items: {\n")
        TEXT("          kind: \"object\",\n")
        TEXT("          fields: {\n")
        TEXT("            key: { kind: \"key\", required: true },\n")
        TEXT("            text: { kind: \"text\", required: true }\n")
        TEXT("          }\n")
        TEXT("        }\n")
        TEXT("      }\n")
        TEXT("    }\n")
        TEXT("  }\n")
        TEXT("}\n");

    const FString NestedSchemaJsonB = TEXT("{\n")
        TEXT("  id: \"core:schema.ui_field.cfc04_isolation_nested.v1\",\n")
        TEXT("  schema_domain: \"ui_field\",\n")
        TEXT("  schema_version: 1,\n")
        TEXT("  root: {\n")
        TEXT("    kind: \"object\",\n")
        TEXT("    fields: {\n")
        TEXT("      tag: { kind: \"key\", required: true },\n")
        TEXT("      flag_b: { kind: \"bool\", required: true }\n")
        TEXT("    }\n")
        TEXT("  }\n")
        TEXT("}\n");

    TestTrue(TEXT("Overwrite TopSchemaJsonB"), FFileHelper::SaveStringToFile(TopSchemaJsonB, *TopSchemaFile));
    TestTrue(TEXT("Overwrite NestedSchemaJsonB"), FFileHelper::SaveStringToFile(NestedSchemaJsonB, *NestedSchemaFile));

    FGV2SessionContentSnapshot SnapshotB;
    FString ErrorB;
    const bool bBuiltB = BuildSnapshotWithExtraRoot(TestSchemaDir, SnapshotB, ErrorB);
    TestTrue(*FString::Printf(TEXT("Build Snapshot B [Error: %s]"), *ErrorB), bBuiltB);
    if (!bBuiltB)
    {
        return false;
    }
    const FGV2PresentationPrepareContext PrepareContextB(SnapshotB);

    AddExpectedErrorPlain(TEXT("rejected (closed schema)"), EAutomationExpectedErrorFlags::Contains, 4);
    AddExpectedErrorPlain(TEXT("ProjectMaterializedValue failed"), EAutomationExpectedErrorFlags::Contains, 2);

    using FObject = GV2RuntimeCore::FValue::FObject;
    using FArray = GV2RuntimeCore::FValue::FArray;

    // 1. Top-level and binding outcome checks
    GV2RuntimeCore::FScreenRequest ReqDocA;
    ReqDocA.ScreenId = "core:screen.test_screen";
    GV2RuntimeCore::FScreenField FieldDocA;
    FieldDocA.FieldId = "top_field";
    FieldDocA.SchemaId = "core:schema.ui_field.cfc04_isolation_top.v1";
    FObject ObjA;
    ObjA["number_val"] = GV2RuntimeCore::FValue(15.0);
    FObject BtnA;
    BtnA["key"] = GV2RuntimeCore::FValue(std::string("btn1"));
    BtnA["binding"] = GV2RuntimeCore::FValue(std::string("core:command.test"));
    ObjA["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(BtnA)});
    FieldDocA.Value = GV2RuntimeCore::FValue(MoveTemp(ObjA));
    ReqDocA.Fields.push_back(MoveTemp(FieldDocA));

    GV2RuntimeCore::FScreenRequest ReqDocB;
    ReqDocB.ScreenId = "core:screen.test_screen";
    GV2RuntimeCore::FScreenField FieldDocB;
    FieldDocB.FieldId = "top_field";
    FieldDocB.SchemaId = "core:schema.ui_field.cfc04_isolation_top.v1";
    FObject ObjB;
    ObjB["number_val"] = GV2RuntimeCore::FValue(150.0);
    FObject BtnB;
    BtnB["key"] = GV2RuntimeCore::FValue(std::string("btn1"));
    FObject TextB;
    TextB["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.common.ok"));
    BtnB["text"] = GV2RuntimeCore::FValue(TextB);
    ObjB["items"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(BtnB)});
    FieldDocB.Value = GV2RuntimeCore::FValue(MoveTemp(ObjB));
    ReqDocB.Fields.push_back(MoveTemp(FieldDocB));

    // Binding extraction
    TArray<FGV2UiBindingDefinition> DefsA;
    TestTrue(TEXT("Snapshot A extracts binding for Doc A"),
        GV2ScreenFieldMaterializer::PrepareBindingDefinitions(PrepareContextA, ReqDocA, DefsA));
    TestEqual(TEXT("Snapshot A extracted 1 binding definition"), DefsA.Num(), 1);

    TArray<FGV2UiBindingDefinition> DefsB;
    TestFalse(TEXT("Snapshot B rejects Doc A in binding extraction"),
        GV2ScreenFieldMaterializer::PrepareBindingDefinitions(PrepareContextB, ReqDocA, DefsB));

    TArray<FGV2UiBindingDefinition> DefsA2;
    TestFalse(TEXT("Snapshot A rejects Doc B in binding extraction"),
        GV2ScreenFieldMaterializer::PrepareBindingDefinitions(PrepareContextA, ReqDocB, DefsA2));

    // BuildFields
    TArray<FGV2UiBindingHandle> HandlesA;
    HandlesA.Add(FGV2UiBindingHandle::Create(TEXT("core:command.test")));
    TArray<FGV2ScreenFieldValue> FieldsA;
    TestTrue(TEXT("Snapshot A builds fields for Doc A"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextA, ReqDocA, HandlesA, FieldsA));

    TArray<FGV2ScreenFieldValue> FieldsA_on_B;
    TestFalse(TEXT("Snapshot B rejects fields for Doc A (number_val out of range, items invalid)"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextB, ReqDocA, HandlesA, FieldsA_on_B));

    TArray<FGV2ScreenFieldValue> FieldsB;
    TestTrue(TEXT("Snapshot B builds fields for Doc B"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextB, ReqDocB, {}, FieldsB));

    TArray<FGV2ScreenFieldValue> FieldsB_on_A;
    TestFalse(TEXT("Snapshot A rejects fields for Doc B (number_val 150 > max 20)"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextA, ReqDocB, {}, FieldsB_on_A));

    // 2. Nested schema outcome checks
    FObject InnerObjA;
    InnerObjA["tag"] = GV2RuntimeCore::FValue(std::string("tag1"));
    InnerObjA["flag_a"] = GV2RuntimeCore::FValue(true);

    FObject EnvelopeA;
    EnvelopeA["field_id"] = GV2RuntimeCore::FValue(std::string("nested_item"));
    EnvelopeA["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.cfc04_isolation_nested.v1"));
    EnvelopeA["value"] = GV2RuntimeCore::FValue(InnerObjA);

    FObject CompositeObjA;
    CompositeObjA["fields"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(EnvelopeA)});

    GV2RuntimeCore::FScreenRequest NestedReqA;
    NestedReqA.ScreenId = "core:screen.test_screen";
    GV2RuntimeCore::FScreenField CompFieldA;
    CompFieldA.FieldId = "comp_field";
    CompFieldA.SchemaId = "core:schema.ui_field.cfc04_isolation_composite.v1";
    CompFieldA.Value = GV2RuntimeCore::FValue(MoveTemp(CompositeObjA));
    NestedReqA.Fields.push_back(MoveTemp(CompFieldA));

    TArray<FGV2ScreenFieldValue> NestedBuiltA;
    TestTrue(TEXT("Snapshot A builds nested field containing flag_a"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextA, NestedReqA, {}, NestedBuiltA));

    TArray<FGV2ScreenFieldValue> NestedBuiltA_on_B;
    TestFalse(TEXT("Snapshot B rejects nested field containing flag_a (requires flag_b)"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextB, NestedReqA, {}, NestedBuiltA_on_B));

    FObject InnerObjB;
    InnerObjB["tag"] = GV2RuntimeCore::FValue(std::string("tag1"));
    InnerObjB["flag_b"] = GV2RuntimeCore::FValue(true);

    FObject EnvelopeB;
    EnvelopeB["field_id"] = GV2RuntimeCore::FValue(std::string("nested_item"));
    EnvelopeB["schema_id"] = GV2RuntimeCore::FValue(std::string("core:schema.ui_field.cfc04_isolation_nested.v1"));
    EnvelopeB["value"] = GV2RuntimeCore::FValue(InnerObjB);

    FObject CompositeObjB;
    CompositeObjB["fields"] = GV2RuntimeCore::FValue(FArray{GV2RuntimeCore::FValue(EnvelopeB)});

    GV2RuntimeCore::FScreenRequest NestedReqB;
    NestedReqB.ScreenId = "core:screen.test_screen";
    GV2RuntimeCore::FScreenField CompFieldB;
    CompFieldB.FieldId = "comp_field";
    CompFieldB.SchemaId = "core:schema.ui_field.cfc04_isolation_composite.v1";
    CompFieldB.Value = GV2RuntimeCore::FValue(MoveTemp(CompositeObjB));
    NestedReqB.Fields.push_back(MoveTemp(CompFieldB));

    TArray<FGV2ScreenFieldValue> NestedBuiltB;
    TestTrue(TEXT("Snapshot B builds nested field containing flag_b"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextB, NestedReqB, {}, NestedBuiltB));

    TArray<FGV2ScreenFieldValue> NestedBuiltB_on_A;
    TestFalse(TEXT("Snapshot A rejects nested field containing flag_b (requires flag_a)"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextA, NestedReqB, {}, NestedBuiltB_on_A));

    // 3. Discovery count check on re-processing
    const int32 DiscoveryCountBefore = FGV2UiSchemaCache::GetGlobalDiscoveryCount();

    TArray<FGV2UiBindingDefinition> ReplayDefsA;
    TestTrue(TEXT("Replay Snapshot A binding extraction"),
        GV2ScreenFieldMaterializer::PrepareBindingDefinitions(PrepareContextA, ReqDocA, ReplayDefsA));
    TArray<FGV2ScreenFieldValue> ReplayFieldsA;
    TestTrue(TEXT("Replay Snapshot A build fields"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextA, ReqDocA, HandlesA, ReplayFieldsA));
    TArray<FGV2ScreenFieldValue> ReplayNestedA;
    TestTrue(TEXT("Replay Snapshot A nested build fields"),
        GV2ScreenFieldMaterializer::BuildFields(PrepareContextA, NestedReqA, {}, ReplayNestedA));

    const int32 DiscoveryCountAfter = FGV2UiSchemaCache::GetGlobalDiscoveryCount();
    TestEqual(TEXT("Re-processing against Snapshot A does not read filesystem or invoke discovery"),
        DiscoveryCountAfter, DiscoveryCountBefore);

    return true;
}

#endif
