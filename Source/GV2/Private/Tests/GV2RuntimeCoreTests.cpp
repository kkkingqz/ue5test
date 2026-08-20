#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2RepositoryPublisher.h"
#include "Application/GV2SessionCoordinator.h"
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
    const std::vector<GV2RuntimeCore::FRuntimeSource> RuntimeSources = LoadTestRuntimeSources();
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
    StartRequest.CommandId = "core:command.debug.start";
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
            5);
        const GV2RuntimeCore::FScreenField* DescriptionField = nullptr;
        const GV2RuntimeCore::FScreenField* ButtonsField = nullptr;
        const GV2RuntimeCore::FScreenField* CheckboxField = nullptr;
        const GV2RuntimeCore::FScreenField* InputField = nullptr;
        const GV2RuntimeCore::FScreenField* DropdownField = nullptr;
        for (const GV2RuntimeCore::FScreenField& Field : PendingScreen->Fields)
        {
            if (Field.FieldId == "description") DescriptionField = &Field;
            if (Field.FieldId == "buttons") ButtonsField = &Field;
            if (Field.FieldId == "checkbox") CheckboxField = &Field;
            if (Field.FieldId == "player_name") InputField = &Field;
            if (Field.FieldId == "class_select") DropdownField = &Field;
        }
        TestNotNull(TEXT("Generic request contains description field"), DescriptionField);
        TestNotNull(TEXT("Generic request contains buttons field"), ButtonsField);
        TestNotNull(TEXT("Generic request contains checkbox field"), CheckboxField);
        TestNotNull(TEXT("Generic request contains input field"), InputField);
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
            TEXT("Checkbox field keeps its schema identity"),
            FString(UTF8_TO_TCHAR(CheckboxField != nullptr ? CheckboxField->SchemaId.c_str() : "")),
            FString(TEXT("core:schema.ui_field.checkbox.v1")));
        TestEqual(
            TEXT("Input field keeps its schema identity"),
            FString(UTF8_TO_TCHAR(InputField != nullptr ? InputField->SchemaId.c_str() : "")),
            FString(TEXT("core:schema.ui_field.input_field.v1")));
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
                                    FString(TEXT("core:item.weapon.iron_sword")));
                            }
                        }
                    }
                }
            }
        }

        if (InputField != nullptr && std::holds_alternative<GV2RuntimeCore::FValue::FObject>(InputField->Value.Data))
        {
            const auto& InputObj = std::get<GV2RuntimeCore::FValue::FObject>(InputField->Value.Data);
            auto ValueIt = InputObj.find("value");
            if (ValueIt != InputObj.end() && std::holds_alternative<std::string>(ValueIt->second.Data))
            {
                TestEqual(
                    TEXT("Input field initial value was populated from repository definition"),
                    FString(UTF8_TO_TCHAR(std::get<std::string>(ValueIt->second.Data).c_str())),
                    FString(TEXT("core:item.weapon.iron_sword")));
            }
        }
    }
    TestTrue(
        TEXT("Pending presentation is consumed exactly once"),
        Host.TakePendingScreen(PendingScreen, Fault));
    TestFalse(TEXT("No presentation remains after consumption"), PendingScreen.has_value());

    Item.CommandId = "core:command.test.force_error";
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeIngressDispatchTest,
    "GV2.Runtime.Ingress.FifoAndNonReentrantDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeIngressDispatchTest::RunTest(const FString& Parameters)
{
    FGV2SessionCoordinator Coordinator;
    TestTrue(TEXT("Coordinator starts its Lua VM"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Lua VM belongs to the active session"), Coordinator.IsLuaVmStarted());

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Two bindings are published"),
        Coordinator.PublishUiBindings(
            TEXT("ui@1:1"),
            1,
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
    FGV2SessionCoordinator Coordinator;
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
        Coordinator.PublishUiBindings(TEXT("ui@1:1"), 1, {Definition}, Handles));
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
    FGV2SessionCoordinator Coordinator(0);
    TestTrue(TEXT("Coordinator starts for capacity validation"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Binding is published for capacity test"),
        Coordinator.PublishUiBindings(
            TEXT("ui@1:1"),
            1,
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
    FGV2SessionCoordinator Coordinator(4);

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

    // 2. Start valid session, then call StartSession with invalid handle to ensure full teardown
    TestTrue(TEXT("Start valid session"), Coordinator.StartSession(MakeFrozenCoreFixturePinnedRepository(*this), 1));
    TestTrue(TEXT("Lua VM is started for valid session"), Coordinator.IsLuaVmStarted());
    TestTrue(TEXT("Session is ready"), Coordinator.GetStatus().bIsReady);

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    TestFalse(
        TEXT("StartSession rejects invalid handle on active session"),
        Coordinator.StartSession(GV2ContentCore::FRepositoryReadHandle(), 0));
    TestFalse(TEXT("Active VM is stopped on failed StartSession"), Coordinator.IsLuaVmStarted());
    TestFalse(TEXT("Coordinator is not ready after failed StartSession"), Coordinator.GetStatus().bIsReady);
    TestFalse(TEXT("Pinned repository handle is cleared on failure"), Coordinator.GetPinnedRepository().IsValid());
    TestEqual(
        TEXT("Session state is Failed after failed StartSession on active session"),
        Coordinator.GetStatus().SessionState,
        EGV2SessionState::Failed);

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

#endif
