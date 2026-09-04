-- DCA-09: npc_portrait fixture presenter spec.
-- Verifies that the fixture presenter assembles a schema-conformant envelope
-- entirely from Lua data -- name as Text, portrait as a resource StableId --
-- reaching both declared capabilities of WBP_NpcPortrait: name and
-- portrait_resource_id. The composite's real observability (that each
-- capability actually applies to and reads back from its bound child widget)
-- is proven separately by GV2.UI.CapabilityObservabilityCompositeSweep, which
-- discovers WBP_NpcPortrait by reflection with no per-asset C++ addition.

local dca09_fixture = require("textsystem:module.presentation.dca09_fixture")

return {
    npc_portrait_no_fixture_requested_returns_nil = function()
        assert(dca09_fixture.build_requested_screen() == nil,
            "no debug command issued yet -- fixture must stay unpublished")
    end,

    npc_portrait_show_command_publishes_matching_envelope = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca09_show",
            args = {},
            sequence = 9001,
        })

        local req = dca09_fixture.build_requested_screen()
        assert(req ~= nil, "screen request must be generated after dca09_show")
        assert(req.screen_id == "textsystem:screen.dca09_npc_portrait_fixture")
        assert(req.instance_key == "dca09_npc_portrait")

        local field = req.fields.npc_portrait
        assert(field ~= nil, "npc_portrait field must exist")
        assert(field.schema_id == "textsystem:schema.ui_field.npc_portrait.v1")
        assert(field.value.name.text_id == "textsystem:text.dca09.npc_name")
        assert(field.value.name.args.npc_name == "Aria")
        assert(field.value.portrait_resource_id == "textsystem:resource.ui.missing_portrait")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca09_clear",
            args = {},
            sequence = 9002,
        })
    end,

    npc_portrait_update_command_replaces_previous_values = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca09_update",
            args = {},
            sequence = 9003,
        })

        local req = dca09_fixture.build_requested_screen()
        assert(req ~= nil, "screen request must be generated after dca09_update")
        local field = req.fields.npc_portrait
        assert(field.value.name.args.npc_name == "Merchant")
        assert(field.value.portrait_resource_id == "core:resource.ui.compass")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca09_clear",
            args = {},
            sequence = 9004,
        })

        assert(dca09_fixture.build_requested_screen() == nil,
            "dca09_clear must unpublish the fixture")
    end,
}
