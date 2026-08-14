#include "GV2ContentCore/Testing/RepresentativeCore.h"

namespace GV2ContentCore::Testing
{
FPackageDescriptor MakeRepresentativeCorePackageDescriptor()
{
    return FPackageDescriptor(
        "core",
        "core",
        0,
        {
            "definitions/items.json5",
            "definitions/locations.json5",
            "definitions/resources.json5",
            "definitions/screens.json5",
            "definitions/texts.json5",
        },
        {
            FSchemaBinding("item", 1, "core:schema.definition.item.v1", "schemas/item_v1.schema.json5"),
            FSchemaBinding("location", 1, "core:schema.definition.location.v1", "schemas/location_v1.schema.json5"),
            FSchemaBinding("resource", 1, "core:schema.definition.resource.v1", "schemas/resource_v1.schema.json5"),
            FSchemaBinding("screen", 1, "core:schema.definition.screen.v1", "schemas/screen_v1.schema.json5"),
            FSchemaBinding("text", 1, "core:schema.definition.text.v1", "schemas/text_v1.schema.json5"),
        });
}

FPackageDescriptor MakeRepresentativeTestModPackageDescriptor()
{
    return FPackageDescriptor(
        "test_mod",
        "test_mod",
        1,
        {
            "definitions/locations.json5",
            "definitions/screens.json5",
            "definitions/texts.json5",
        },
        {},
        {},
        {
            FRedirectDescriptor(
                "test_mod:screen.codex_archive",
                "test_mod:screen.codex_legacy"),
            FRedirectDescriptor(
                "test_mod:screen.codex_legacy",
                "test_mod:screen.codex_lab"),
        },
        { "test_mod:screen.retired" });
}
}
