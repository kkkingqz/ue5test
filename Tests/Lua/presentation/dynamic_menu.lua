-- TGS-08 / TGS-09: Dynamic Menu & Location Screen Specification
-- Verifies dynamic generation of button_list combining location actions and travel links,
-- and automated publication of location screens on travel events.

local location_presenter = require("textsystem:module.presentation.location_presenter")
local screens = require("core:module.presentation.screen_requests")
local mutation_window = require("core:module.runtime.mutation_window")
local event_bus = require("core:module.runtime.event_bus")

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

local function ensure_player(initial_stamina, initial_gold)
    local player = game.instances.actors.player()
    if not player then
        local hero = game.instances.actors.create("rh:actor.character.hero", {
            stamina = initial_stamina or 50,
            gold = initial_gold or 50,
        })
        game.state.meta.player_actor_id = hero.instance_id
        return hero
    end
    player.stamina = initial_stamina or 50
    return player
end

return {
    -- TGS-09: an action rebuilds the screen, otherwise the player presses a button
    -- and sees nothing change. Travel is covered separately by the enter-event case.
    screen_republished_after_location_action = function()
        event_bus.clear_published_events()

        mutation_window.execute_in_window(function()
            local player = ensure_player(20, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.tavern"

            screens.take_pending()
            assert(screens.take_pending() == nil, "no screen must be pending before the action")

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.time.wait_day",
                args = {},
                sequence = 901,
            })
            assert(seq == 901)

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok ~= false, "wait_day must succeed")

            local published = screens.take_pending()
            assert(published ~= nil, "a successful action must republish the screen")
            assert(published.screen_id == "textsystem:screen.location",
                "republished screen must use the LocationScreen template, got: " .. tostring(published.screen_id))
            assert(published.instance_key == "location", "location route identity must remain stable")
            assert(find_button(published.fields.commands, "wait_day") ~= nil,
                "republished menu must still contain the location actions")

            assert(player.stamina == 30, "wait_day must add 10 stamina, got: " .. tostring(player.stamina))
        end)

        event_bus.clear_published_events()
    end,

    screen_not_republished_after_refused_action = function()
        event_bus.clear_published_events()

        mutation_window.execute_in_window(function()
            local player = ensure_player(4, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.tavern"

            screens.take_pending()

            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.work.do_work",
                args = {},
                sequence = 902,
            })
            assert(seq == 902)

            local result = game.runtime.last_command_result
            assert(result ~= nil and result.ok == false, "work at stamina 4 must be refused")

            assert(screens.take_pending() == nil, "a refused action must not republish the screen")
        end)

        event_bus.clear_published_events()
    end,

    market_screen_structure_and_buttons = function()
        local req = location_presenter.build_screen_request("rh:location.city.market")
        assert(req ~= nil, "screen request must be generated for market")
        assert(req.screen_id == "textsystem:screen.location", "screen_id must be the shared LocationScreen template")
        assert(req.instance_key == "location", "location route identity must be stable")

        local scene = req.fields.scene
        assert(scene ~= nil and scene.schema_id == "textsystem:schema.ui_field.location_scene.v1")
        assert(scene.value.context_text.text_id == "rh:text.screen.market.description")

        local buttons = req.fields.commands
        assert(buttons ~= nil and buttons.schema_id == "textsystem:schema.ui_field.location_commands.v1")
        assert(#buttons.value.items == 3, "market must have 3 buttons (2 actions + 1 travel), got: " .. tostring(#buttons.value.items))

        local btn_sword = find_button(buttons, "buy_sword")
        assert(btn_sword ~= nil, "buy_sword button must exist")
        assert(btn_sword.binding.command_id == "rh:command.request_buy")
        assert(btn_sword.binding.args.item == "rh:item.weapon.iron_sword")

        local btn_armor = find_button(buttons, "buy_armor")
        assert(btn_armor ~= nil, "buy_armor button must exist")
        assert(btn_armor.binding.command_id == "rh:command.request_buy")
        assert(btn_armor.binding.args.item == "rh:item.armor.leather_armor")

        local btn_travel = find_button(buttons, "travel_city_tavern")
        assert(btn_travel ~= nil, "travel to tavern button must exist")
        assert(btn_travel.binding.command_id == "rh:command.travel")
        assert(btn_travel.binding.args.target == "rh:location.city.tavern")
    end,

    tavern_screen_structure_and_buttons = function()
        local req = location_presenter.build_screen_request("rh:location.city.tavern")
        assert(req ~= nil, "screen request must be generated for tavern")
        assert(req.screen_id == "textsystem:screen.location", "screen_id must be the shared LocationScreen template")

        local scene = req.fields.scene
        assert(scene.value.context_text.text_id == "rh:text.screen.tavern.description")

        local buttons = req.fields.commands
        assert(#buttons.value.items == 4, "tavern must have 4 buttons (2 actions + 2 travel), got: " .. tostring(#buttons.value.items))

        local btn_wait = find_button(buttons, "wait_day")
        assert(btn_wait ~= nil, "wait_day button must exist")
        assert(btn_wait.binding.command_id == "rh:command.time.wait_day")

        local btn_work = find_button(buttons, "do_work")
        assert(btn_work ~= nil, "do_work button must exist")
        assert(btn_work.binding.command_id == "rh:command.work.do_work")

        local btn_market = find_button(buttons, "travel_city_market")
        assert(btn_market ~= nil, "travel to market button must exist")
        assert(btn_market.binding.args.target == "rh:location.city.market")

        local btn_gate = find_button(buttons, "travel_city_gate")
        assert(btn_gate ~= nil, "travel to gate button must exist")
        assert(btn_gate.binding.args.target == "rh:location.city.gate")
    end,

    gate_screen_structure_and_buttons = function()
        local req = location_presenter.build_screen_request("rh:location.city.gate")
        assert(req ~= nil, "screen request must be generated for gate")
        assert(req.screen_id == "textsystem:screen.location", "screen_id must be the shared LocationScreen template")

        local scene = req.fields.scene
        assert(scene.value.context_text.text_id == "rh:text.screen.gate.description")

        local buttons = req.fields.commands
        assert(#buttons.value.items == 1, "gate must have 1 button (travel to tavern), got: " .. tostring(#buttons.value.items))

        local btn_tavern = find_button(buttons, "travel_city_tavern")
        assert(btn_tavern ~= nil, "travel to tavern button must exist")
        assert(btn_tavern.binding.args.target == "rh:location.city.tavern")
    end,

    screen_updates_on_travel_event = function()
        mutation_window.execute_in_window(function()
            local player = ensure_player(50, 50)
            local world = game.instances.world()
            player.current_location_id = "rh:location.city.market"

            -- Publish market screen
            location_presenter.build_and_publish_screen()
            local screen_market = screens.take_pending()
            assert(screen_market ~= nil and screen_market.screen_id == "textsystem:screen.location")
            assert(screen_market.fields.top_bar.value.location.text_id == "rh:text.location.market.title")
            assert(screen_market.fields.scene.value.background_resource_id == "rh:resource.location.market")
            assert(#screen_market.fields.scene.value.character_resource_ids == 1
                and screen_market.fields.scene.value.character_resource_ids[1] == "textsystem:resource.ui.missing_character")
            assert(find_button(screen_market.fields.commands, "buy_sword") ~= nil)

            -- Dispatch travel to tavern
            local seq = game.runtime.dispatch_command({
                command_id = "rh:command.travel",
                args = { "rh:location.city.tavern" },
                sequence = 950,
            })
            assert(seq == 950)

            -- Event subscriber should have published tavern screen
            local screen_tavern = screens.take_pending()
            assert(screen_tavern ~= nil, "tavern screen must be published after travel event")
            assert(screen_tavern.screen_id == "textsystem:screen.location",
                "screen_id must remain the LocationScreen template, got: " .. tostring(screen_tavern.screen_id))
            assert(screen_tavern.instance_key == screen_market.instance_key,
                "travel must retain the location screen instance")
            assert(screen_tavern.fields.top_bar.value.location.text_id == "rh:text.location.tavern.title",
                "top bar must receive the destination location")
            assert(screen_tavern.fields.scene.value.background_resource_id == "rh:resource.location.tavern",
                "scene must receive the destination background")
            assert(screen_tavern.fields.scene.value.character_resource_ids[1] == "rh:resource.character.tavern_keeper",
                "scene must receive the tavern character")
            assert(find_button(screen_tavern.fields.commands, "wait_day") ~= nil,
                "destination commands must replace the market command set")
            assert(find_button(screen_tavern.fields.commands, "buy_sword") == nil,
                "market command must not survive the transition")
            assert(player.current_location_id == "rh:location.city.tavern",
                "semantic travel command must mutate canonical location state")
            assert(#screen_tavern.fields.commands.value.items == 4, "tavern menu must have 4 buttons")
        end)
    end,
}
