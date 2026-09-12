#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Value.h"

#include <string>

namespace GV2ContentCore
{
/** Computes SHA-256 canonical hash of the structured Value. */
GV2_CONTENT_CORE_API std::string ComputeCanonicalHash(const FValue& Value);

/** Returns true if Text contains exactly 64 lowercase ASCII hex characters [0-9a-f]. */
GV2_CONTENT_CORE_API bool IsCanonicalSha256(std::string_view Text) noexcept;
}
