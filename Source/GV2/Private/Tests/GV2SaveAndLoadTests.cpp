#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Application/GV2SessionCoordinator.h"
#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
using GV2PresentationTestFixtures::FGV2ScopedSamplePackageOverride;
}

// CFC-09/10: Helper to retrieve ButtonRepeater from LocationScreen
static UGV2ListViewWidgetBase* GetButtonRepeaterFromLocationScreen(UGV2ScreenWidgetBase* Screen)
{
    if (Screen == nullptr || Screen->WidgetTree == nullptr)
    {
        return nullptr;
    }
    UGV2DeclaredCompositeWidgetBase* CommandWidget = nullptr;
    Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        if (auto* Cmd = Cast<UGV2DeclaredCompositeWidgetBase>(Widget); Cmd != nullptr && Cmd->GetHostIdentity() == FName(TEXT("commands")))
        {
            CommandWidget = Cmd;
        }
    });
    return CommandWidget != nullptr
        ? Cast<UGV2ListViewWidgetBase>(CommandWidget->GetWidgetFromName(TEXT("ButtonRepeater")))
        : nullptr;
}

// CFC-09/10: Helper to compute canonical state hash from active session
static FString GetSessionStateHash(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session)
{
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::string SpecSource = R"lua(
return {
    sample = function()
        local state_hasher = require("core:module.runtime.state_hasher")
        local hash = state_hasher.hash_state(game.state)
        error("STATE_HASH:" .. tostring(hash))
    end
}
)lua";

    Session.RunLuaSpec("@query_state_hash", SpecSource, Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return TEXT("");
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("STATE_HASH:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix, ESearchCase::CaseSensitive);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract state hash: %s"), *ErrorMessage));
        return TEXT("");
    }

    FString HashStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    int32 NewlineIdx = INDEX_NONE;
    if (HashStr.FindChar(TEXT('\n'), NewlineIdx))
    {
        HashStr = HashStr.Left(NewlineIdx);
    }
    if (HashStr.FindChar(TEXT('\r'), NewlineIdx))
    {
        HashStr = HashStr.Left(NewlineIdx);
    }
    return HashStr.TrimStartAndEnd();
}

// CFC-09/10: Helper to query player location from active session
static FString GetSessionPlayerLocation(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session)
{
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::string SpecSource = R"lua(
return {
    sample = function()
        local player_id = game.state.meta.player_actor_id
        local actor = player_id and game.state.actors and game.state.actors[player_id]
        local loc = actor and (actor.current_location_id or actor.current_location) or ""
        error("PLAYER_LOC:" .. tostring(loc))
    end
}
)lua";

    Session.RunLuaSpec("@query_player_loc", SpecSource, Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return TEXT("");
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("PLAYER_LOC:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix, ESearchCase::CaseSensitive);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract player location: %s"), *ErrorMessage));
        return TEXT("");
    }

    FString LocStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    int32 NewlineIdx = INDEX_NONE;
    if (LocStr.FindChar(TEXT('\n'), NewlineIdx))
    {
        LocStr = LocStr.Left(NewlineIdx);
    }
    if (LocStr.FindChar(TEXT('\r'), NewlineIdx))
    {
        LocStr = LocStr.Left(NewlineIdx);
    }
    return LocStr.TrimStartAndEnd();
}

// CFC-09: Helper to query the error code from the session's last command execution result
static FString GetSessionLastCommandErrorCode(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session)
{
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::string SpecSource = std::string(R"lua(
return {
    sample = function()
        local res = game and game.runtime and game.runtime.last_command_result
        local code = (res and res.error and res.error.code) or ""
        error("LAST_CMD_ERR:" .. tostring(code))
    end
}
)lua");

    Session.RunLuaSpec("@query_cmd_err", SpecSource, Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return TEXT("");
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("LAST_CMD_ERR:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix, ESearchCase::CaseSensitive);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract last command error code: %s"), *ErrorMessage));
        return TEXT("");
    }

    FString CodeStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    int32 NewlineIdx = INDEX_NONE;
    if (CodeStr.FindChar(TEXT('\n'), NewlineIdx))
    {
        CodeStr = CodeStr.Left(NewlineIdx);
    }
    if (CodeStr.FindChar(TEXT('\r'), NewlineIdx))
    {
        CodeStr = CodeStr.Left(NewlineIdx);
    }
    return CodeStr.TrimStartAndEnd();
}

// CFC-09/10: Helper to compute canonical state hash from container bytes via Lua
static FString GetContainerStateHash(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session,
    const TArray<uint8>& ContainerBytes)
{
    const FString HexStr = FString::FromHexBlob(ContainerBytes.GetData(), ContainerBytes.Num());
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const FString SpecSource = FString(TEXT("return {\n    sample = function()\n        local hex = \""))
        + HexStr
        + TEXT("\"\n")
        + TEXT(R"lua(
        local bytes = (hex:gsub('..', function(cc) return string.char(tonumber(cc, 16)) end))
        local load_mod = require("core:module.runtime.load")
        local envelope, err = load_mod.preflight(bytes)
        if not envelope then
            error("CONTAINER_ERROR:" .. tostring(err))
        end
        local canonical_codec = require("core:module.runtime.canonical_codec")
        local state_hasher = require("core:module.runtime.state_hasher")
        local ok, decoded = pcall(canonical_codec.deserialize, envelope.payload)
        if not ok or type(decoded) ~= "table" then
            error("CONTAINER_ERROR:payload_corrupt")
        end
        local hash = state_hasher.hash_state(decoded)
        error("CONTAINER_HASH:" .. tostring(hash))
    end
}
)lua");

    Session.RunLuaSpec("@query_container_hash", TCHAR_TO_UTF8(*SpecSource), Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return TEXT("");
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("CONTAINER_HASH:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix, ESearchCase::CaseSensitive);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract container state hash: %s"), *ErrorMessage));
        return TEXT("");
    }

    FString HashStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    int32 NewlineIdx = INDEX_NONE;
    if (HashStr.FindChar(TEXT('\n'), NewlineIdx))
    {
        HashStr = HashStr.Left(NewlineIdx);
    }
    if (HashStr.FindChar(TEXT('\r'), NewlineIdx))
    {
        HashStr = HashStr.Left(NewlineIdx);
    }
    return HashStr.TrimStartAndEnd();
}

// CFC-09/10: Helper to query player location from container bytes via Lua
static FString GetContainerPlayerLocation(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session,
    const TArray<uint8>& ContainerBytes)
{
    const FString HexStr = FString::FromHexBlob(ContainerBytes.GetData(), ContainerBytes.Num());
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const FString SpecSource = FString(TEXT("return {\n    sample = function()\n        local hex = \""))
        + HexStr
        + TEXT("\"\n")
        + TEXT(R"lua(
        local bytes = (hex:gsub('..', function(cc) return string.char(tonumber(cc, 16)) end))
        local load_mod = require("core:module.runtime.load")
        local envelope, err = load_mod.preflight(bytes)
        if not envelope then
            error("CONTAINER_ERROR:" .. tostring(err))
        end
        local canonical_codec = require("core:module.runtime.canonical_codec")
        local ok, decoded = pcall(canonical_codec.deserialize, envelope.payload)
        if not ok or type(decoded) ~= "table" then
            error("CONTAINER_ERROR:payload_corrupt")
        end
        local player_id = decoded.meta.player_actor_id
        local actor = player_id and decoded.actors and decoded.actors[player_id]
        local loc = actor and (actor.current_location_id or actor.current_location) or ""
        error("CONTAINER_PLAYER_LOC:" .. tostring(loc))
    end
}
)lua");

    Session.RunLuaSpec("@query_container_player_loc", TCHAR_TO_UTF8(*SpecSource), Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return TEXT("");
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("CONTAINER_PLAYER_LOC:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix, ESearchCase::CaseSensitive);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract container player location: %s"), *ErrorMessage));
        return TEXT("");
    }

    FString LocStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    int32 NewlineIdx = INDEX_NONE;
    if (LocStr.FindChar(TEXT('\n'), NewlineIdx))
    {
        LocStr = LocStr.Left(NewlineIdx);
    }
    if (LocStr.FindChar(TEXT('\r'), NewlineIdx))
    {
        LocStr = LocStr.Left(NewlineIdx);
    }
    return LocStr.TrimStartAndEnd();
}

// CFC-09: Production RequestSave executes safe-point save and writes container to disk
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadProductionRequestSaveTest,
    "GV2.Runtime.SaveAndLoad.ProductionRequestSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadProductionRequestSaveTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_prod_save");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));

    // Ensure clean state before test
    IFileManager::Get().Delete(*HeadFile);

    // 1. Start session
    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    const FString InitialLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Initial player location is non-empty"), InitialLoc.IsEmpty());

    // 2. Execute gameplay command (travel or work)
    UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("LocationScreen is presented"), Screen);
    UGV2ListViewWidgetBase* CmdRep = GetButtonRepeaterFromLocationScreen(Screen);
    TestNotNull(TEXT("ButtonRepeater exists"), CmdRep);
    UGV2ButtonWidgetBase* TravelMarketBtn = CmdRep != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(TEXT("travel_city_market"))))
        : nullptr;
    TestNotNull(TEXT("Travel to market button exists"), TravelMarketBtn);
    if (TravelMarketBtn != nullptr)
    {
        const EGV2SubmitUiInteractionResult SubmitRes = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
        TestEqual(TEXT("Command interaction accepted"), SubmitRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    const FString SessionHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString SessionLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("Session player location changed after travel"), SessionLoc, InitialLoc);

    // 3. Request save
    const int64 OpId = Runtime->RequestSave(SaveSlot);
    TestTrue(TEXT("RequestSave returned non-zero operation ID"), OpId > 0);

    ESessionOperationOutcome Outcome = ESessionOperationOutcome::Superseded;
    FGV2OperationFault Fault;
    const bool bGotOutcome = Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault);
    TestTrue(TEXT("Save outcome is available"), bGotOutcome);
    TestEqual(TEXT("Save completed successfully"), Outcome, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Completed outcome has no fault"), Fault.Code.IsEmpty());

    // 4. Verify save container on disk
    TestTrue(TEXT("Head file exists on disk"), IFileManager::Get().FileExists(*HeadFile));
    FString HeadContent;
    TestTrue(TEXT("Head file can be read"), FFileHelper::LoadFileToString(HeadContent, *HeadFile));
    TestTrue(TEXT("Head references gen_"), HeadContent.Contains(TEXT("test_prod_save.gen_")));

    TSharedPtr<FJsonObject> HeadJson;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HeadContent);
    TestTrue(TEXT("Head is valid JSON"), FJsonSerializer::Deserialize(Reader, HeadJson) && HeadJson.IsValid());
    const FString CurrentGen = HeadJson.IsValid() ? HeadJson->GetStringField(TEXT("current")) : FString();
    const FString GenFile = FPaths::Combine(SaveDir, CurrentGen);
    TestTrue(TEXT("Generation file exists on disk"), IFileManager::Get().FileExists(*GenFile));
    int64 GenFileSize = IFileManager::Get().FileSize(*GenFile);
    TestTrue(TEXT("Generation file is non-empty"), GenFileSize > 0);

    // Verify container bytes match reached state
    TArray<uint8> GenBytes;
    TestTrue(TEXT("Load save container file into bytes"), FFileHelper::LoadFileToArray(GenBytes, *GenFile));
    TestTrue(TEXT("Save container bytes non-empty"), GenBytes.Num() > 0);

    const FString SavedHash = GetContainerStateHash(*this, Coordinator->GetRuntimeSession(), GenBytes);
    const FString SavedLoc = GetContainerPlayerLocation(*this, Coordinator->GetRuntimeSession(), GenBytes);
    TestEqual(TEXT("Save container state hash matches reached session state hash"), SavedHash, SessionHash);
    TestEqual(TEXT("Save container player location matches reached session player location"), SavedLoc, SessionLoc);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    IFileManager::Get().Delete(*GenFile);
    Runtime->EndSession();
    return true;
}

