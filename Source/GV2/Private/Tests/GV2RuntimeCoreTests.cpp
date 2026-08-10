#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2SessionCoordinator.h"
#include "Bridge/GV2UiBindingRegistry.h"
#include "GV2RuntimeCore/GV2RuntimeSession.h"

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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2PortableRuntimeTest,
    "GV2.Runtime.Lua.SafeEnvironmentAndProtectedEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2PortableRuntimeTest::RunTest(const FString& Parameters)
{
    GV2RuntimeCore::FRuntimeSession Host;
    GV2RuntimeCore::FRuntimeFault Fault;
    TestTrue(
        TEXT("Lua VM starts with the restricted test runtime"),
        Host.StartTestRuntime(23, Fault));
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

    Item.CommandId = "core:command.test.force_error";
    Item.Sequence = 3;
    TestFalse(
        TEXT("Lua runtime error is returned as a structured fault"),
        Host.DispatchSemanticInput(Item, Fault));
    TestEqual(
        TEXT("Runtime fault has stable code"),
        FString(UTF8_TO_TCHAR(Fault.Code.c_str())),
        FString(TEXT("LuaDispatchError")));
    TestFalse(TEXT("Lua VM is idle after failed protected call"), Host.IsExecuting());

    Item.CommandId = "core:command.test.lua_round_trip";
    Item.Sequence = 4;
    TestTrue(
        TEXT("Stack is restored and the next protected call can run"),
        Host.DispatchSemanticInput(Item, Fault));

    Host.Stop();
    TestFalse(TEXT("Lua VM stops deterministically"), Host.IsStarted());
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

    TArray<FGV2UiBindingHandle> RevisionTwoHandles;
    TestTrue(
        TEXT("New valid revision replaces the binding set"),
        Registry.PublishBindings(
            TEXT("ui@11:1"),
            2,
            {MakeBindingDefinition(TEXT("second"), TEXT("core:command.test.second"))},
            RevisionTwoHandles));
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
    TestTrue(TEXT("Coordinator starts its Lua VM"), Coordinator.StartTestSession());
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
            TestTrue(TEXT("Runtime execution flag is active inside the sink"), Coordinator.IsExecutingRuntime());
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
    TestTrue(TEXT("Coordinator starts for schema validation"), Coordinator.StartTestSession());

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
    TestTrue(TEXT("Coordinator starts for capacity validation"), Coordinator.StartTestSession());

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

#endif
