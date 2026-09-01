-- DUC-10 nested data-driven UI fixture.
-- The chain is intentionally assembled only from Screen Templates, generic
-- declared composites, and existing UI-field schemas:
-- screen -> tabs -> nested screen -> declared block.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("textsystem")
M.id = "textsystem:module.presentation.duc10_fixture"

local ROOT_TEMPLATE_ID = "textsystem:screen.duc10_nested_chain"
local BLOCK_TEMPLATE_ID = "textsystem:screen.duc10_nested_block"
local BLOCK_SCHEMA_ID = "textsystem:schema.ui_field.declared_composite_fixture.v1"
local requested_fixture = nil

local function publish_fixture(day, value, block_field_id)
    return M.show_screen({
        template = ROOT_TEMPLATE_ID,
        instance_key = "duc10",
        fields = {
            tabs = M.tab_container({
                default_tab = "info",
                tabs = {
                    M.tab(
                        "info",
                        M.text("textsystem:text.location.day", { day = day }),
                        BLOCK_TEMPLATE_ID,
                        {
                            {
                                field_id = block_field_id,
                                schema_id = BLOCK_SCHEMA_ID,
                                value = {
                                    day = M.text("textsystem:text.location.day", { day = day }),
                                    value = value,
                                },
                            },
                        }),
                },
            }),
        },
    })
end

function M.register(_ctx)
    game.commands.handlers.register("textsystem:command.debug.duc10_show", function(_req)
        requested_fixture = { day = 1, value = 0.25, block_field_id = "day_block" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.duc10_update", function(_req)
        requested_fixture = { day = 2, value = 0.75, block_field_id = "day_block" }
        return true
    end)

    -- The malformed third-level field is used only by the Prepare atomicity test.
    game.commands.handlers.register("textsystem:command.debug.duc10_prepare_failure", function(_req)
        requested_fixture = { day = 3, value = 0.90, block_field_id = "missing_block" }
        return true
    end)
end

function M.build_requested_screen()
    if requested_fixture == nil then
        return nil
    end
    return publish_fixture(
        requested_fixture.day,
        requested_fixture.value,
        requested_fixture.block_field_id)
end

return M