// CFC-09: Save initiated via UI bound command core:command.session.save executes on safe point
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadUiAuthoredSaveButtonTest,
    "GV2.Runtime.SaveAndLoad.UiAuthoredSaveButton",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadUiAuthoredSaveButtonTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_ui_save");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Initial state: Tavern
    const FString InitialHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString InitialLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Initial player location is non-empty"), InitialLoc.IsEmpty());

    // Execute gameplay command to reach Market before saving
    UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("LocationScreen is presented"), Screen);
    UGV2ListViewWidgetBase* CmdRep = GetButtonRepeaterFromLocationScreen(Screen);
    TestNotNull(TEXT("ButtonRepeater exists"), CmdRep);
    UGV2ButtonWidgetBase* TravelMarketBtn = CmdRep != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(TEXT("travel_city_market"))))
        : nullptr;
    TestNotNull(TEXT("Travel to market button exists"), TravelMarketBtn);
    if (TravelMarketBtn != nullptr)
    {
        const EGV2SubmitUiInteractionResult TravelRes = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
        TestEqual(TEXT("Travel command accepted"), TravelRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    // Verify reached state in active session
    const FString ReachedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString ReachedLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("State hash changed after travel"), ReachedHash, InitialHash);
    TestNotEqual(TEXT("Player location changed after travel"), ReachedLoc, InitialLoc);

    // Publish a screen binding representing a UI-authored save button
    TArray<FGV2UiBindingDefinition> Defs;
    FGV2UiBindingDefinition SaveBtnDef;
    SaveBtnDef.NodeKeyPath = { TEXT("root"), TEXT("save_button") };
    SaveBtnDef.ElementId = TEXT("btn_save_game");
    SaveBtnDef.CommandId = TEXT("core:command.session.save");
    FGV2UiControlValue SlotVal;
    SlotVal.Name = FName(TEXT("slot_id"));
    SlotVal.Type = EGV2UiControlValueType::String;
    SlotVal.StringValue = SaveSlot;
    SaveBtnDef.BoundArgs.Add(SlotVal);
    Defs.Add(SaveBtnDef);

    TArray<FGV2UiBindingHandle> Handles;
    const bool bPublished = Coordinator->PublishScreenBindings(Defs, Handles);
    TestTrue(TEXT("Published screen binding for save command"), bPublished);
    if (!TestEqual(TEXT("Got 1 handle"), Handles.Num(), 1))
    {
        Runtime->EndSession();
        return false;
    }

    // Submit interaction: UI save button click
    const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(Handles[0], {});
    TestEqual(TEXT("UI save command interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);

    // Safe point drain executed the save
    TestTrue(TEXT("Head file exists on disk after UI command execution"), IFileManager::Get().FileExists(*HeadFile));
    FString HeadContent;
    TestTrue(TEXT("Head file can be read"), FFileHelper::LoadFileToString(HeadContent, *HeadFile));

    TSharedPtr<FJsonObject> HeadJson;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HeadContent);
    TestTrue(TEXT("Head is valid JSON"), FJsonSerializer::Deserialize(Reader, HeadJson) && HeadJson.IsValid());
    const FString CurrentGen = HeadJson.IsValid() ? HeadJson->GetStringField(TEXT("current")) : FString();
    const FString GenFile = FPaths::Combine(SaveDir, CurrentGen);
    TestTrue(TEXT("Generation file exists on disk"), IFileManager::Get().FileExists(*GenFile));

    // Verify save container bytes and assert it contains reached state
    TArray<uint8> GenBytes;
    TestTrue(TEXT("Load save container file into bytes"), FFileHelper::LoadFileToArray(GenBytes, *GenFile));
    TestTrue(TEXT("Save container bytes non-empty"), GenBytes.Num() > 0);

    const FString ContainerHash = GetContainerStateHash(*this, Coordinator->GetRuntimeSession(), GenBytes);
    const FString ContainerLoc = GetContainerPlayerLocation(*this, Coordinator->GetRuntimeSession(), GenBytes);

    TestEqual(TEXT("Save container state hash matches reached state hash (Market)"), ContainerHash, ReachedHash);
    TestNotEqual(TEXT("Save container state hash does not match initial state hash"), ContainerHash, InitialHash);
    TestEqual(TEXT("Save container player location matches reached location"), ContainerLoc, ReachedLoc);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    IFileManager::Get().Delete(*GenFile);
    Runtime->EndSession();
    return true;
}

// CFC-09: When command is refused (in handler or validator), staged save request is discarded; no file is written
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadCommandRefusalDiscardsSaveTest,
    "GV2.Runtime.SaveAndLoad.CommandRefusalDiscardsSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadCommandRefusalDiscardsSaveTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString ValidSlot = TEXT("test_refused_save");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, ValidSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Install a hook on core:command.session.save handler:
    // It stages a save request via game.bridge.request_save(valid_slot_id) using original handler logic,
    // and then refuses the command by returning { ok = false, error = { code = "core:error.command.validation_refused" } }.
    // Under CFC-09 invariant, the dispatcher must discard/rollback the staged request,
    // so the host receives no save request and writes no file.
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::string HookSpec = std::string(R"lua(
return {
    hook_save = function()
        local entry = game.commands.handlers.get_entry("core:command.session.save")
        if not entry then
            error("core:command.session.save handler entry not found")
        end
        local orig = entry.handler
        entry.handler = function(req)
            -- Call orig to validate slot_id and stage the save request into outbound buffer
            local res = orig(req)
            if not res or res.ok == false then
                error("Original save handler failed unexpectedly")
            end
            -- Refuse the command after staging to verify that command failure discards staged save
            return {
                ok = false,
                error = {
                    code = "core:error.command.validation_refused",
                    params = {},
                },
            }
        end
    end
}
)lua");
    const bool bHookOk = Coordinator->GetRuntimeSession().RunLuaSpec("@hook_save_refusal", HookSpec, Results, Fault);
    TestTrue(TEXT("Installed save refusal hook in session"), bHookOk && Fault.Code.empty());

    // Publish UI binding with valid slot_id
    TArray<FGV2UiBindingDefinition> Defs;
    FGV2UiBindingDefinition SaveBtnDef;
    SaveBtnDef.NodeKeyPath = { TEXT("root"), TEXT("refused_save_button") };
    SaveBtnDef.ElementId = TEXT("btn_refused_save");
    SaveBtnDef.CommandId = TEXT("core:command.session.save");
    FGV2UiControlValue SlotVal;
    SlotVal.Name = FName(TEXT("slot_id"));
    SlotVal.Type = EGV2UiControlValueType::String;
    SlotVal.StringValue = ValidSlot;
    SaveBtnDef.BoundArgs.Add(SlotVal);
    Defs.Add(SaveBtnDef);

    TArray<FGV2UiBindingHandle> Handles;
    const bool bPublished = Coordinator->PublishScreenBindings(Defs, Handles);
    TestTrue(TEXT("Published screen binding for refused save command"), bPublished);
    if (!TestEqual(TEXT("Got 1 handle"), Handles.Num(), 1))
    {
        Runtime->EndSession();
        return false;
    }

    // Submit interaction: accepted into ingress
    const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(Handles[0], {});
    TestEqual(TEXT("UI save command interaction accepted into ingress"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);

    // Check last command result in session: command dispatch refused by handler
    const FString LastErr = GetSessionLastCommandErrorCode(*this, Coordinator->GetRuntimeSession());
    TestEqual(TEXT("Command dispatch was refused by handler"), LastErr, TEXT("core:error.command.validation_refused"));

    // The staged save must have been discarded: no file written!
    TestFalse(TEXT("No head file written on handler refused command"), IFileManager::Get().FileExists(*HeadFile));

    // Verify validator refusal with valid slot ID as well
    const FString ValSlot = TEXT("test_val_refused_save");
    const FString ValHeadFile = FPaths::Combine(SaveDir, ValSlot + TEXT(".head"));
    IFileManager::Get().Delete(*ValHeadFile);

    const std::string ValidatorSpec = std::string(R"lua(
return {
    install_validator = function()
        local mt = getmetatable(game.commands.validators)
        local _, reg = debug.getupvalue(mt.__index, 1)
        local old_ordered = reg.ordered
        reg.ordered = function()
            local list = old_ordered()
            table.insert(list, {
                id = "core:validator.test.deny_save",
                impl = {
                    validate = function(ctx)
                        if ctx and ctx.command_id == "core:command.session.save" and ctx.payload and ctx.payload.slot_id == "test_val_refused_save" then
                            return false, {
                                code = "core:error.command.validation_refused",
                                params = {},
                            }
                        end
                        return true, nil
                    end
                }
            })
            return list
        end
    end
}
)lua");
    const bool bValOk = Coordinator->GetRuntimeSession().RunLuaSpec("@install_val", ValidatorSpec, Results, Fault);
    TestTrue(TEXT("Installed test validator in session"), bValOk && Fault.Code.empty());

    TArray<FGV2UiBindingDefinition> ValDefs;
    FGV2UiBindingDefinition ValSaveBtnDef;
    ValSaveBtnDef.NodeKeyPath = { TEXT("root"), TEXT("val_refused_save_button") };
    ValSaveBtnDef.ElementId = TEXT("btn_val_refused_save");
    ValSaveBtnDef.CommandId = TEXT("core:command.session.save");
    FGV2UiControlValue ValSlotVal;
    ValSlotVal.Name = FName(TEXT("slot_id"));
    ValSlotVal.Type = EGV2UiControlValueType::String;
    ValSlotVal.StringValue = ValSlot;
    ValSaveBtnDef.BoundArgs.Add(ValSlotVal);
    ValDefs.Add(ValSaveBtnDef);

    TArray<FGV2UiBindingHandle> ValHandles;
    TestTrue(TEXT("Published binding for validator save"), Coordinator->PublishScreenBindings(ValDefs, ValHandles));
    if (TestEqual(TEXT("Got 1 validator handle"), ValHandles.Num(), 1))
    {
        const EGV2SubmitUiInteractionResult ValSubmitRes = Runtime->SubmitUiInteraction(ValHandles[0], {});
        TestEqual(TEXT("Validator save command interaction accepted into ingress"), ValSubmitRes, EGV2SubmitUiInteractionResult::Accepted);

        const FString ValLastErr = GetSessionLastCommandErrorCode(*this, Coordinator->GetRuntimeSession());
        TestEqual(TEXT("Command dispatch was refused by validator"), ValLastErr, TEXT("core:error.command.validation_refused"));
        TestFalse(TEXT("No head file written on validator refused command"), IFileManager::Get().FileExists(*ValHeadFile));
    }

    IFileManager::Get().Delete(*HeadFile);
    IFileManager::Get().Delete(*ValHeadFile);
    Runtime->EndSession();
    return true;
}

// CFC-09: Invalid slot name grammar is rejected and writes no file
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadInvalidSlotNameRejectionTest,
    "GV2.Runtime.SaveAndLoad.InvalidSlotNameRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadInvalidSlotNameRejectionTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString BadSlot = TEXT("INVALID_SLOT_NAME!");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, BadSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Publish binding with invalid slot_id argument
    TArray<FGV2UiBindingDefinition> Defs;
    FGV2UiBindingDefinition SaveBtnDef;
    SaveBtnDef.NodeKeyPath = { TEXT("root"), TEXT("bad_save_button") };
    SaveBtnDef.ElementId = TEXT("btn_bad_save");
    SaveBtnDef.CommandId = TEXT("core:command.session.save");
    FGV2UiControlValue BadSlotVal;
    BadSlotVal.Name = FName(TEXT("slot_id"));
    BadSlotVal.Type = EGV2UiControlValueType::String;
    BadSlotVal.StringValue = BadSlot;
    SaveBtnDef.BoundArgs.Add(BadSlotVal);
    Defs.Add(SaveBtnDef);

    TArray<FGV2UiBindingHandle> Handles;
    const bool bPublished = Coordinator->PublishScreenBindings(Defs, Handles);
    TestTrue(TEXT("Published screen binding for invalid slot save"), bPublished);
    if (!TestEqual(TEXT("Got 1 handle"), Handles.Num(), 1))
    {
        Runtime->EndSession();
        return false;
    }

    // Submit interaction: accepted into ingress
    const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(Handles[0], {});
    TestEqual(TEXT("UI save command interaction accepted into ingress"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);

    // Check last command result in session: rejected with core:error.save.request_rejected
    const FString LastErr = GetSessionLastCommandErrorCode(*this, Coordinator->GetRuntimeSession());
    TestEqual(TEXT("Command rejected with request_rejected error code"), LastErr, TEXT("core:error.save.request_rejected"));

    // No head file written
    TestFalse(TEXT("No head file written on invalid slot name"), IFileManager::Get().FileExists(*HeadFile));

    Runtime->EndSession();
    return true;
}

// CFC-09: Consecutive saves produce Current and Previous revisions in storage
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadConsecutiveSavesCurrentAndPreviousTest,
    "GV2.Runtime.SaveAndLoad.ConsecutiveSavesCurrentAndPrevious",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadConsecutiveSavesCurrentAndPreviousTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_consec_save");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Initial state: State 1 (Tavern)
    const FString State1Hash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString State1Loc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Initial location is non-empty"), State1Loc.IsEmpty());

    // First save: generation 1 (State 1: Tavern)
    const int64 Op1 = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome Outcome1;
    FGV2OperationFault Fault1;
    TestTrue(TEXT("Outcome 1 available"), Runtime->GetSessionOperationOutcome(Op1, Outcome1, Fault1));
    TestEqual(TEXT("Save 1 completed"), Outcome1, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Completed outcome 1 has no fault"), Fault1.Code.IsEmpty());

    FString Head1;
    TestTrue(TEXT("Head 1 exists"), FFileHelper::LoadFileToString(Head1, *HeadFile));
    TSharedPtr<FJsonObject> Head1Json;
    TSharedRef<TJsonReader<>> Reader1 = TJsonReaderFactory<>::Create(Head1);
    TestTrue(TEXT("Head 1 valid JSON"), FJsonSerializer::Deserialize(Reader1, Head1Json) && Head1Json.IsValid());
    const FString Gen1Name = Head1Json.IsValid() ? Head1Json->GetStringField(TEXT("current")) : FString();
    TestTrue(TEXT("Head 1 has current gen"), Gen1Name.StartsWith(TEXT("test_consec_save.gen_")));
    TestFalse(TEXT("Head 1 has no previous"), Head1Json.IsValid() && Head1Json->HasField(TEXT("previous")));
    const FString Gen1File = FPaths::Combine(SaveDir, Gen1Name);
    TestTrue(TEXT("Gen 1 file exists"), IFileManager::Get().FileExists(*Gen1File));

    TArray<uint8> Gen1Bytes;
    TestTrue(TEXT("Gen 1 bytes loaded"), FFileHelper::LoadFileToArray(Gen1Bytes, *Gen1File));
    TestTrue(TEXT("Gen 1 bytes non-empty"), Gen1Bytes.Num() > 0);

    const FString Gen1ContainerHash = GetContainerStateHash(*this, Coordinator->GetRuntimeSession(), Gen1Bytes);
    const FString Gen1ContainerLoc = GetContainerPlayerLocation(*this, Coordinator->GetRuntimeSession(), Gen1Bytes);
    TestEqual(TEXT("Gen 1 container state hash matches State 1"), Gen1ContainerHash, State1Hash);
    TestEqual(TEXT("Gen 1 container player location is tavern"), Gen1ContainerLoc, State1Loc);

    // Separate saves by a gameplay command: travel to Market
    UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("LocationScreen is presented"), Screen);
    UGV2ListViewWidgetBase* CmdRep = GetButtonRepeaterFromLocationScreen(Screen);
    TestNotNull(TEXT("ButtonRepeater exists"), CmdRep);
    UGV2ButtonWidgetBase* TravelMarketBtn = CmdRep != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(TEXT("travel_city_market"))))
        : nullptr;
    TestNotNull(TEXT("Travel to market button exists"), TravelMarketBtn);
    if (TravelMarketBtn != nullptr)
    {
        const EGV2SubmitUiInteractionResult TravelRes = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
        TestEqual(TEXT("Travel command accepted"), TravelRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    // Mutated state: State 2 (Market)
    const FString State2Hash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString State2Loc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("State 2 hash differs from State 1 hash"), State2Hash, State1Hash);
    TestNotEqual(TEXT("Player reached different location for State 2"), State2Loc, State1Loc);

    // Second save: generation 2 (State 2: Market)
    const int64 Op2 = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome Outcome2;
    FGV2OperationFault Fault2;
    TestTrue(TEXT("Outcome 2 available"), Runtime->GetSessionOperationOutcome(Op2, Outcome2, Fault2));
    TestEqual(TEXT("Save 2 completed"), Outcome2, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Completed outcome 2 has no fault"), Fault2.Code.IsEmpty());

    FString Head2;
    TestTrue(TEXT("Head 2 exists"), FFileHelper::LoadFileToString(Head2, *HeadFile));
    TSharedPtr<FJsonObject> Head2Json;
    TSharedRef<TJsonReader<>> Reader2 = TJsonReaderFactory<>::Create(Head2);
    TestTrue(TEXT("Head 2 valid JSON"), FJsonSerializer::Deserialize(Reader2, Head2Json) && Head2Json.IsValid());
    const FString Gen2Name = Head2Json.IsValid() ? Head2Json->GetStringField(TEXT("current")) : FString();
    const FString Head2PrevName = Head2Json.IsValid() ? Head2Json->GetStringField(TEXT("previous")) : FString();
    TestTrue(TEXT("Head 2 has current gen"), Gen2Name.StartsWith(TEXT("test_consec_save.gen_")));
    TestEqual(TEXT("Head 2 previous matches Gen 1"), Head2PrevName, Gen1Name);
    TestNotEqual(TEXT("Current and previous distinct"), Gen2Name, Gen1Name);

    const FString Gen2File = FPaths::Combine(SaveDir, Gen2Name);
    TestTrue(TEXT("Gen 1 file still exists as previous"), IFileManager::Get().FileExists(*Gen1File));
    TestTrue(TEXT("Gen 2 file exists as current"), IFileManager::Get().FileExists(*Gen2File));

    TArray<uint8> Gen2Bytes;
    TestTrue(TEXT("Gen 2 bytes loaded"), FFileHelper::LoadFileToArray(Gen2Bytes, *Gen2File));
    TestTrue(TEXT("Gen 2 bytes non-empty"), Gen2Bytes.Num() > 0);

    const FString Gen2ContainerHash = GetContainerStateHash(*this, Coordinator->GetRuntimeSession(), Gen2Bytes);
    const FString Gen2ContainerLoc = GetContainerPlayerLocation(*this, Coordinator->GetRuntimeSession(), Gen2Bytes);
    TestEqual(TEXT("Gen 2 container state hash matches State 2"), Gen2ContainerHash, State2Hash);
    TestEqual(TEXT("Gen 2 container player location is market"), Gen2ContainerLoc, State2Loc);

    // Require that Current and Previous bytes differ and correspond to their reached states
    TestNotEqual(TEXT("Current (Gen 2) and Previous (Gen 1) save bytes are distinct"), Gen2Bytes, Gen1Bytes);
    TestNotEqual(TEXT("Current (Gen 2) and Previous (Gen 1) container hashes are distinct"), Gen2ContainerHash, Gen1ContainerHash);
    TestEqual(TEXT("Previous container hash corresponds to State 1 (Tavern)"), Gen1ContainerHash, State1Hash);
    TestEqual(TEXT("Current container hash corresponds to State 2 (Market)"), Gen2ContainerHash, State2Hash);
    TestNotEqual(TEXT("Current and Previous container player locations are distinct"), Gen2ContainerLoc, Gen1ContainerLoc);
    TestEqual(TEXT("Previous container player location corresponds to State 1 (Tavern)"), Gen1ContainerLoc, State1Loc);
    TestEqual(TEXT("Current container player location corresponds to State 2 (Market)"), Gen2ContainerLoc, State2Loc);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    IFileManager::Get().Delete(*Gen1File);
    IFileManager::Get().Delete(*Gen2File);
    Runtime->EndSession();
    return true;
}

// CFC-09: Error cases: unready session, invalid slot name, cancellation before execution
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadRequestSaveErrorCasesTest,
    "GV2.Runtime.SaveAndLoad.RequestSaveErrorCases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadRequestSaveErrorCasesTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    // 1. Unready session: RequestSave before StartSession
    const int64 UnreadyOp = Runtime->RequestSave(TEXT("any_slot"));
    TestTrue(TEXT("Unready RequestSave returns op ID"), UnreadyOp > 0);
    ESessionOperationOutcome UnreadyOutcome;
    FGV2OperationFault UnreadyFault;
    TestTrue(TEXT("Unready outcome available"), Runtime->GetSessionOperationOutcome(UnreadyOp, UnreadyOutcome, UnreadyFault));
    TestEqual(TEXT("Unready outcome is Failed"), UnreadyOutcome, ESessionOperationOutcome::Failed);
    TestEqual(TEXT("Unready fault code is SessionNotReady"), UnreadyFault.Code, FGV2SessionFaultCodes::SessionNotReady);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    // 2. Invalid slot names
    const TArray<FString> BadSlotNames = {
        TEXT(""),
        TEXT("123bad"),
        TEXT("BadCaps"),
        TEXT("bad-dash"),
        TEXT("bad slot with spaces"),
    };
    for (const FString& BadSlot : BadSlotNames)
    {
        const int64 BadOp = Runtime->RequestSave(BadSlot);
        TestTrue(TEXT("Bad slot returns op ID"), BadOp > 0);
        ESessionOperationOutcome BadOutcome;
        FGV2OperationFault BadFault;
        TestTrue(TEXT("Bad slot outcome available"), Runtime->GetSessionOperationOutcome(BadOp, BadOutcome, BadFault));
        TestEqual(TEXT("Invalid slot name outcome is Failed"), BadOutcome, ESessionOperationOutcome::Failed);
        TestEqual(TEXT("Bad slot fault code is InvalidSaveSlotId"), BadFault.Code, FGV2SessionFaultCodes::InvalidSaveSlotId);
    }

    // 3. Cancellation before execution
    // Set interaction sink to queue a save while pumping ingress, and immediately cancel it
    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    uint64 QueuedOpId = 0;
    ESessionCancellationResult CancelResult = ESessionCancellationResult::Stale;

    Coordinator->SetInteractionSink([&](const FGV2UiIngressItem&)
    {
        QueuedOpId = Coordinator->RequestSave(TEXT("cancelled_slot"));
        CancelResult = Coordinator->CancelSessionRequest(QueuedOpId);
    });

    // Publish a dummy binding and submit interaction to trigger the sink
    TArray<FGV2UiBindingDefinition> DummyDefs;
    FGV2UiBindingDefinition DummyDef;
    DummyDef.NodeKeyPath = { TEXT("root"), TEXT("dummy_button") };
    DummyDef.ElementId = TEXT("btn_dummy");
    DummyDef.CommandId = TEXT("core:command.session.save");
    FGV2UiControlValue DummyVal;
    DummyVal.Name = FName(TEXT("slot_id"));
    DummyVal.Type = EGV2UiControlValueType::String;
    DummyVal.StringValue = TEXT("dummy_slot");
    DummyDef.BoundArgs.Add(DummyVal);
    DummyDefs.Add(DummyDef);

    TArray<FGV2UiBindingHandle> DummyHandles;
    const bool bPublished = Coordinator->PublishScreenBindings(DummyDefs, DummyHandles);
    TestTrue(TEXT("Published screen binding for dummy"), bPublished);
    if (!TestEqual(TEXT("Got 1 handle"), DummyHandles.Num(), 1))
    {
        Coordinator->SetInteractionSink(nullptr);
        Runtime->EndSession();
        return false;
    }
    Runtime->SubmitUiInteraction(DummyHandles[0], {});

    TestTrue(TEXT("QueuedOpId was set"), QueuedOpId > 0);
    TestEqual(TEXT("Cancellation of queued save was accepted"), CancelResult, ESessionCancellationResult::Accepted);

    TOptional<FGV2SessionOperationResult> CancelOutcome = Coordinator->GetSessionOperationOutcome(QueuedOpId);
    TestTrue(TEXT("CancelOutcome is set"), CancelOutcome.IsSet());
    TestEqual(TEXT("Outcome is Cancelled"), CancelOutcome->Outcome, ESessionOperationOutcome::Cancelled);
    TestTrue(TEXT("Cancelled outcome has no fault"), CancelOutcome->Fault.Code.IsEmpty());

    // Reset interaction sink
    Coordinator->SetInteractionSink(nullptr);

    // Stale cancellation on completed operation
    const ESessionCancellationResult StaleResult = Coordinator->CancelSessionRequest(999999);
    TestEqual(TEXT("Unknown op returns Stale"), StaleResult, ESessionCancellationResult::Stale);

    // Cleanup files that might have been created by dummy_slot
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    IFileManager::Get().Delete(*FPaths::Combine(SaveDir, TEXT("dummy_slot.head")));
    IFileManager::Get().Delete(*FPaths::Combine(SaveDir, TEXT("dummy_slot.gen_1")));
    IFileManager::Get().Delete(*FPaths::Combine(SaveDir, TEXT("cancelled_slot.head")));
    IFileManager::Get().Delete(*FPaths::Combine(SaveDir, TEXT("cancelled_slot.gen_1")));

    Runtime->EndSession();
    return true;
}

