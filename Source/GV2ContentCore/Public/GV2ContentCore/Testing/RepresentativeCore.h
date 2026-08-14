#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/PackageDescriptor.h"

namespace GV2ContentCore::Testing
{
/** Canonical M3 fixture descriptor shared by standalone and Unreal tests. */
GV2_CONTENT_CORE_API FPackageDescriptor MakeRepresentativeCorePackageDescriptor();
/** Canonical additive/full-override package used by M4 conformance tests. */
GV2_CONTENT_CORE_API FPackageDescriptor MakeRepresentativeTestModPackageDescriptor();
}
