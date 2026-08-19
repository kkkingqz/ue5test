-- EAE-11: Location Definition Extension Specification (TextSystem tier)
-- Verifies declarative Location methods on wrapped definitions and authoring surface.

local properties = require("core:module.authoring.properties")
local mutation_window = require("core:module.runtime.mutation_window")

return {
    location_definition_connectivity_methods = function()
        local hub = properties.wrap_definition("sample:location.hub")
        assert(hub ~= nil, "hub definition must be wrapped")
        assert(hub.id == "sample:location.hub")
        assert(type(hub.is_connected) == "function", "is_connected must be available on Location definition")
        assert(type(hub.require_connected) == "function", "require_connected must be available on Location definition")

        -- sample:location.hub is connected to sample:location.east
        assert(hub:is_connected("sample:location.east") == true, "hub must be connected to east")
        assert(hub:is_connected("sample:location.west") == true or hub:is_connected("sample:location.hub") == false)

        local east = properties.wrap_definition("sample:location.east")
        -- east is connected only to hub, not to west
        assert(east:is_connected("sample:location.hub") == true, "east must be connected to hub")
        assert(east:is_connected("sample:location.west") == false, "east must not be connected to west")

        -- require_connected succeeds on connected location
        local ok_conn = pcall(function()
            east:require_connected("sample:location.hub")
        end)
        assert(ok_conn == true, "require_connected on connected target must not fail")
    end,

    location_definition_access_to_repo_data = function()
        local hub = properties.wrap_definition("sample:location.hub")
        assert(hub.definition_id == "sample:location.hub")
        assert(hub.__gv2_ref == "definition")

        -- Repository fields are accessible via __index
        local def = hub.get_def()
        assert(def ~= nil, "get_def() must return repo definition")
    end,

    actor_and_location_methods_available_in_textsystem_tier = function()
        assert(game and game.entity_extensions, "game.entity_extensions must be present")
        local actor_methods = game.entity_extensions.describe("Actor")
        local loc_methods = game.entity_extensions.describe("Location")

        assert(#actor_methods >= 4, "Actor must have at least textsystem methods (is_player, is_npc, require_location, move_to)")
        assert(#loc_methods >= 2, "Location must have at least is_connected and require_connected")

        -- Verify textsystem methods are present
        assert(type(game.entity_extensions.get_method("Actor", "is_player")) == "function")
        assert(type(game.entity_extensions.get_method("Actor", "move_to")) == "function")
        assert(type(game.entity_extensions.get_method("Location", "is_connected")) == "function")
        assert(type(game.entity_extensions.get_method("Location", "require_connected")) == "function")
    end,
}