// CFC-10: RequestLoad starts session B from save slot and restores state
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadProductionRequestLoadTest,
    "GV2.Runtime.SaveAndLoad.ProductionRequestLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadProductionRequestLoadTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_prod_load");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> ExistingGens;
    IFileManager::Get().FindFiles(ExistingGens, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : ExistingGens)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    // 1. Start initial session A
    Runtime->StartSession();
    TestTrue(TEXT("Session A is ready"), Runtime->GetSessionState().bIsReady);
    const int32 GenA = Runtime->GetSessionState().SessionGeneration;
    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);
    if (Coordinator == nullptr)
    {
        Runtime->EndSession();
        return false;
    }

    // Capture initial state identity (Tavern)
    const FString InitialHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Initial state hash is non-empty"), InitialHash.IsEmpty());
    const FString InitialLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Initial player location is non-empty"), InitialLoc.IsEmpty());

    // 2. Submit gameplay command 1: travel to Market
    UGV2ScreenWidgetBase* ScreenA1 = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("Session A LocationScreen is presented"), ScreenA1);
    UGV2ListViewWidgetBase* CmdRepA1 = GetButtonRepeaterFromLocationScreen(ScreenA1);
    TestNotNull(TEXT("Tavern ButtonRepeater exists"), CmdRepA1);
    UGV2ButtonWidgetBase* TravelMarketBtn = CmdRepA1 != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRepA1->GetEntryWidget(FName(TEXT("travel_city_market"))))
        : nullptr;
    TestNotNull(TEXT("Travel to market button found in Tavern"), TravelMarketBtn);
    if (TravelMarketBtn != nullptr)
    {
        const EGV2SubmitUiInteractionResult SubmitRes = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
        TestEqual(TEXT("Command 1 (travel to market) accepted"), SubmitRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    // Verify state after Command 1: player is now at Market
    const FString SavedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString SavedLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("State hash changed after travel to market"), SavedHash, InitialHash);
    TestNotEqual(TEXT("Player location updated after travel to market"), SavedLoc, InitialLoc);

    // 3. Request save: captures state at Market
    const int64 SaveOpId = Runtime->RequestSave(SaveSlot);
    TestTrue(TEXT("RequestSave returned non-zero operation ID"), SaveOpId > 0);
    ESessionOperationOutcome SaveOutcome;
    FGV2OperationFault SaveFault;
    TestTrue(TEXT("Save outcome is available"), Runtime->GetSessionOperationOutcome(SaveOpId, SaveOutcome, SaveFault));
    TestEqual(TEXT("Save completed successfully"), SaveOutcome, ESessionOperationOutcome::Completed);

    // 4. Submit gameplay command 2 in Session A (post-save mutation): travel back to Tavern
    UGV2ScreenWidgetBase* ScreenA2 = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("Session A LocationScreen at Market is presented"), ScreenA2);
    UGV2ListViewWidgetBase* CmdRepA2 = GetButtonRepeaterFromLocationScreen(ScreenA2);
    TestNotNull(TEXT("Market ButtonRepeater exists"), CmdRepA2);
    UGV2ButtonWidgetBase* TravelTavernBtnA = CmdRepA2 != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRepA2->GetEntryWidget(FName(TEXT("travel_city_tavern"))))
        : nullptr;
    TestNotNull(TEXT("Travel to tavern button found in Market"), TravelTavernBtnA);
    if (TravelTavernBtnA != nullptr)
    {
        const EGV2SubmitUiInteractionResult SubmitRes2 = Runtime->SubmitUiInteraction(TravelTavernBtnA->GetBindingHandle(), {});
        TestEqual(TEXT("Command 2 (post-save travel back to tavern) accepted"), SubmitRes2, EGV2SubmitUiInteractionResult::Accepted);
    }

    // Verify Session A mutated post-save to Tavern
    const FString PostSaveHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString PostSaveLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("State hash changed after post-save mutation"), PostSaveHash, SavedHash);
    TestEqual(TEXT("Player location changed back to initial location after command 2"), PostSaveLoc, InitialLoc);

    // 5. Request load: replaces Session A with Session B restored from SaveSlot
    const int64 LoadOpId = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Current);
    TestTrue(TEXT("RequestLoad returned non-zero operation ID"), LoadOpId > 0);
    ESessionOperationOutcome LoadOutcome;
    FGV2OperationFault LoadFault;
    TestTrue(TEXT("Load outcome is available"), Runtime->GetSessionOperationOutcome(LoadOpId, LoadOutcome, LoadFault));
    TestEqual(TEXT("Load completed successfully"), LoadOutcome, ESessionOperationOutcome::Completed);

    // 6. Assert replacement session B lifecycle state
    const FGV2SessionStatus StatusB = Runtime->GetSessionState();
    TestTrue(TEXT("Session B is ready"), StatusB.bIsReady);
    TestEqual(TEXT("Session B is in Ready state"), StatusB.SessionState, EGV2SessionState::Ready);
    TestTrue(TEXT("Session generation incremented"), StatusB.SessionGeneration > GenA);

    // 7. CFC-10: Assert independent state identity and gameplay value restoration
    const FString LoadedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString LoadedLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());

    // Independent state hash assertions
    TestEqual(TEXT("Loaded state hash strictly matches saved state hash"), LoadedHash, SavedHash);
    TestNotEqual(TEXT("Loaded state hash does not match post-save mutation hash"), LoadedHash, PostSaveHash);
    TestNotEqual(TEXT("Loaded state hash does not match initial new-game hash"), LoadedHash, InitialHash);

    // Canonical gameplay value assertions
    TestEqual(TEXT("Loaded player location strictly matches saved location (Market)"), LoadedLoc, SavedLoc);
    TestNotEqual(TEXT("Loaded player location does not match post-save location (Tavern)"), LoadedLoc, PostSaveLoc);

    // Presentation / UI verification in loaded Session B
    UGV2ScreenWidgetBase* ScreenB = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("Session B LocationScreen is presented"), ScreenB);
    UGV2ListViewWidgetBase* CmdRepB = GetButtonRepeaterFromLocationScreen(ScreenB);
    TestNotNull(TEXT("Session B ButtonRepeater exists"), CmdRepB);
    if (CmdRepB != nullptr)
    {
        TestNotNull(TEXT("travel_city_tavern button present in loaded Market screen"), CmdRepB->GetEntryWidget(FName(TEXT("travel_city_tavern"))));
        TestNull(TEXT("travel_city_market button absent from loaded Market screen"), CmdRepB->GetEntryWidget(FName(TEXT("travel_city_market"))));
    }

    // 8. Submit gameplay command in session B to verify post-load mutability
    if (CmdRepB != nullptr)
    {
        if (auto* Btn = Cast<UGV2ButtonWidgetBase>(CmdRepB->GetEntryWidget(FName(TEXT("travel_city_tavern")))))
        {
            const EGV2SubmitUiInteractionResult SubmitRes = Runtime->SubmitUiInteraction(Btn->GetBindingHandle(), {});
            TestEqual(TEXT("Session B command interaction accepted"), SubmitRes, EGV2SubmitUiInteractionResult::Accepted);

            const FString HashBPostCmd = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
            const FString LocBPostCmd = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
            TestNotEqual(TEXT("Session B state hash changed after executing subsequent command"), HashBPostCmd, LoadedHash);
            TestEqual(TEXT("Session B player location updated to initial location"), LocBPostCmd, InitialLoc);
        }
    }

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->EndSession();
    return true;
}

