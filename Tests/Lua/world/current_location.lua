-- GEW-05 / TAS-12 / TSL-17: state.world.current_location_id as a Stable ID of kind
-- `location`, resolved against the pinned repository (TextSystem tier).

local function make_tree(current_location_id)
    return {
        meta = { schema_version = 1, save_version = 1, save_id = "", instance_counters = {}, prng = {}, time = {} },
        player = {},
        actors = {},
        item_instances = {},
        world = { current_location_id = current_location_id },
        quests = {},
        mods = {},
        definitions = {},
    }
end

return {
    valid_location_reference_accepted = function()
        local state_validator = require("core:module.runtime.state_validator")
        local ok = pcall(function()
            state_validator.validate_state_tree(make_tree("sample:location.hub"))
        end)
        assert(ok, "a Stable ID of kind 'location' resolving in the pinned repository must be accepted as world.current_location_id")
    end,

    wrong_kind_reference_rejected = function()
        local state_validator = require("core:module.runtime.state_validator")
        local ok = pcall(function()
            state_validator.validate_state_tree(make_tree("core:screen.main"))
        end)
        assert(not ok, "a Stable ID of kind 'screen' must be rejected as world.current_location_id")
    end,

    dangling_reference_rejected = function()
        local state_validator = require("core:module.runtime.state_validator")
        local ok = pcall(function()
            state_validator.validate_state_tree(make_tree("sample:location.nonexistent"))
        end)
        assert(not ok, "a well-formed but non-existent location reference must be rejected as dangling")
    end,

    current_location_readable_without_mutation = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        local player
        mutation_window.execute_in_window(function()
            player = game.instances.actors.player()
            if not player then
                player = game.instances.actors.create("sample:actor.character.hero", {
                    current_location_id = "sample:location.hub",
                })
                game.state.meta.player_actor_id = player.instance_id
            else
                player.current_location_id = "sample:location.hub"
            end
        end)
        local w = game.instances.world()
        assert(w.current_location_id == "sample:location.hub", "current_location_id must be readable through the World wrapper")

        -- Direct write to world.current_location_id must be rejected (read-only accessor)
        local write_err = false
        mutation_window.execute_in_window(function()
            local ok, err = pcall(function()
                w.current_location_id = "sample:location.east"
            end)
            if not ok then
                write_err = true
                assert(string.find(tostring(err), "WorldLocationReadOnly"), "Expected WorldLocationReadOnly error")
            end
        end)
        assert(write_err, "Direct write to world.current_location_id must be rejected")
    end,

    textsystem_actor_isolated_from_rh_economy = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        local player
        mutation_window.execute_in_window(function()
            player = game.instances.actors.player()
            if not player then
                player = game.instances.actors.create("sample:actor.character.hero", {
                    current_location_id = "sample:location.hub",
                })
                game.state.meta.player_actor_id = player.instance_id
            end
        end)
        assert(player:is_player() == true)
        -- In TextSystem tier (without rh), RH-specific methods must not exist (INV-016)
        assert(player.get_gold == nil, "get_gold must not exist in TextSystem tier")
        assert(player.spend_gold == nil, "spend_gold must not exist in TextSystem tier")
        assert(player.get_stamina == nil, "get_stamina must not exist in TextSystem tier")
        assert(player.spend_stamina == nil, "spend_stamina must not exist in TextSystem tier")
        assert(player.add_item == nil, "add_item must not exist in TextSystem tier")
    end,

    textsystem_actor_receiver_contract = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        local player
        local npc
        mutation_window.execute_in_window(function()
            player = game.instances.actors.player()
            if not player then
                player = game.instances.actors.create("sample:actor.character.hero", {
                    current_location_id = "sample:location.hub",
                })
                game.state.meta.player_actor_id = player.instance_id
            end
            npc = game.instances.actors.wrap({
                instance_id = "actor@1:999",
                definition_id = "sample:actor.character.hero",
                discriminator = "npc",
            })
        end)

        -- 1. Evaluates receiver correctly
        assert(player:is_player() == true, "player:is_player() must be true")
        assert(player:is_npc() == false, "player:is_npc() must be false")
        assert(npc:is_player() == false, "npc:is_player() must be false")
        assert(npc:is_npc() == true, "npc:is_npc() must be true")

        -- 2. Ignores extra arguments and evaluates receiver only
        assert(player:is_player(npc) == true, "player:is_player(other) must evaluate receiver, not argument")
        assert(npc:is_player(player) == false, "npc:is_player(other) must evaluate receiver, not argument")

        -- 3. Calling without receiver must throw MissingReceiver (EEH-06)
        local is_player_fn = game.entity_extensions.get_method("Actor", "is_player")
        local ok1, err1 = pcall(is_player_fn)
        assert(not ok1, "calling is_player without receiver must throw")
        assert(string.find(tostring(err1), "MissingReceiver") ~= nil,
            "Error must contain MissingReceiver, got: " .. tostring(err1))

        local ok2, err2 = pcall(function()
            return player.is_player() -- dot call passes no self
        end)
        assert(not ok2, "calling player.is_player() without self must throw MissingReceiver")
        assert(string.find(tostring(err2), "MissingReceiver") ~= nil)

        local is_npc_fn = game.entity_extensions.get_method("Actor", "is_npc")
        local ok3, err3 = pcall(is_npc_fn)
        assert(not ok3, "calling is_npc without receiver must throw MissingReceiver")
        assert(string.find(tostring(err3), "MissingReceiver") ~= nil)
    end,
}
