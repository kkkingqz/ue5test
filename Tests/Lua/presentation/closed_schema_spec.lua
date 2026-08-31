-- BAI-03: Closed Schema Lua Spec
-- Tests that location presentation builders emit strictly conforming closed schema tables with no extra keys.
--
-- UPP-26 note: the allowed-key sets below are hand-copied from the
-- ui_field_location_*.v1 schemas, not read from them -- Lua has no way to
-- see a compiled or raw schema today (RepositoryBuilder discards its
-- FSchemaRegistry after building, and the Lua sandbox exposes no schema
-- lookup). GV2ContentCore::ValidateUiFieldValue already does real
-- schema-driven validation; wiring it (or the raw schema) up to Lua specs
-- is a deliberately deferred gap, tracked in LocationScreen.md's UPP-26
-- audit note, not a general substitute for this test.

local location_presenter = require("textsystem:module.presentation.location_presenter")

local function has_only_keys(tbl, allowed_set)
    if type(tbl) ~= "table" then return false end
    for k, _ in pairs(tbl) do
        if not allowed_set[k] then
            return false, k
        end
    end
    return true
end

return {
    location_screen_fields_comply_with_closed_schemas = function()
        local req = location_presenter.build_screen_request("rh:location.city.tavern")
        assert(req ~= nil, "screen request must not be nil")
        assert(req.fields ~= nil, "screen fields must exist")

        -- 1. Check top_bar
        local top_bar = req.fields.top_bar
        assert(top_bar ~= nil and top_bar.value ~= nil)
        local top_bar_keys = { day = true, location = true, primary_resource = true }
        local ok, bad_k = has_only_keys(top_bar.value, top_bar_keys)
        assert(ok, "top_bar has unexpected key: " .. tostring(bad_k))

        -- 2. Check scene
        local scene = req.fields.scene
        assert(scene ~= nil and scene.value ~= nil)
        local scene_keys = {
            background_tile_resource_id = true,
            background_resource_id = true,
            context_text = true,
            characters = true
        }
        ok, bad_k = has_only_keys(scene.value, scene_keys)
        assert(ok, "scene has unexpected key: " .. tostring(bad_k))

        -- Check scene characters
        local char_keys = { key = true, resource_id = true }
        for _, char in ipairs(scene.value.characters) do
            ok, bad_k = has_only_keys(char, char_keys)
            assert(ok, "character element has unexpected key: " .. tostring(bad_k))
        end

        -- 3. Check player_status
        local status = req.fields.player_status
        assert(status ~= nil and status.value ~= nil)
        local status_keys = {
            name = true,
            portrait_resource_id = true,
            meters = true,
            items = true,
            effects = true
        }
        ok, bad_k = has_only_keys(status.value, status_keys)
        assert(ok, "player_status has unexpected key: " .. tostring(bad_k))

        -- Check meters
        local meter_keys = { key = true, percent = true, label = true }
        for _, meter in ipairs(status.value.meters) do
            ok, bad_k = has_only_keys(meter, meter_keys)
            assert(ok, "meter element has unexpected key: " .. tostring(bad_k))
        end

        -- Check item and effect icon elements
        local icon_keys = { key = true, resource_id = true }
        for _, collection in ipairs({ status.value.items, status.value.effects }) do
            for _, icon in ipairs(collection) do
                ok, bad_k = has_only_keys(icon, icon_keys)
                assert(ok, "icon element has unexpected key: " .. tostring(bad_k))
            end
        end

        -- Item identity comes from the item instance, never from its position or icon.
        for index, icon in ipairs(status.value.items) do
            assert(icon.key ~= nil and icon.key ~= "", "item icon must carry a key")
            assert(icon.key ~= "item_" .. tostring(index - 1), "item key must not be the array position")
            assert(icon.key ~= icon.resource_id, "item key must not be its resource id")
        end

        -- 4. Check commands
        local commands = req.fields.commands
        assert(commands ~= nil and commands.value ~= nil)
        local commands_keys = { items = true }
        ok, bad_k = has_only_keys(commands.value, commands_keys)
        assert(ok, "commands has unexpected key: " .. tostring(bad_k))

        -- Check command items
        local item_keys = { key = true, text = true, binding = true }
        for _, item in ipairs(commands.value.items) do
            ok, bad_k = has_only_keys(item, item_keys)
            assert(ok, "command item has unexpected key: " .. tostring(bad_k))
        end
    end,

    -- PCC-03: An element property declared in schema but absent from entry widget
    -- capability contract must not silently pass through; closed-schema checking rejects unexpected keys.
    element_schema_rejects_unsupported_properties = function()
        local bad_element = {
            key = "meter_1",
            percent = 0.5,
            label = { text_id = "core:text.ok" },
            unsupported_extra = "unbacked_value",
        }
        local allowed_meter_keys = { key = true, percent = true, label = true }
        local ok, bad_k = has_only_keys(bad_element, allowed_meter_keys)
        assert(not ok, "Element with unsupported property must not pass closed-schema check")
        assert(bad_k == "unsupported_extra", "Unexpected key must match unsupported_extra")
    end,
}