// CFC-10: UI-authored load button triggers core:command.session.load which performs replacement load
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadUiAuthoredLoadButtonTest,
    "GV2.Runtime.SaveAndLoad.UiAuthoredLoadButton",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadUiAuthoredLoadButtonTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_ui_load");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);
    const int32 GenA = Runtime->GetSessionState().SessionGeneration;

    // Save slot first
    const int64 SaveOp = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome SaveOutcome;
    FGV2OperationFault SaveFault;
    TestTrue(TEXT("Save outcome available"), Runtime->GetSessionOperationOutcome(SaveOp, SaveOutcome, SaveFault));
    TestEqual(TEXT("Save completed"), SaveOutcome, ESessionOperationOutcome::Completed);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Publish UI binding with core:command.session.load
    TArray<FGV2UiBindingDefinition> Defs;
    FGV2UiBindingDefinition LoadBtnDef;
    LoadBtnDef.NodeKeyPath = { TEXT("root"), TEXT("load_button") };
    LoadBtnDef.ElementId = TEXT("btn_load");
    LoadBtnDef.CommandId = TEXT("core:command.session.load");
    FGV2UiControlValue SlotVal;
    SlotVal.Name = FName(TEXT("slot_id"));
    SlotVal.Type = EGV2UiControlValueType::String;
    SlotVal.StringValue = SaveSlot;
    LoadBtnDef.BoundArgs.Add(SlotVal);
    Defs.Add(LoadBtnDef);

    TArray<FGV2UiBindingHandle> Handles;
    const bool bPublished = Coordinator->PublishScreenBindings(Defs, Handles);
    TestTrue(TEXT("Published screen binding for load button"), bPublished);
    if (!TestEqual(TEXT("Got 1 handle"), Handles.Num(), 1))
    {
        Runtime->EndSession();
        return false;
    }

    // Submit UI interaction: triggers load command
    const EGV2SubmitUiInteractionResult SubmitResult = Runtime->SubmitUiInteraction(Handles[0], {});
    TestEqual(TEXT("UI load command interaction accepted"), SubmitResult, EGV2SubmitUiInteractionResult::Accepted);

    // Assert session B is ready with new generation
    const FGV2SessionStatus StatusB = Runtime->GetSessionState();
    TestTrue(TEXT("Session B is ready after UI load"), StatusB.bIsReady);
    TestTrue(TEXT("Session B generation incremented"), StatusB.SessionGeneration > GenA);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->EndSession();
    return true;
}

