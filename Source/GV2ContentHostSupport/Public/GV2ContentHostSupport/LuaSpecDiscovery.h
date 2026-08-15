#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include <filesystem>
#include <string>
#include <vector>

namespace GV2ContentHostSupport
{
struct FLuaSpecFile
{
    // Forward-slash-normalized path relative to the spec root (e.g.
    // "world/current_location.lua"). Never an absolute path.
    std::string RelativePath;
    std::string Source;
};

/**
 * Recursively discovers `*.lua` spec files under Root (TAS-02, Tests/Lua
 * format — see BuildAndTooling.md "Формат Lua-спеки"). Order is
 * deterministic: entries are sorted by RelativePath (byte-wise), regardless
 * of the underlying filesystem's own iteration order.
 *
 * A missing Root is not an error: it yields an empty result, matching the
 * TAS-02 contract that an absent Tests/Lua directory is a valid (empty)
 * spec set, not a discovery failure.
 */
GV2_CONTENT_HOST_SUPPORT_API std::vector<FLuaSpecFile> DiscoverLuaSpecFiles(const std::filesystem::path& Root);
}
