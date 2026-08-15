#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include <string>

namespace GV2ContentHostSupport
{
/**
 * Derives the stable spec identifier from a spec file's RelativePath, as
 * produced by DiscoverLuaSpecFiles (forward-slash-normalized, ".lua"
 * extension present): strips the extension and replaces '/' with '.'.
 *
 * "world/current_location.lua" -> "world.current_location"
 *
 * Contains no absolute paths, addresses, or timing information (TAS-03).
 */
GV2_CONTENT_HOST_SUPPORT_API std::string DeriveLuaSpecId(const std::string& RelativePath);

/**
 * A stable, host-independent identifier for one Lua spec failure (TAS-03).
 *
 * Two shapes exist, distinguished by Code, never by parsing Identifier:
 * - Case failure: Identifier is "<spec>.<case>", Code is
 *   "LuaSpecCaseFailed". The case ran to completion as code; its assertion
 *   (or explicit error()) reported the behavior under test as wrong. This
 *   is the "логический провал кейса" (logical case failure).
 * - Spec-level fault: Identifier is "<spec>" alone (no case ever ran),
 *   Code is one of RunLuaSpec's OutFault codes (LuaSpecSyntaxError,
 *   LuaSpecLoadError, LuaSpecFormatInvalid, LuaSpecEmpty). The spec itself
 *   is malformed or failed to load — this is the "ошибка Lua внутри
 *   кейса" reading of a broken spec, distinct from a case's own failed
 *   assertion by Code alone.
 */
struct FLuaSpecFailure
{
    std::string Identifier;
    std::string Code;
    std::string Message;
};

GV2_CONTENT_HOST_SUPPORT_API FLuaSpecFailure MakeLuaSpecCaseFailure(
    const std::string& SpecId,
    const std::string& CaseId,
    const std::string& ErrorMessage);

GV2_CONTENT_HOST_SUPPORT_API FLuaSpecFailure MakeLuaSpecFault(
    const std::string& SpecId,
    const std::string& FaultCode,
    const std::string& FaultMessage);
}