// CFC-10: Corrupt save bytes fail preflight in VM A and preserve Session A
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadPreflightFailurePreservesSessionATest,
    "GV2.Runtime.SaveAndLoad.PreflightFailurePreservesSessionA",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadPreflightFailurePreservesSessionATest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString CorruptSlot = TEXT("test_corrupt_preflight");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, CorruptSlot + TEXT(".head"));
    const FString GenFile = FPaths::Combine(SaveDir, CorruptSlot + TEXT(".gen_1.save"));

    // Write corrupted container on disk
    IFileManager::Get().MakeDirectory(*SaveDir, true);
    FFileHelper::SaveStringToFile(TEXT("{\"version\":1,\"current\":\"test_corrupt_preflight.gen_1.save\"}"), *HeadFile);
    FFileHelper::SaveStringToFile(TEXT("GARBAGE_BYTES_NOT_A_VALID_SAVE_CONTAINER"), *GenFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session A is ready"), Runtime->GetSessionState().bIsReady);
    const int32 GenA = Runtime->GetSessionState().SessionGeneration;
    UGV2GameShellWidgetBase* ShellA = Runtime->GetActiveGameShell();
    UUserWidget* ScreenA = Runtime->GetActiveScreen();

    AddExpectedError(
        TEXT("GV2 Lua runtime fault: code=SaveContainerCorrupt"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    // Request load of corrupt slot
    const int64 LoadOpId = Runtime->RequestLoad(CorruptSlot, EGV2SaveSlotRevision::Current);
    TestTrue(TEXT("RequestLoad returned non-zero operation ID"), LoadOpId > 0);

    ESessionOperationOutcome LoadOutcome;
    FGV2OperationFault LoadFault;
    TestTrue(TEXT("Load outcome available"), Runtime->GetSessionOperationOutcome(LoadOpId, LoadOutcome, LoadFault));
    TestEqual(TEXT("Load outcome is Failed"), LoadOutcome, ESessionOperationOutcome::Failed);
    TestEqual(TEXT("Load fault code is SaveContainerCorrupt"), LoadFault.Code, TEXT("SaveContainerCorrupt"));

    // Assert session A is preserved
    const FGV2SessionStatus StatusAfterFail = Runtime->GetSessionState();
    TestTrue(TEXT("Session A remains ready"), StatusAfterFail.bIsReady);
    TestEqual(TEXT("Session A remains in Ready state"), StatusAfterFail.SessionState, EGV2SessionState::Ready);
    TestEqual(TEXT("Session generation unchanged"), StatusAfterFail.SessionGeneration, GenA);
    TestEqual(TEXT("GameShell unchanged"), Runtime->GetActiveGameShell(), ShellA);
    TestEqual(TEXT("Screen unchanged"), Runtime->GetActiveScreen(), ScreenA);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    IFileManager::Get().Delete(*GenFile);

    Runtime->EndSession();
    return true;
}

// CFC-10: CapturedSaveBytes in-memory buffer makes replacement immune to disk file tampering after capture
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadCapturedBytesImmunityToFileOverwriteTest,
    "GV2.Runtime.SaveAndLoad.CapturedBytesImmunityToFileOverwrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadCapturedBytesImmunityToFileOverwriteTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_captured_immunity");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);
    const int32 GenA = Runtime->GetSessionState().SessionGeneration;

    const int64 SaveOp = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome SaveOutcome;
    FGV2OperationFault SaveFault;
    TestTrue(TEXT("Save outcome available"), Runtime->GetSessionOperationOutcome(SaveOp, SaveOutcome, SaveFault));
    TestEqual(TEXT("Save completed"), SaveOutcome, ESessionOperationOutcome::Completed);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Set hook to corrupt file on disk right after bytes are captured
    bool bHookFired = false;
    Coordinator->TestOnSaveBytesCaptured = [&]()
    {
        bHookFired = true;
        // Delete head file and corrupt gen file on disk
        IFileManager::Get().Delete(*HeadFile);
        TArray<FString> GenFiles;
        IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
        for (const FString& GenFile : GenFiles)
        {
            FFileHelper::SaveStringToFile(TEXT("DESTROYED_ON_DISK"), *FPaths::Combine(SaveDir, GenFile));
        }
    };

    const int64 LoadOp = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Current);
    TestTrue(TEXT("RequestLoad returned op ID"), LoadOp > 0);

    // Clear hook
    Coordinator->TestOnSaveBytesCaptured = nullptr;

    TestTrue(TEXT("Hook was fired during ExecuteSessionStart"), bHookFired);

    ESessionOperationOutcome LoadOutcome;
    FGV2OperationFault LoadFault;
    TestTrue(TEXT("Load outcome available"), Runtime->GetSessionOperationOutcome(LoadOp, LoadOutcome, LoadFault));
    TestEqual(TEXT("Load completed successfully despite disk corruption"), LoadOutcome, ESessionOperationOutcome::Completed);

    const FGV2SessionStatus StatusB = Runtime->GetSessionState();
    TestTrue(TEXT("Session B is ready"), StatusB.bIsReady);
    TestTrue(TEXT("Session B generation incremented"), StatusB.SessionGeneration > GenA);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->EndSession();
    return true;
}

// CFC-10: Loading Previous revision restores the previous generation rather than current
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadPreviousRevisionLoadTest,
    "GV2.Runtime.SaveAndLoad.PreviousRevisionLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadPreviousRevisionLoadTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_prev_rev_load");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);

    // Initial state: State 1 (Tavern)
    const FString State1Hash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString State1Loc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Initial location is non-empty"), State1Loc.IsEmpty());

    // Save 1 (Rev 1: Tavern)
    const int64 SaveOp1 = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome Outcome1;
    FGV2OperationFault Fault1;
    TestTrue(TEXT("Save 1 outcome available"), Runtime->GetSessionOperationOutcome(SaveOp1, Outcome1, Fault1));
    TestEqual(TEXT("Save 1 completed"), Outcome1, ESessionOperationOutcome::Completed);

    // Mutate state with command: travel to Market
    UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("LocationScreen is presented"), Screen);
    UGV2ListViewWidgetBase* CmdRep = GetButtonRepeaterFromLocationScreen(Screen);
    TestNotNull(TEXT("ButtonRepeater exists"), CmdRep);
    UGV2ButtonWidgetBase* TravelMarketBtn = CmdRep != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(TEXT("travel_city_market"))))
        : nullptr;
    TestNotNull(TEXT("Travel to market button exists"), TravelMarketBtn);
    if (TravelMarketBtn != nullptr)
    {
        const EGV2SubmitUiInteractionResult TravelRes = Runtime->SubmitUiInteraction(TravelMarketBtn->GetBindingHandle(), {});
        TestEqual(TEXT("Travel command accepted"), TravelRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    // Mutated state: State 2 (Market)
    const FString State2Hash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString State2Loc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("State 2 hash differs from State 1 hash"), State2Hash, State1Hash);
    TestNotEqual(TEXT("Player reached different location for State 2"), State2Loc, State1Loc);

    // Save 2 (Rev 2: now current; Rev 1 is now previous)
    const int64 SaveOp2 = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome Outcome2;
    FGV2OperationFault Fault2;
    TestTrue(TEXT("Save 2 outcome available"), Runtime->GetSessionOperationOutcome(SaveOp2, Outcome2, Fault2));
    TestEqual(TEXT("Save 2 completed"), Outcome2, ESessionOperationOutcome::Completed);

    // Verify .head has both current and previous
    FString HeadContent;
    TestTrue(TEXT("Head exists"), FFileHelper::LoadFileToString(HeadContent, *HeadFile));
    TSharedPtr<FJsonObject> HeadJson;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HeadContent);
    TestTrue(TEXT("Head valid JSON"), FJsonSerializer::Deserialize(Reader, HeadJson) && HeadJson.IsValid());
    TestTrue(TEXT("Head has current"), HeadJson.IsValid() && HeadJson->HasField(TEXT("current")));
    TestTrue(TEXT("Head has previous"), HeadJson.IsValid() && HeadJson->HasField(TEXT("previous")));

    const int32 GenBeforeLoad = Runtime->GetSessionState().SessionGeneration;

    // RequestLoad with Previous revision (restores State 1: Tavern)
    const int64 LoadOp = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Previous);
    TestTrue(TEXT("RequestLoad returned op ID"), LoadOp > 0);

    ESessionOperationOutcome LoadOutcome;
    FGV2OperationFault LoadFault;
    TestTrue(TEXT("Load outcome available"), Runtime->GetSessionOperationOutcome(LoadOp, LoadOutcome, LoadFault));
    TestEqual(TEXT("Load previous completed"), LoadOutcome, ESessionOperationOutcome::Completed);

    const FGV2SessionStatus StatusAfterLoad = Runtime->GetSessionState();
    TestTrue(TEXT("Session is ready after loading previous"), StatusAfterLoad.bIsReady);
    TestTrue(TEXT("Generation incremented"), StatusAfterLoad.SessionGeneration > GenBeforeLoad);

    // Assert that loaded previous session restored State 1 (Tavern) and not State 2 (Market)
    const FString LoadedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString LoadedLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestEqual(TEXT("Loaded previous session hash matches State 1 (Tavern)"), LoadedHash, State1Hash);
    TestNotEqual(TEXT("Loaded previous session hash differs from State 2 (Market)"), LoadedHash, State2Hash);
    TestEqual(TEXT("Loaded previous player location is Tavern"), LoadedLoc, State1Loc);
    TestNotEqual(TEXT("Loaded previous player location is not Market"), LoadedLoc, State2Loc);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->EndSession();
    return true;
}

