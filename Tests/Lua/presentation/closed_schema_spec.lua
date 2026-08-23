-- BAI-03: Closed Schema Lua Spec
-- Tests that location presentation builders emit strictly conforming closed schema tables with no extra keys.

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
            item_icon_resource_ids = true,
            effect_icon_resource_ids = true
        }
        ok, bad_k = has_only_keys(status.value, status_keys)
        assert(ok, "player_status has unexpected key: " .. tostring(bad_k))

        -- Check meters
        local meter_keys = { key = true, percent = true, label = true }
        for _, meter in ipairs(status.value.meters) do
            ok, bad_k = has_only_keys(meter, meter_keys)
            assert(ok, "meter element has unexpected key: " .. tostring(bad_k))
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
}
