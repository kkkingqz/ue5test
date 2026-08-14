#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2FilesystemContentSourceProvider.h"
#include "Application/GV2RepositoryPublisher.h"
#include "Application/GV2SessionCoordinator.h"
#include "Bridge/GV2UiBindingRegistry.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

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
            "@Scripts/" + std::string(TCHAR_TO_UTF8(*RelativePath)),
            std::string(Utf8.Get(), Utf8.Length())});
    }
    return Sources;
}

GV2ContentCore::FRepositoryReadHandle MakeTestPinnedRepositoryFrom(const TCHAR* FixtureRelativePath)
{
    const FString PackageRoot = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore"), FixtureRelativePath);
    GV2ContentCore::FBuildResult Result = BuildGV2RepositoryFromDirectory(PackageRoot);
    if (Result.IsFailure())
    {
        return GV2ContentCore::FRepositoryReadHandle();
    }
    return Result.GetCandidate().GetReadHandle();
}

GV2ContentCore::FRepositoryReadHandle MakeTestPinnedRepository()
{
    return MakeTestPinnedRepositoryFrom(TEXT("valid/core"));
}

// BuildRepository() requires a package literally named "core" at load_index 0
// (GameDataRepositoryContract.md); the shared "empty_core" fixture directory
// doesn't match that name, so build a distinct, minimal empty "core" package
// in-memory instead of going through the filesystem discovery convention.
GV2ContentCore::FRepositoryReadHandle MakeEmptyCoreRepository()
{
    using namespace GV2ContentCore;
    const FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    const FBuildOptions Options;
    FBuildResult Result = BuildRepository({EmptyCore}, Options);
    if (Result.IsFailure())
    {
        return FRepositoryReadHandle();
    }
    return Result.GetCandidate().GetReadHandle();
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
        Host.Start(23, RuntimeSources, Fault));
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
                return Source.Name == "@Scripts/resources/service.lua";
            }),
        MissingSource.end());
    GV2RuntimeCore::FRuntimeSession MissingSourceHost;
    TestFalse(
        TEXT("Manifest rejects a missing declared module source"),
        MissingSourceHost.Start(1, MissingSource, Fault));
    TestEqual(
        TEXT("Missing module source has a stable fault code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleSourceMissing")));

    std::vector<GV2RuntimeCore::FRuntimeSource> HiddenDependency = Sources;
    for (GV2RuntimeCore::FRuntimeSource& Source : HiddenDependency)
    {
        if (Source.Name == "@Scripts/boundary/entrypoints.lua")
        {
            Source.Text.insert(
                0,
                "local hidden_dependency = require(\"core:module.resources.service\")\n");
        }
    }
    GV2RuntimeCore::FRuntimeSession HiddenDependencyHost;
    TestFalse(
        TEXT("Module cannot import an undeclared dependency"),
        HiddenDependencyHost.Start(1, HiddenDependency, Fault));
    TestEqual(
        TEXT("Hidden dependency fails during module initialization"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleLoadError")));
    TestTrue(
        TEXT("Hidden dependency diagnostic identifies the manifest violation"),
        FString(UTF8_TO_TCHAR(Fault.Message.c_str())).Contains(TEXT("not declared")));

    std::vector<GV2RuntimeCore::FRuntimeSource> UnlistedSource = Sources;
    UnlistedSource.push_back({"@Scripts/gameplay/unlisted.lua", "return {}"});
    GV2RuntimeCore::FRuntimeSession UnlistedSourceHost;
    TestFalse(
        TEXT("Unlisted Lua source is rejected"),
        UnlistedSourceHost.Start(1, UnlistedSource, Fault));
    TestEqual(
        TEXT("Unlisted source has a stable fault code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleSourceUnlisted")));

    const std::vector<GV2RuntimeCore::FRuntimeSource> CyclicSources = {
        {
            "@Scripts/bootstrap/manifest.lua",
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
        {"@Scripts/runtime/a.lua", "return {}"},
        {"@Scripts/runtime/b.lua", "return {}"},
    };
    GV2RuntimeCore::FRuntimeSession CyclicHost;
    TestFalse(
        TEXT("Cyclic module dependencies are rejected"),
        CyclicHost.Start(1, CyclicSources, Fault));
    TestEqual(
        TEXT("Dependency cycle has a stable fault code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaModuleDependencyCycle")));
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
    TestTrue(TEXT("Coordinator starts its Lua VM"), Coordinator.StartSession(MakeTestPinnedRepository()));
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
    TestTrue(TEXT("Coordinator starts for schema validation"), Coordinator.StartSession(MakeTestPinnedRepository()));

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

GV2ContentCore::FRepositoryReadHandle MakeEmptyCoreRepository()
{
    using namespace GV2ContentCore;
    const FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    const FBuildOptions Options;
    FBuildResult Result = BuildRepository({EmptyCore}, Options);
    if (Result.IsFailure())
    {
        return FRepositoryReadHandle();
    }
    return Result.GetCandidate().GetReadHandle();
}

GV2ContentCore::FBuildResult MakeTestBuildResultFrom(const TCHAR* FixtureRelativePath)
{
    const FString PackageRoot = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore"), FixtureRelativePath);
    return BuildGV2RepositoryFromDirectory(PackageRoot);
}

GV2ContentCore::FBuildResult MakeEmptyCoreBuildResult()
{
    using namespace GV2ContentCore;
    const FPackageDescriptor EmptyCore("core", "core", 0u, {}, {});
    const FBuildOptions Options;
    return BuildRepository({EmptyCore}, Options);
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
    TestTrue(TEXT("Portable runtime session starts"), Host.Start(1, RuntimeSources, Fault));
    TestTrue(TEXT("Portable runtime executes simple script"), Host.Execute(1, Fault));
    Host.Stop();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiBindingRegistryTest,
    "GV2.Runtime.UI.BindingRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiBindingRegistryTest::RunTest(const FString& Parameters)
{
    FGV2UiBindingRegistry Registry;
    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Registry publishes valid binding definition"),
        Registry.PublishBindings(
            TEXT("ui@1:1"),
            1,
            {MakeBindingDefinition(TEXT("start"), TEXT("core:command.menu.start"))},
            Handles));
    if (!TestEqual(TEXT("Registry returns one handle"), Handles.Num(), 1))
    {
        return false;
    }

    const FGV2UiBindingRecord* Record = Registry.FindRecord(Handles[0]);
    if (!TestNotNull(TEXT("Handle resolves to record"), Record))
    {
        return false;
    }
    TestEqual(TEXT("Record retains ui_instance_id"), Record->UiInstanceId, FString(TEXT("ui@1:1")));
    TestEqual(TEXT("Record retains element_id"), Record->ElementId, FString(TEXT("start")));
    TestEqual(TEXT("Record retains command_id"), Record->CommandId, FString(TEXT("core:command.menu.start")));

    Registry.UnpublishUiInstance(TEXT("ui@1:1"));
    TestNull(TEXT("Unpublished handle fails resolution"), Registry.FindRecord(Handles[0]));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SessionCoordinatorIngressTest,
    "GV2.Runtime.Session.IngressValidationAndDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SessionCoordinatorIngressTest::RunTest(const FString& Parameters)
{
    FGV2SessionCoordinator Coordinator(16);
    TestTrue(TEXT("Session starts"), Coordinator.StartSession(MakeTestPinnedRepository()));

    TArray<FGV2UiBindingHandle> Handles;
    TestTrue(
        TEXT("Binding is published"),
        Coordinator.PublishUiBindings(
            TEXT("ui@1:1"),
            1,
            {MakeBindingDefinition(TEXT("start"), TEXT("core:command.menu.start"))},
            Handles));
    if (Handles.Num() != 1)
    {
        return false;
    }

    int32 SinkCalls = 0;
    FGV2UiIngressItem LastItem;
    Coordinator.SetInteractionSink(
        [&SinkCalls, &LastItem](const FGV2UiIngressItem& Item)
        {
            ++SinkCalls;
            LastItem = Item;
        });

    TestEqual(
        TEXT("Valid submission succeeds"),
        Coordinator.SubmitUiInteraction(Handles[0], {{"choice", "new_game"}}),
        EGV2SubmitUiInteractionResult::Accepted);
    TestEqual(TEXT("Ingress queue holds item before dispatch"), Coordinator.GetQueuedIngressCount(), 1);
    TestEqual(TEXT("Dispatch flushes queued item"), Coordinator.DispatchQueuedIngress(), 1);
    TestEqual(TEXT("Sink receives dispatched item"), SinkCalls, 1);
    TestEqual(TEXT("Dispatched item retains session_generation"), LastItem.SessionGeneration, 1u);
    TestEqual(TEXT("Dispatched item retains ui_instance_id"), LastItem.UiInstanceId, FString(TEXT("ui@1:1")));
    TestEqual(TEXT("Dispatched item retains command_id"), LastItem.CommandId, FString(TEXT("core:command.menu.start")));

    FGV2UiBindingHandle StaleHandle = Handles[0];
    Coordinator.UnpublishUiInstance(TEXT("ui@1:1"));
    TestEqual(
        TEXT("Stale handle is rejected"),
        Coordinator.SubmitUiInteraction(StaleHandle, {}),
        EGV2SubmitUiInteractionResult::InvalidHandle);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2RuntimeIngressCapacityTest,
    "GV2.Runtime.Ingress.BoundedCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2RuntimeIngressCapacityTest::RunTest(const FString& Parameters)
{
    FGV2SessionCoordinator Coordinator(0);
    TestTrue(TEXT("Coordinator starts for capacity validation"), Coordinator.StartSession(MakeTestPinnedRepository()));

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
    const GV2ContentCore::FBuildResult ResultA = MakeTestBuildResultFrom(TEXT("valid/core"));
    const GV2ContentCore::FBuildResult ResultB = MakeEmptyCoreBuildResult();
    if (!TestTrue(TEXT("Fixture A builds"), ResultA.IsSuccess())
        || !TestTrue(TEXT("Fixture B builds"), ResultB.IsSuccess()))
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
    TestTrue(TEXT("Session starts pinned to repository A"), Coordinator.StartSession(ReadHandleA));
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
    TestTrue(TEXT("Restarted session starts pinned to Publisher current (B)"), Coordinator.StartSession(Publisher.GetCurrent()));
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
        Coordinator.StartSession(GV2ContentCore::FRepositoryReadHandle()));
    TestFalse(TEXT("No Lua VM was started"), Coordinator.IsLuaVmStarted());
    TestFalse(TEXT("Session is not ready"), Coordinator.GetStatus().bIsReady);
    TestEqual(
        TEXT("Session state is Failed"),
        Coordinator.GetStatus().SessionState,
        EGV2SessionState::Failed);

    // 2. Start valid session, then call StartSession with invalid handle to ensure full teardown
    TestTrue(TEXT("Start valid session"), Coordinator.StartSession(MakeTestPinnedRepository()));
    TestTrue(TEXT("Lua VM is started for valid session"), Coordinator.IsLuaVmStarted());
    TestTrue(TEXT("Session is ready"), Coordinator.GetStatus().bIsReady);

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=RepositoryNotReady"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    TestFalse(
        TEXT("StartSession rejects invalid handle on active session"),
        Coordinator.StartSession(GV2ContentCore::FRepositoryReadHandle()));
    TestFalse(TEXT("Active VM is stopped on failed StartSession"), Coordinator.IsLuaVmStarted());
    TestFalse(TEXT("Coordinator is not ready after failed StartSession"), Coordinator.GetStatus().bIsReady);
    TestFalse(TEXT("Pinned repository handle is cleared on failure"), Coordinator.GetPinnedRepository().IsValid());
    TestEqual(
        TEXT("Session state is Failed after failed StartSession on active session"),
        Coordinator.GetStatus().SessionState,
        EGV2SessionState::Failed);

    return true;
}

#endif