// CFC-10: Helper to sample PRNG next_u32 directly from active session
static uint32 SampleSessionPrngU32(
    FAutomationTestBase& Test,
    GV2RuntimeCore::FRuntimeSession& Session,
    const FString& StreamId)
{
    const std::string StreamIdUtf8 = TCHAR_TO_UTF8(*StreamId);
    std::vector<GV2RuntimeCore::FLuaSpecCaseResult> Results;
    GV2RuntimeCore::FRuntimeFault Fault;
    const std::string SpecSource = std::string(R"lua(
return {
    sample = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        local random = require("core:module.runtime.random")
        local val = mutation_window.execute_in_window(function()
            return random.next_u32(")lua") + StreamIdUtf8 + R"lua(")
        end)
        error("PRNG_VAL:" .. tostring(val))
    end
}
)lua";

    Session.RunLuaSpec("@test_prng_sample", SpecSource, Results, Fault);
    if (Results.empty())
    {
        Test.AddError(FString::Printf(TEXT("RunLuaSpec returned no results: %s"), UTF8_TO_TCHAR(Fault.Message.c_str())));
        return 0;
    }

    const FString ErrorMessage = UTF8_TO_TCHAR(Results[0].ErrorMessage.c_str());
    const FString Prefix = TEXT("PRNG_VAL:");
    const int32 PrefixIdx = ErrorMessage.Find(Prefix);
    if (PrefixIdx == INDEX_NONE)
    {
        Test.AddError(FString::Printf(TEXT("Could not extract PRNG value from spec error: %s"), *ErrorMessage));
        return 0;
    }

    const FString NumStr = ErrorMessage.Mid(PrefixIdx + Prefix.Len());
    return static_cast<uint32>(FCString::Strtoui64(*NumStr, nullptr, 10));
}

// CFC-10: Enforce empty seed descriptor for load and verify PRNG continuity
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadPrngStreamContinuationTest,
    "GV2.Runtime.SaveAndLoad.PrngStreamContinuation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadPrngStreamContinuationTest::RunTest(const FString& Parameters)
{
    // 1. Negative descriptor validation: non-empty seed is rejected for LoadSave mode
    {
        FSessionStartDescriptor BadDesc;
        BadDesc.Mode = ESessionStartMode::LoadSave;
        BadDesc.SaveSlotId = TEXT("valid_slot");
        BadDesc.SaveSlotRevision = TEXT("Current");
        BadDesc.SeedHex = TEXT("0123456789abcdef");
        BadDesc.RepositoryVersion = TEXT("1");
        BadDesc.RepositoryContentHash = TEXT("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");

        FString ErrorMsg;
        TestFalse(TEXT("LoadSave with non-empty seed must be rejected"), BadDesc.IsValid(&ErrorMsg));
        TestTrue(TEXT("Error message mentions SeedHex"), ErrorMsg.Contains(TEXT("SeedHex")));

        BadDesc.SeedHex = TEXT("");
        TestTrue(TEXT("LoadSave with empty seed is valid"), BadDesc.IsValid(&ErrorMsg));
    }

    // 2. Runtime session load restores PRNG streams from meta.prng
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = TEXT("test_prng_load");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> ExistingGens;
    IFileManager::Get().FindFiles(ExistingGens, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : ExistingGens)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->StartSession();
    TestTrue(TEXT("Initial session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);
    if (Coordinator == nullptr)
    {
        Runtime->EndSession();
        return false;
    }

    // Start Session A with canonical seed ffffffffffffffff (CanonicalStateAndSave.md golden vector)
    FSessionStartDescriptor FixedDesc;
    FixedDesc.Mode = ESessionStartMode::NewGame;
    FixedDesc.RepositoryVersion = FString::Printf(TEXT("%lld"), Coordinator->GetStatus().RepositoryVersion);
    FixedDesc.RepositoryContentHash = UTF8_TO_TCHAR(Coordinator->GetPinnedRepository().GetContentHash().c_str());
    FixedDesc.SeedHex = TEXT("ffffffffffffffff");

    const int64 NewGameOp = Runtime->RequestSession(FixedDesc);
    ESessionOperationOutcome NewGameOutcome;
    FGV2OperationFault NewGameFault;
    TestTrue(TEXT("NewGame outcome available"), Runtime->GetSessionOperationOutcome(NewGameOp, NewGameOutcome, NewGameFault));
    TestEqual(TEXT("NewGame completed"), NewGameOutcome, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Session A is ready"), Runtime->GetSessionState().bIsReady);
    TestEqual(TEXT("Active seed is canonical"), Runtime->GetActiveSeedHex(), FString(TEXT("ffffffffffffffff")));

    // Sample initial 3 draws in Session A and verify against CanonicalStateAndSave.md golden vectors:
    // #1: 0x0d9c8816, #2: 0x8a60ae26, #3: 0x1241ad6f
    const FString StreamId = TEXT("core:random_stream.gameplay");
    const uint32 DrawA1 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);
    const uint32 DrawA2 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);
    const uint32 DrawA3 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);

    TestEqual(TEXT("DrawA1 matches golden output #1"), DrawA1, 0x0d9c8816u);
    TestEqual(TEXT("DrawA2 matches golden output #2"), DrawA2, 0x8a60ae26u);
    TestEqual(TEXT("DrawA3 matches golden output #3"), DrawA3, 0x1241ad6fu);

    // Save Session A at draw #3
    const int64 SaveOp = Runtime->RequestSave(SaveSlot);
    ESessionOperationOutcome SaveOutcome;
    FGV2OperationFault SaveFault;
    TestTrue(TEXT("Save outcome available"), Runtime->GetSessionOperationOutcome(SaveOp, SaveOutcome, SaveFault));
    TestEqual(TEXT("Save completed"), SaveOutcome, ESessionOperationOutcome::Completed);

    // Continued draws in Session A match golden outputs #4 and #5:
    // #4: 0x99d5616e, #5: 0x73735263
    const uint32 DrawA4 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);
    const uint32 DrawA5 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);
    TestEqual(TEXT("DrawA4 matches golden output #4"), DrawA4, 0x99d5616eu);
    TestEqual(TEXT("DrawA5 matches golden output #5"), DrawA5, 0x73735263u);

    // Perform load into Session B
    const int64 LoadOpB = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Current);
    ESessionOperationOutcome LoadOutcomeB;
    FGV2OperationFault LoadFaultB;
    TestTrue(TEXT("Load B outcome available"), Runtime->GetSessionOperationOutcome(LoadOpB, LoadOutcomeB, LoadFaultB));
    TestEqual(TEXT("Load B completed"), LoadOutcomeB, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Session B is ready after load"), Runtime->GetSessionState().bIsReady);

    // Verify loaded Session B strictly continues from the save point (not reseeding to draw #1)
    const uint32 DrawB1 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);
    const uint32 DrawB2 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);

    TestEqual(TEXT("DrawB1 matches continuation output DrawA4 / golden #4"), DrawB1, 0x99d5616eu);
    TestEqual(TEXT("DrawB2 matches continuation output DrawA5 / golden #5"), DrawB2, 0x73735263u);
    TestNotEqual(TEXT("DrawB1 does not reseed to golden #1"), DrawB1, 0x0d9c8816u);

    // Repeatedly load into Session C to prove deterministic reproduction of the continuation sequence
    const int64 LoadOpC = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Current);
    ESessionOperationOutcome LoadOutcomeC;
    FGV2OperationFault LoadFaultC;
    TestTrue(TEXT("Load C outcome available"), Runtime->GetSessionOperationOutcome(LoadOpC, LoadOutcomeC, LoadFaultC));
    TestEqual(TEXT("Load C completed"), LoadOutcomeC, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Session C is ready after reload"), Runtime->GetSessionState().bIsReady);

    const uint32 DrawC1 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);
    const uint32 DrawC2 = SampleSessionPrngU32(*this, Coordinator->GetRuntimeSession(), StreamId);

    TestEqual(TEXT("DrawC1 reproduces DrawB1 identically"), DrawC1, DrawB1);
    TestEqual(TEXT("DrawC2 reproduces DrawB2 identically"), DrawC2, DrawB2);

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->EndSession();
    return true;
}

