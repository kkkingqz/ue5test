-- SVC-04: Location Scene Boundary Spec
-- Tests schema shape, character collections, fallbacks, and validation for textsystem:schema.ui_field.location_scene.v1.

local location_presenter = require("textsystem:module.presentation.location_presenter")
local screens = require("core:module.presentation.screen_requests")

return {
    location_scene_tavern_has_character_with_key_and_resource = function()
        local req = location_presenter.build_screen_request("rh:location.city.tavern")
        assert(req ~= nil, "screen request must be generated for tavern")
        local scene_field = req.fields.scene
        assert(scene_field ~= nil, "scene field must exist")
        assert(scene_field.schema_id == "textsystem:schema.ui_field.location_scene.v1")
        assert(scene_field.value.background_resource_id == "rh:resource.location.tavern")
        assert(type(scene_field.value.characters) == "table")
        assert(#scene_field.value.characters == 1)
        assert(scene_field.value.characters[1].key == "rh:resource.character.tavern_keeper")
        assert(scene_field.value.characters[1].resource_id == "rh:resource.character.tavern_keeper")
    end,

    location_scene_gate_and_market_have_empty_characters = function()
        local req_market = location_presenter.build_screen_request("rh:location.city.market")
        assert(req_market ~= nil)
        assert(type(req_market.fields.scene.value.characters) == "table")
        assert(#req_market.fields.scene.value.characters == 0)

        local req_gate = location_presenter.build_screen_request("rh:location.city.gate")
        assert(req_gate ~= nil)
        assert(type(req_gate.fields.scene.value.characters) == "table")
        assert(#req_gate.fields.scene.value.characters == 0)
    end,
}
