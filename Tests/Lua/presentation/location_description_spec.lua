-- DCA-10: location_description fixture presenter spec.
-- Verifies both the with-illustration and without-illustration variants
-- assemble a schema-conformant envelope entirely from Lua data. The
-- composite's real observability, and the DCA-01 optionality drop for the
-- Illustration-less asset variant, are proven separately on the real WBP
-- assets via GV2.UI.CapabilityObservabilityCompositeSweep (no per-asset
-- C++ addition needed).

local dca10_fixture = require("textsystem:module.presentation.dca10_fixture")

return {
    location_description_no_fixture_requested_returns_nil = function()
        assert(dca10_fixture.build_requested_screen() == nil,
            "no debug command issued yet -- fixture must stay unpublished")
    end,

    location_description_with_illustration_publishes_matching_envelope = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca10_show_with_illustration",
            args = {},
            sequence = 9101,
        })

        local req = dca10_fixture.build_requested_screen()
        assert(req ~= nil, "screen request must be generated after dca10_show_with_illustration")
        assert(req.screen_id == "textsystem:screen.dca10_location_description_fixture")
        assert(req.instance_key == "dca10_location_description")

        local field = req.fields.location_description
        assert(field ~= nil, "location_description field must exist")
        assert(field.schema_id == "textsystem:schema.ui_field.location_description.v1")
        assert(field.value.content_text.text_id == "textsystem:text.dca10.tavern_notice")
        assert(field.value.illustration_resource_id == "textsystem:resource.ui.missing_portrait")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca10_clear",
            args = {},
            sequence = 9102,
        })
    end,

    location_description_without_illustration_omits_the_field_entirely = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca10_show_without_illustration",
            args = {},
            sequence = 9103,
        })

        local req = dca10_fixture.build_requested_screen()
        assert(req ~= nil, "screen request must be generated after dca10_show_without_illustration")
        assert(req.screen_id == "textsystem:screen.dca10_location_description_no_illustration_fixture")

        local field = req.fields.location_description
        assert(field ~= nil)
        assert(field.value.content_text.text_id == "textsystem:text.dca10.market_notice")
        assert(field.value.illustration_resource_id == nil,
            "the schema declares illustration_resource_id optional -- omitting the key, " ..
            "not sending an empty placeholder, is what proves real optionality")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca10_clear",
            args = {},
            sequence = 9104,
        })

        assert(dca10_fixture.build_requested_screen() == nil,
            "dca10_clear must unpublish the fixture")
    end,
}