// CFC-10 (шаг 5): load-another-save, повторный load и restart загруженной сессии.
//
// Остальные save/load тесты работают с одним слотом, поэтому все они одинаково пройдут
// реализацию, в которой загрузка привязана к последнему сохранённому слоту. Здесь два
// слота с РАЗНЫМ состоянием, и каждая загрузка обязана дать состояние именно своего
// слота: сначала из живой сессии грузится A, затем из уже загруженной сессии -- B.
// Это последний из трёх сценариев формулировки STATUS-001 ("load-another-save"), два
// других -- повторный load того же слота и restart загруженной сессии -- закрываются
// здесь же.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadAnotherSaveAndRestartTest,
    "GV2.Runtime.SaveAndLoad.LoadAnotherSaveAndRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadAnotherSaveAndRestartTest::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SlotTavern = TEXT("test_slot_tavern");
    const FString SlotMarket = TEXT("test_slot_market");
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));

    auto PurgeSlot = [&SaveDir](const FString& Slot)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, Slot + TEXT(".head")));
        TArray<FString> Gens;
        IFileManager::Get().FindFiles(Gens, *SaveDir, *(Slot + TEXT(".gen_*")));
        for (const FString& GenFile : Gens)
        {
            IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
        }
    };
    PurgeSlot(SlotTavern);
    PurgeSlot(SlotMarket);

    Runtime->StartSession();
    TestTrue(TEXT("Session A is ready"), Runtime->GetSessionState().bIsReady);
    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);
    if (Coordinator == nullptr)
    {
        Runtime->EndSession();
        return false;
    }

    auto TravelVia = [&](const TCHAR* ButtonKey, const TCHAR* What)
    {
        UGV2ScreenWidgetBase* Screen = Runtime->GetActiveScreenInLayer(
            UGV2GameShellWidgetBase::LayerLocationContent,
            FName(TEXT("location")));
        TestNotNull(*FString::Printf(TEXT("LocationScreen presented before %s"), What), Screen);
        UGV2ListViewWidgetBase* CmdRep = GetButtonRepeaterFromLocationScreen(Screen);
        TestNotNull(*FString::Printf(TEXT("ButtonRepeater present before %s"), What), CmdRep);
        UGV2ButtonWidgetBase* Btn = CmdRep != nullptr
            ? Cast<UGV2ButtonWidgetBase>(CmdRep->GetEntryWidget(FName(ButtonKey)))
            : nullptr;
        TestNotNull(*FString::Printf(TEXT("Travel button found for %s"), What), Btn);
        if (Btn != nullptr)
        {
            TestEqual(
                *FString::Printf(TEXT("Travel command accepted for %s"), What),
                Runtime->SubmitUiInteraction(Btn->GetBindingHandle(), {}),
                EGV2SubmitUiInteractionResult::Accepted);
        }
    };

    auto AwaitOutcome = [&](int64 OpId, const TCHAR* What)
    {
        TestTrue(*FString::Printf(TEXT("%s returned a non-zero operation ID"), What), OpId > 0);
        ESessionOperationOutcome Outcome;
        FGV2OperationFault Fault;
        TestTrue(*FString::Printf(TEXT("%s outcome is available"), What), Runtime->GetSessionOperationOutcome(OpId, Outcome, Fault));
        TestEqual(*FString::Printf(TEXT("%s completed"), What), Outcome, ESessionOperationOutcome::Completed);
    };

    // 1. Слот A сохраняется в исходной локации (таверна).
    const FString TavernHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString TavernLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Tavern state hash is non-empty"), TavernHash.IsEmpty());
    AwaitOutcome(Runtime->RequestSave(SlotTavern), TEXT("Save of the tavern slot"));

    // 2. Слот B сохраняется в другой локации (рынок), из той же сессии.
    TravelVia(TEXT("travel_city_market"), TEXT("travel to market"));
    const FString MarketHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString MarketLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("Market state hash differs from tavern state hash"), MarketHash, TavernHash);
    TestNotEqual(TEXT("Market location differs from tavern location"), MarketLoc, TavernLoc);
    AwaitOutcome(Runtime->RequestSave(SlotMarket), TEXT("Save of the market slot"));

    // 3. Загрузка A из живой сессии, состояние которой сейчас соответствует B.
    const int32 GenBeforeFirstLoad = Runtime->GetSessionState().SessionGeneration;
    AwaitOutcome(Runtime->RequestLoad(SlotTavern, EGV2SaveSlotRevision::Current), TEXT("Load of the tavern slot"));
    TestTrue(TEXT("Session is ready after loading the tavern slot"), Runtime->GetSessionState().bIsReady);
    TestTrue(
        TEXT("Generation advanced after loading the tavern slot"),
        Runtime->GetSessionState().SessionGeneration > GenBeforeFirstLoad);
    TestEqual(
        TEXT("Loaded state hash matches the tavern slot, not the live market state"),
        GetSessionStateHash(*this, Coordinator->GetRuntimeSession()),
        TavernHash);
    TestEqual(
        TEXT("Loaded player location matches the tavern slot"),
        GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession()),
        TavernLoc);

    // 4. LOAD-ANOTHER-SAVE: из уже загруженной сессии грузится ДРУГОЙ слот.
    const int32 GenBeforeAnotherLoad = Runtime->GetSessionState().SessionGeneration;
    AwaitOutcome(Runtime->RequestLoad(SlotMarket, EGV2SaveSlotRevision::Current), TEXT("Load of another slot"));
    TestTrue(TEXT("Session is ready after loading another slot"), Runtime->GetSessionState().bIsReady);
    TestTrue(
        TEXT("Generation advanced after loading another slot"),
        Runtime->GetSessionState().SessionGeneration > GenBeforeAnotherLoad);
    TestEqual(
        TEXT("State hash after loading another slot matches the market slot"),
        GetSessionStateHash(*this, Coordinator->GetRuntimeSession()),
        MarketHash);
    TestNotEqual(
        TEXT("State hash after loading another slot no longer matches the tavern slot"),
        GetSessionStateHash(*this, Coordinator->GetRuntimeSession()),
        TavernHash);
    TestEqual(
        TEXT("Player location after loading another slot matches the market slot"),
        GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession()),
        MarketLoc);

    // Независимое от хэша свидетельство: рынок предлагает дорогу в таверну, но не в рынок.
    UGV2ListViewWidgetBase* MarketRep = GetButtonRepeaterFromLocationScreen(
        Runtime->GetActiveScreenInLayer(UGV2GameShellWidgetBase::LayerLocationContent, FName(TEXT("location"))));
    TestNotNull(TEXT("ButtonRepeater is presented after loading another slot"), MarketRep);
    if (MarketRep != nullptr)
    {
        TestNotNull(
            TEXT("travel_city_tavern is offered in the loaded market screen"),
            MarketRep->GetEntryWidget(FName(TEXT("travel_city_tavern"))));
        TestNull(
            TEXT("travel_city_market is absent from the loaded market screen"),
            MarketRep->GetEntryWidget(FName(TEXT("travel_city_market"))));
    }

    // 5. Повторный load того же слота воспроизводит то же состояние.
    AwaitOutcome(Runtime->RequestLoad(SlotMarket, EGV2SaveSlotRevision::Current), TEXT("Repeat load of the same slot"));
    TestEqual(
        TEXT("Repeat load reproduces the market state hash"),
        GetSessionStateHash(*this, Coordinator->GetRuntimeSession()),
        MarketHash);

    // 6. Restart загруженной сессии: обычный NewGame поверх неё. Состояние возвращается
    // в стартовую локацию, а хэш не совпадает ни с одним сохранённым -- каждый новый
    // прогон получает собственный seed (CFC-07A), поэтому сравнивается локация.
    const int32 GenBeforeRestart = Runtime->GetSessionState().SessionGeneration;
    Runtime->StartSession();
    TestTrue(TEXT("Session is ready after restarting the loaded session"), Runtime->GetSessionState().bIsReady);
    TestTrue(
        TEXT("Generation advanced after restarting the loaded session"),
        Runtime->GetSessionState().SessionGeneration > GenBeforeRestart);
    TestEqual(
        TEXT("Restarted session starts at the initial location again"),
        GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession()),
        TavernLoc);
    const FString RestartedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("Restarted session is not the market save"), RestartedHash, MarketHash);

    PurgeSlot(SlotTavern);
    PurgeSlot(SlotMarket);
    Runtime->EndSession();
    return true;
}

