#include "GV2ContentHostSupport/LuaSpecIdentity.h"

#include <algorithm>

namespace GV2ContentHostSupport
{
namespace
{
constexpr const char* LuaExtension = ".lua";
constexpr const char* CaseFailedCode = "LuaSpecCaseFailed";
} // namespace

std::string DeriveLuaSpecId(const std::string& RelativePath)
{
    std::string Result = RelativePath;

    const std::size_t ExtensionLength = std::char_traits<char>::length(LuaExtension);
    if (Result.size() >= ExtensionLength
        && Result.compare(Result.size() - ExtensionLength, ExtensionLength, LuaExtension) == 0)
    {
        Result.erase(Result.size() - ExtensionLength);
    }

    std::replace(Result.begin(), Result.end(), '/', '.');
    return Result;
}

FLuaSpecFailure MakeLuaSpecCaseFailure(
    const std::string& SpecId,
    const std::string& CaseId,
    const std::string& ErrorMessage)
{
    return FLuaSpecFailure{SpecId + "." + CaseId, CaseFailedCode, ErrorMessage};
}

FLuaSpecFailure MakeLuaSpecFault(
    const std::string& SpecId,
    const std::string& FaultCode,
    const std::string& FaultMessage)
{
    return FLuaSpecFailure{SpecId, FaultCode, FaultMessage};
}
} // namespace GV2ContentHostSupport
