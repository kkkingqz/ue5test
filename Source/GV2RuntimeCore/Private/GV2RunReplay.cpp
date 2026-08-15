#include "GV2RuntimeCore/GV2RunReplay.h"

namespace GV2RuntimeCore
{
bool ReplayRunManifest(
    const FRunManifest& Manifest,
    const GV2ContentCore::FRepositoryReadHandle& RepositoryHandle,
    const std::vector<FRuntimeSource>& RuntimeSources,
    FRunResult& OutResult,
    FRuntimeFault& OutFault)
{
    OutResult = FRunResult{};
    OutResult.bSuccess = false;

    if (Manifest.LuaReleaseNumber != FRuntimeSession::LuaReleaseNumber)
    {
        OutFault.Code = "core:fault.run_manifest.lua_release_mismatch";
        OutFault.Message = "Manifest Lua release number mismatch";
        OutResult.FaultCode = OutFault.Code;
        return false;
    }

    if (Manifest.RepositoryContentHash != RepositoryHandle.GetContentHash())
    {
        OutFault.Code = "core:fault.run_manifest.repository_hash_mismatch";
        OutFault.Message = "Manifest repository content hash mismatch";
        OutResult.FaultCode = OutFault.Code;
        return false;
    }

    FRuntimeSession Runtime;
    if (!Runtime.Start(1, RepositoryHandle, RuntimeSources, OutFault))
    {
        OutResult.FaultCode = OutFault.Code;
        return false;
    }

    for (const auto& Cmd : Manifest.AcceptedCommands)
    {
        FCommandRequest Request;
        Request.CommandId = Cmd.CommandId;
        Request.Sequence = Cmd.Sequence;
        Request.Args = Cmd.Args;

        if (!Runtime.DispatchCommand(Request, OutFault))
        {
            OutResult.FaultCode = OutFault.Code;
            Runtime.Stop();
            return false;
        }
        OutResult.ExecutedCommandsCount++;
    }

    std::optional<FScreenRequest> PendingScreen;
    if (Runtime.TakePendingScreen(PendingScreen, OutFault) && PendingScreen.has_value())
    {
        OutResult.FinalScreenId = PendingScreen->ScreenId;
        for (const auto& Field : PendingScreen->Fields)
        {
            OutResult.FinalScreenFields.emplace(Field.FieldId, Field.Value);
        }
    }

    OutResult.StateHash = Runtime.GetCanonicalStateHash();

    Runtime.Stop();

    OutResult.bSuccess = true;
    OutResult.FaultCode = "";
    return true;
}

} // namespace GV2RuntimeCore