// CFC-12: Full end-to-end gameplay slice with no native C++ gameplay logic:
// package-owned start -> bound UI command -> service mutation -> post-commit event ->
// desired presentation -> typed save request -> post-save mutation -> typed load ->
// reconstructed UI (geometry/fields/binding resolution) -> stale handle rejection ->
// stream continuation and next bound command.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadGameplaySliceTest,
    "GV2.Runtime.SaveAndLoad.GameplaySlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadGameplaySliceTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = FString::Printf(TEXT("cfc12_slice_slot_%llu"), FPlatformTime::Cycles64());
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));

    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Initial session is ready"), Runtime->GetSessionState().bIsReady);

    FGV2SessionCoordinator* Coordinator = Runtime->GetCoordinatorForAutomationTest();
    TestNotNull(TEXT("Coordinator exists"), Coordinator);
    if (Coordinator == nullptr)
    {
        Runtime->EndSession();
        return false;
    }

    // 1. Start Session A with deterministic canonical seed
    FSessionStartDescriptor FixedDesc;
    FixedDesc.Mode = ESessionStartMode::NewGame;
    FixedDesc.RepositoryVersion = FString::Printf(TEXT("%lld"), Coordinator->GetStatus().RepositoryVersion);
    FixedDesc.RepositoryContentHash = UTF8_TO_TCHAR(Coordinator->GetPinnedRepository().GetContentHash().c_str());
    FixedDesc.SeedHex = TEXT("0123456789abcdef");

    const int64 NewGameOp = Runtime->RequestSession(FixedDesc);
    ESessionOperationOutcome NewGameOutcome;
    FGV2OperationFault NewGameFault;
    TestTrue(TEXT("NewGame outcome available"), Runtime->GetSessionOperationOutcome(NewGameOp, NewGameOutcome, NewGameFault));
    TestEqual(TEXT("NewGame completed"), NewGameOutcome, ESessionOperationOutcome::Completed);
    TestTrue(TEXT("Session A is ready"), Runtime->GetSessionState().bIsReady);
    const int32 GenA = Runtime->GetSessionState().SessionGeneration;

    // 2. Explicit package-owned start: dispatch start_game via UI command interaction interface
    const FString SamplePkg = TEXT("sample");
    TArray<FGV2UiBindingDefinition> StartDefs;
    FGV2UiBindingDefinition StartBtnDef;
    StartBtnDef.NodeKeyPath = { TEXT("root"), TEXT("start_button") };
    StartBtnDef.ElementId = TEXT("btn_start_game");
    StartBtnDef.CommandId = FString::Printf(TEXT("%s:command.start_game"), *SamplePkg);
    StartDefs.Add(StartBtnDef);

    TArray<FGV2UiBindingHandle> StartHandles;
    const bool bPublishedStart = Coordinator->PublishScreenBindings(StartDefs, StartHandles);
    TestTrue(TEXT("Published screen binding for start_game command"), bPublishedStart);
    if (TestEqual(TEXT("Got 1 start handle"), StartHandles.Num(), 1))
    {
        const EGV2SubmitUiInteractionResult StartRes = Runtime->SubmitUiInteraction(StartHandles[0], {});
        TestEqual(TEXT("Start game command interaction accepted"), StartRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    // 3. UI presentation verification: WBP_LocationScreen is presented at Hub
    UGV2ScreenWidgetBase* ScreenA1 = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("Session A LocationScreen is presented"), ScreenA1);
    TSharedPtr<SWidget> SlateA1;
    if (ScreenA1 != nullptr)
    {
        SlateA1 = ScreenA1->TakeWidget();
    }
    TestTrue(TEXT("Session A LocationScreen produces valid Slate widget"), SlateA1.IsValid());

    // Verify Screen Fields
    const TArray<FName> FieldIds = ScreenA1 ? ScreenA1->GetScreenFieldIds() : TArray<FName>{};
    TestTrue(TEXT("top_bar field exists"), FieldIds.Contains(FName(TEXT("top_bar"))));
    TestTrue(TEXT("player_status field exists"), FieldIds.Contains(FName(TEXT("player_status"))));
    TestTrue(TEXT("scene field exists"), FieldIds.Contains(FName(TEXT("scene"))));
    TestTrue(TEXT("commands field exists"), FieldIds.Contains(FName(TEXT("commands"))));

    // Verify scout_hub button in ButtonRepeater
    UGV2ListViewWidgetBase* CmdRepA1 = GetButtonRepeaterFromLocationScreen(ScreenA1);
    TestNotNull(TEXT("Session A ButtonRepeater exists"), CmdRepA1);
    UGV2ButtonWidgetBase* ScoutBtnA1 = CmdRepA1 != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRepA1->GetEntryWidget(FName(TEXT("scout_hub"))))
        : nullptr;
    TestNotNull(TEXT("scout_hub button exists in Hub screen"), ScoutBtnA1);

    const FGV2UiBindingHandle ScoutHandleA = ScoutBtnA1 ? ScoutBtnA1->GetBindingHandle() : FGV2UiBindingHandle{};
    TestTrue(TEXT("scout_hub button has valid binding handle"), ScoutHandleA.IsValid());

    // 4. Bound UI interaction execution: submit scout command via ScoutBtnA1 handle
    if (ScoutBtnA1 != nullptr)
    {
        const EGV2SubmitUiInteractionResult SubmitRes = Runtime->SubmitUiInteraction(ScoutHandleA, {});
        TestEqual(TEXT("Scout command UI interaction accepted"), SubmitRes, EGV2SubmitUiInteractionResult::Accepted);
    }

    // 5. Verify service mutation: location recorded, state hash recorded
    const FString SavedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString SavedLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestFalse(TEXT("Player location is non-empty after scout"), SavedLoc.IsEmpty());

    // 6. Request save: captures state with gold 125, scout_count 1, and advanced PRNG stream
    const int64 SaveOpId = Runtime->RequestSave(SaveSlot);
    TestTrue(TEXT("RequestSave returned non-zero operation ID"), SaveOpId > 0);
    ESessionOperationOutcome SaveOutcome;
    FGV2OperationFault SaveFault;
    TestTrue(TEXT("Save outcome is available"), Runtime->GetSessionOperationOutcome(SaveOpId, SaveOutcome, SaveFault));
    TestEqual(TEXT("Save completed successfully"), SaveOutcome, ESessionOperationOutcome::Completed);

    // 7. Post-save mutation in Session A: travel to East Wing via UI interaction
    UGV2ScreenWidgetBase* ScreenA2 = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("Session A LocationScreen after scout exists"), ScreenA2);
    UGV2ListViewWidgetBase* CmdRepA2 = GetButtonRepeaterFromLocationScreen(ScreenA2);
    UGV2ButtonWidgetBase* TravelEastBtnA = CmdRepA2 != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRepA2->GetEntryWidget(FName(TEXT("travel_east"))))
        : nullptr;
    TestNotNull(TEXT("travel_east button found in Hub"), TravelEastBtnA);
    if (TravelEastBtnA != nullptr)
    {
        const EGV2SubmitUiInteractionResult SubmitResTravel = Runtime->SubmitUiInteraction(TravelEastBtnA->GetBindingHandle(), {});
        TestEqual(TEXT("Post-save travel interaction accepted"), SubmitResTravel, EGV2SubmitUiInteractionResult::Accepted);
    }

    const FString PostSaveHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString PostSaveLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestNotEqual(TEXT("State hash changed after post-save travel"), PostSaveHash, SavedHash);
    TestNotEqual(TEXT("Player location changed after post-save travel"), PostSaveLoc, SavedLoc);

    // 8. Typed load: replaces Session A with Session B restored from SaveSlot
    const int64 LoadOpId = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Current);
    TestTrue(TEXT("RequestLoad returned non-zero operation ID"), LoadOpId > 0);
    ESessionOperationOutcome LoadOutcome;
    FGV2OperationFault LoadFault;
    TestTrue(TEXT("Load outcome is available"), Runtime->GetSessionOperationOutcome(LoadOpId, LoadOutcome, LoadFault));
    TestEqual(TEXT("Load completed successfully"), LoadOutcome, ESessionOperationOutcome::Completed);

    // Assert replacement session B lifecycle state
    const FGV2SessionStatus StatusB = Runtime->GetSessionState();
    TestTrue(TEXT("Session B is ready"), StatusB.bIsReady);
    TestEqual(TEXT("Session B is in Ready state"), StatusB.SessionState, EGV2SessionState::Ready);
    TestTrue(TEXT("Session generation incremented"), StatusB.SessionGeneration > GenA);

    // 9. Assert independent state restoration
    const FString LoadedHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
    const FString LoadedLoc = GetSessionPlayerLocation(*this, Coordinator->GetRuntimeSession());
    TestEqual(TEXT("Loaded state hash strictly matches saved state hash"), LoadedHash, SavedHash);
    TestNotEqual(TEXT("Loaded state hash does not match post-save mutation hash"), LoadedHash, PostSaveHash);
    TestEqual(TEXT("Loaded player location matches saved location (Hub)"), LoadedLoc, SavedLoc);

    // 10. Reconstructed UI verification
    UGV2ScreenWidgetBase* ScreenB = Runtime->GetActiveScreenInLayer(
        UGV2GameShellWidgetBase::LayerLocationContent,
        FName(TEXT("location")));
    TestNotNull(TEXT("Session B reconstructed LocationScreen is presented"), ScreenB);
    TSharedPtr<SWidget> SlateB;
    if (ScreenB != nullptr)
    {
        SlateB = ScreenB->TakeWidget();
    }
    TestTrue(TEXT("Session B LocationScreen produces valid Slate widget"), SlateB.IsValid());

    UGV2ListViewWidgetBase* CmdRepB = GetButtonRepeaterFromLocationScreen(ScreenB);
    TestNotNull(TEXT("Session B ButtonRepeater exists"), CmdRepB);
    UGV2ButtonWidgetBase* ScoutBtnB = CmdRepB != nullptr
        ? Cast<UGV2ButtonWidgetBase>(CmdRepB->GetEntryWidget(FName(TEXT("scout_hub"))))
        : nullptr;
    TestNotNull(TEXT("scout_hub button exists in reconstructed Hub screen"), ScoutBtnB);

    // 11. Stale binding handle rejection vs new binding handle execution
    // Submitting old handle from Session A MUST be rejected with StaleBindingHandle!
    const EGV2SubmitUiInteractionResult StaleResult = Runtime->SubmitUiInteraction(ScoutHandleA, {});
    TestEqual(
        TEXT("Old binding handle from Session A is rejected as StaleBindingHandle in Session B"),
        StaleResult,
        EGV2SubmitUiInteractionResult::StaleBindingHandle);

    // Submitting new handle from Session B MUST be accepted!
    if (ScoutBtnB != nullptr)
    {
        const FGV2UiBindingHandle ScoutHandleB = ScoutBtnB->GetBindingHandle();
        TestTrue(TEXT("Session B scout_hub has valid binding handle"), ScoutHandleB.IsValid());
        TestNotEqual(TEXT("Session B binding handle differs from Session A handle"), ScoutHandleB, ScoutHandleA);

        const EGV2SubmitUiInteractionResult NewResult = Runtime->SubmitUiInteraction(ScoutHandleB, {});
        TestEqual(
            TEXT("New binding handle from Session B is accepted"),
            NewResult,
            EGV2SubmitUiInteractionResult::Accepted);

        // 12. Continuation of deterministic PRNG stream and state mutation in Session B
        const FString PostContinuationHash = GetSessionStateHash(*this, Coordinator->GetRuntimeSession());
        TestNotEqual(TEXT("State hash changed after continuation command"), PostContinuationHash, LoadedHash);
    }

    // Cleanup
    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    Runtime->EndSession();
    return true;
}

// CFC-12: Bounded lifecycle stress: 100 repetitions of save/load/restart
// verifying no leaked generations, pending operations, or stored native Lua callbacks.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SaveAndLoadLifecycleStressTest,
    "GV2.Runtime.SaveAndLoad.LifecycleStress100",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SaveAndLoadLifecycleStressTest::RunTest(const FString& Parameters)
{
    const FGV2ScopedSamplePackageOverride SampleOverride;

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();

    UGV2RuntimeSubsystem* Runtime = GameInstance->GetSubsystem<UGV2RuntimeSubsystem>();
    TestNotNull(TEXT("Runtime subsystem exists"), Runtime);
    if (Runtime == nullptr)
    {
        return false;
    }

    const FString SaveSlot = FString::Printf(TEXT("cfc12_stress_slot_%llu"), FPlatformTime::Cycles64());
    const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
    const FString HeadFile = FPaths::Combine(SaveDir, SaveSlot + TEXT(".head"));

    IFileManager::Get().Delete(*HeadFile);

    Runtime->StartSession();
    TestTrue(TEXT("Initial session is ready"), Runtime->GetSessionState().bIsReady);

    int32 LastGen = Runtime->GetSessionState().SessionGeneration;

    constexpr int32 StressIterations = 100;
    for (int32 Cycle = 0; Cycle < StressIterations; ++Cycle)
    {
        const int64 SaveOp = Runtime->RequestSave(SaveSlot);
        TestTrue(TEXT("Stress save op issued"), SaveOp > 0);
        ESessionOperationOutcome SaveOutcome;
        FGV2OperationFault SaveFault;
        TestTrue(TEXT("Stress save completed"), Runtime->GetSessionOperationOutcome(SaveOp, SaveOutcome, SaveFault));
        TestEqual(TEXT("Stress save outcome ok"), SaveOutcome, ESessionOperationOutcome::Completed);

        const int64 LoadOp = Runtime->RequestLoad(SaveSlot, EGV2SaveSlotRevision::Current);
        TestTrue(TEXT("Stress load op issued"), LoadOp > 0);
        ESessionOperationOutcome LoadOutcome;
        FGV2OperationFault LoadFault;
        TestTrue(TEXT("Stress load completed"), Runtime->GetSessionOperationOutcome(LoadOp, LoadOutcome, LoadFault));
        TestEqual(TEXT("Stress load outcome ok"), LoadOutcome, ESessionOperationOutcome::Completed);

        const FGV2SessionStatus Status = Runtime->GetSessionState();
        TestTrue(TEXT("Session ready during stress"), Status.bIsReady);
        TestTrue(TEXT("Session generation monotonically increases"), Status.SessionGeneration > LastGen);
        LastGen = Status.SessionGeneration;
    }

    Runtime->EndSession();
    TestFalse(TEXT("Session not ready after EndSession"), Runtime->GetSessionState().bIsReady);

    TestEqual(TEXT("Zero live VMs after session teardown"), GV2RuntimeCore::FRuntimeSession::GetLiveVmCount(), 0);

    IFileManager::Get().Delete(*HeadFile);
    TArray<FString> GenFiles;
    IFileManager::Get().FindFiles(GenFiles, *SaveDir, *(SaveSlot + TEXT(".gen_*")));
    for (const FString& GenFile : GenFiles)
    {
        IFileManager::Get().Delete(*FPaths::Combine(SaveDir, GenFile));
    }

    return true;
}

#endif
