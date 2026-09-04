-- DCA-09 npc_portrait fixture.
-- The composite is assembled only from an existing declared composite class
-- (UGV2DeclaredCompositeWidgetBase), existing WBP_Portrait/WBP_Text children, and a
-- data-declared schema -- no Source/ change: screen -> npc_portrait composite.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("textsystem")
M.id = "textsystem:module.presentation.dca09_fixture"

local ROOT_TEMPLATE_ID = "textsystem:screen.dca09_npc_portrait_fixture"
local FIELD_SCHEMA_ID = "textsystem:schema.ui_field.npc_portrait.v1"
local requested_fixture = nil

local function publish_fixture(npc_name, portrait_resource_id)
    return M.show_screen({
        template = ROOT_TEMPLATE_ID,
        instance_key = "dca09_npc_portrait",
        fields = {
            npc_portrait = {
                schema_id = FIELD_SCHEMA_ID,
                value = {
                    name = M.text("textsystem:text.dca09.npc_name", { npc_name = npc_name }),
                    portrait_resource_id = portrait_resource_id,
                },
            },
        },
    })
end

function M.register(_ctx)
    game.commands.handlers.register("textsystem:command.debug.dca09_show", function(_req)
        requested_fixture = { npc_name = "Aria", portrait_resource_id = "textsystem:resource.ui.missing_portrait" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.dca09_update", function(_req)
        requested_fixture = { npc_name = "Merchant", portrait_resource_id = "core:resource.ui.compass" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.dca09_clear", function(_req)
        requested_fixture = nil
        return true
    end)
end

function M.build_requested_screen()
    if requested_fixture == nil then
        return nil
    end
    return publish_fixture(requested_fixture.npc_name, requested_fixture.portrait_resource_id)
end

return M
