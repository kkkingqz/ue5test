-- GLS-15: Location Screen Transition Scenario Spec
-- Verifies end-to-end location transition:
--   1. Starts in Tavern with full player status, resources, and commands.
--   2. Dispatches travel to Market.
--   3. Verifies Screen instance identity (screen_id = textsystem:screen.location, instance_key = location).
--   4. Verifies TopBar, Scene, and Commands fields update correctly.
--   5. Verifies Command buttons send semantic action bindings.
--   6. Dispatches travel back to Tavern.

local mutation_window = require("core:module.runtime.mutation_window")
local presentation_authoring = require("core:module.authoring.presentation")
local location_presenter = require("textsystem:module.presentation.location_presenter")

local function find_button(buttons_field, key)
    if not buttons_field or not buttons_field.value or not buttons_field.value.items then
        return nil
    end
    for _, item in ipairs(buttons_field.value.items) do
        if item.key == key then
            return item
        end
    end
    return nil
end

local function ensure_game_state()
    local player = game.instances.actors.player()
    if not player then
        game.runtime.dispatch_command({
            command_id = "rh:command.start_game",
            args = {},
            sequence = 1000,
        })
        player = game.instances.actors.player()
    end
    player.current_location_id = "rh:location.city.tavern"
    player.stamina = 50
    player.gold = 100
    return player
end

return {
    location_transition_preserves_screen_identity = function()
        mutation_window.execute_in_window(function()
            local player = ensure_game_state()

            -- 1. Initial presentation in Tavern
            location_presenter.build_and_publish_screen()
            local state_tavern = presentation_authoring.get_active_document_state()
            assert(state_tavern ~= nil, "active document state must exist")
            assert(state_tavern.route ~= nil, "route must exist")
            assert(state_tavern.route.screen_id == "textsystem:screen.location", "must use LocationScreen template")
            assert(state_tavern.route.instance_key == "location", "instance_key must be 'location'")

            -- Verify Tavern fields
            local fields_tavern = state_tavern.route.fields
            assert(fields_tavern.top_bar ~= nil)
            assert(fields_tavern.top_bar.value.location.text_id == "rh:text.location.tavern.title")
            assert(fields_tavern.scene ~= nil)
            assert(fields_tavern.scene.value.context_text.text_id == "rh:text.screen.tavern.description")

            -- Verify Tavern command buttons
            local btn_wait = find_button(fields_tavern.commands, "wait_day")
            assert(btn_wait ~= nil, "wait_day command must exist in tavern")
            assert(btn_wait.binding.command_id == "rh:command.time.wait_day")

            local btn_to_market = find_button(fields_tavern.commands, "travel_city_market")
            assert(btn_to_market ~= nil, "travel_city_market must exist in tavern")
            assert(btn_to_market.binding.command_id == "rh:command.travel")
            assert(btn_to_market.binding.args.target == "rh:location.city.market")

            -- 2. Dispatch Travel to Market
            game.runtime.dispatch_command({
                command_id = "rh:command.travel",
                args = { "rh:location.city.market" },
                sequence = 1001,
            })

            -- 3. Verify Market presentation
            local state_market = presentation_authoring.get_active_document_state()
            assert(state_market ~= nil, "market document state must exist")
            assert(state_market.route ~= nil, "market route must exist")
            assert(state_market.route.screen_id == "textsystem:screen.location", "screen_id must stay textsystem:screen.location")
            assert(state_market.route.instance_key == "location", "instance_key must stay 'location'")

            -- Verify Market fields
            local fields_market = state_market.route.fields
            assert(fields_market.top_bar.value.location.text_id == "rh:text.location.market.title", "top_bar location must be market")
            assert(fields_market.scene.value.context_text.text_id == "rh:text.screen.market.description", "scene description must be market")

            -- Verify Market commands
            local btn_buy_sword = find_button(fields_market.commands, "buy_sword")
            assert(btn_buy_sword ~= nil, "buy_sword command must exist in market")
            assert(btn_buy_sword.binding.command_id == "rh:command.request_buy")
            assert(btn_buy_sword.binding.args.item == "rh:item.weapon.iron_sword")

            local btn_to_tavern = find_button(fields_market.commands, "travel_city_tavern")
            assert(btn_to_tavern ~= nil, "travel_city_tavern must exist in market")
            assert(btn_to_tavern.binding.command_id == "rh:command.travel")
            assert(btn_to_tavern.binding.args.target == "rh:location.city.tavern")

            -- 4. Dispatch Travel back to Tavern
            game.runtime.dispatch_command({
                command_id = "rh:command.travel",
                args = { "rh:location.city.tavern" },
                sequence = 1002,
            })

            -- 5. Verify returned Tavern presentation
            local state_tavern2 = presentation_authoring.get_active_document_state()
            assert(state_tavern2.route.screen_id == "textsystem:screen.location")
            assert(state_tavern2.route.instance_key == "location")
            assert(state_tavern2.route.fields.top_bar.value.location.text_id == "rh:text.location.tavern.title")
            assert(state_tavern2.route.fields.scene.value.context_text.text_id == "rh:text.screen.tavern.description")
        end)
    end,
}
