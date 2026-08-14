#pragma once

#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/SemanticValidation.h"

#include <vector>
#include <optional>
#include <string>
#include <string_view>

#include "GV2ContentCore/ParseLimits.h"

namespace GV2ContentCore
{
class GV2_CONTENT_CORE_API IContentSourceProvider
{
public:
    virtual ~IContentSourceProvider() = default;

    virtual std::optional<std::string> ReadSource(
        std::string_view PackageId,
        std::string_view RelativeSource) const = 0;
};

struct FBuildOptions final
{
    FParseLimits ParseLimits;
    const IContentSourceProvider* SourceProvider = nullptr;
    const FSemanticValidatorRegistry* SemanticValidatorRegistry = nullptr;

    bool operator==(const FBuildOptions&) const = default;
};

/**
 * Builds an immutable stage-tagged artifact from an already resolved provider
 * set. Reads every declared definition/schema source and runs all implemented
 * portable validation stages. Only RepositoryResolved is publishable.
 */
GV2_CONTENT_CORE_API FBuildResult BuildRepository(
    const std::vector<FPackageDescriptor>& PackageSet,
    const FBuildOptions& Options = {});
}
