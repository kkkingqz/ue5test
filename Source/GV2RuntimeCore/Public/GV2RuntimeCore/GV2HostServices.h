#pragma once

#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <optional>

namespace GV2RuntimeCore
{
enum class EResourceKind : std::uint8_t
{
    Unknown,
    Image,
    Audio,
    Video,
    WidgetFactory
};

struct FResourceMetadata
{
    std::string ResourceId;
    EResourceKind Kind = EResourceKind::Unknown;
    bool bAvailable = false;
};

class GV2_PORTABLE_API IResourceCatalog
{
public:
    virtual ~IResourceCatalog() = default;
    virtual std::optional<FResourceMetadata> FindMetadata(const std::string& ResourceId) const = 0;
};

class GV2_PORTABLE_API ILocalizationAdapter
{
public:
    virtual ~ILocalizationAdapter() = default;

    // Returns empty when the host intentionally preserves unresolved TextSpec.
    virtual std::optional<std::string> Resolve(
        const FTextSpec& Text,
        const std::string& Locale) const = 0;
};
}
