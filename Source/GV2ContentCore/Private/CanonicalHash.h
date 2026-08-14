#pragma once

#include "GV2ContentCore/Value.h"

#include <string>

namespace GV2ContentCore
{
std::string ComputeCanonicalHash(const FValue& Value);
}
