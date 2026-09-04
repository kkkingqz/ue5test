-- DCA-10 location_description fixture.
-- Two real WBP assets prove DCA-01's optionality on a real case (not a test
-- fixture): WBP_LocationDescription (illustration bound) and
-- WBP_LocationDescriptionNoIllustration (illustration child absent, capability
-- still declared optional) -- both assembled only from existing declared
-- composite/WBP_Text/WBP_Image and a data-declared schema, no Source/ change.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("textsystem")
M.id = "textsystem:module.presentation.dca10_fixture"

local WITH_ILLUSTRATION_TEMPLATE_ID = "textsystem:screen.dca10_location_description_fixture"
local NO_ILLUSTRATION_TEMPLATE_ID = "textsystem:screen.dca10_location_description_no_illustration_fixture"
local FIELD_SCHEMA_ID = "textsystem:schema.ui_field.location_description.v1"
local requested_fixture = nil

local function publish_with_illustration(content_key, illustration_resource_id)
    return M.show_screen({
        template = WITH_ILLUSTRATION_TEMPLATE_ID,
        instance_key = "dca10_location_description",
        fields = {
            location_description = {
                schema_id = FIELD_SCHEMA_ID,
                value = {
                    content_text = M.text("textsystem:text.dca10." .. content_key),
                    illustration_resource_id = illustration_resource_id,
                },
            },
        },
    })
end

local function publish_without_illustration(content_key)
    return M.show_screen({
        template = NO_ILLUSTRATION_TEMPLATE_ID,
        instance_key = "dca10_location_description_no_illustration",
        fields = {
            location_description = {
                schema_id = FIELD_SCHEMA_ID,
                value = {
                    -- illustration_resource_id deliberately absent: the schema declares
                    -- it optional (required = false), and the composite's own capability
                    -- is dropped for the same reason (DCA-01) since its Illustration
                    -- child does not exist on this variant of the asset.
                    content_text = M.text("textsystem:text.dca10." .. content_key),
                },
            },
        },
    })
end

function M.register(_ctx)
    game.commands.handlers.register("textsystem:command.debug.dca10_show_with_illustration", function(_req)
        requested_fixture = { kind = "with_illustration", content_key = "tavern_notice", illustration_resource_id = "textsystem:resource.ui.missing_portrait" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.dca10_show_without_illustration", function(_req)
        requested_fixture = { kind = "without_illustration", content_key = "market_notice" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.dca10_clear", function(_req)
        requested_fixture = nil
        return true
    end)
end

function M.build_requested_screen()
    if requested_fixture == nil then
        return nil
    end
    if requested_fixture.kind == "with_illustration" then
        return publish_with_illustration(requested_fixture.content_key, requested_fixture.illustration_resource_id)
    end
    return publish_without_illustration(requested_fixture.content_key)
end

return M
