#pragma once

#include "GV2ContentHostSupport/GV2ContentHostSupport.h"

#include <cstddef>

namespace GV2ContentHostSupport::TestHooks
{
GV2_CONTENT_HOST_SUPPORT_API std::size_t GetManifestReadCount();
GV2_CONTENT_HOST_SUPPORT_API void ResetManifestReadCount();
}
