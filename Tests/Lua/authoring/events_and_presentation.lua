-- DLA-17, DLA-18, DLA-19: Events, Semantic Actions, Buttons, and Screens Specification (ADR-0027)
-- Verifies:
--   1. emit() unrolls wrappers into canonical tagged refs, on() rehydrates them
--   2. Event subscribers receive fresh wrappers reading current committed state
--   3. action() accepts command descriptors/names, rejects closures (ActionClosureDisallowed)
--   4. button() and show_screen() reject raw strings with RawStringDisallowed
--   5. text() creates canonical TextSpec, show_screen() publishes screen request

local mutation_window = require("core:module.runtime.mutation_window")
local event_bus = require("core:module.runtime.event_bus")
local authoring_context = require("core:module.authoring.context")
local properties = require("core:module.authoring.properties")
local actor_registry = require("core:module.runtime.actor_registry")
local screen_requests = require("core:module.presentation.screen_requests")
local subscriber_registry = require("core:module.runtime.subscriber_registry")

local function run_with_mock_environment(fn)
    local prev_game = _G.game
    _G.game = {
        state = {
            meta = {
                next_instance_id = 10,
                player_actor_id = nil,
            },
            actors = {},
            item_instances = {},
            world = {},
            quests = {},
            mods = {},
            definitions = {},
        },
        repository = {
            get = function(id)
                if id == "rh:actor.character.hero" then
                    return {
                        id = "rh:actor.character.hero",
                        discriminator = "character",
                        data = { name = "Hero" },
                    }
                elseif id == "rh:item.weapon.iron_sword" then
                    return {
                        id = "rh:item.weapon.iron_sword",
                        discriminator = "item",
                        data = { name = "Iron Sword", price = 10 },
                    }
                elseif id == "rh:screen.location.market" then
                    return {
                        id = "rh:screen.location.market",
                        discriminator = "screen",
                        data = { title_text_id = "rh:text.location.market.title" },
                    }
                end
                return nil
            end,
            exists = function(id)
                return id == "rh:actor.character.hero"
                    or id == "rh:item.weapon.iron_sword"
                    or id == "rh:screen.location.market"
            end,
        },
    }

    local registry = actor_registry.create_registry()
    registry.register_type("character", function(base) return {} end)
    local sub_reg = subscriber_registry.create_registry()
    local service_reg = require("core:module.runtime.service_registry")
    service_reg.register()

    local queue = {}
    local published_events = {}

    _G.game.instances = {
        actors = registry,
    }
    _G.game.commands = {
        handlers = {
            register = function(cmd_id, handler) end,
        },
    }
    _G.game.events = {
        subscribers = sub_reg,
        enqueue = function(item)
            table.insert(queue, item)
        end,
        publish = function(item)
            table.insert(published_events, item)
            local subs = sub_reg.get_subscribers_for_event(item.event_id)
            for _, sub in ipairs(subs) do
                sub.handler({
                    event_id = item.event_id,
                    payload = item.payload,
                })
            end
        end,
        get_published_events = function()
            return published_events
        end,
    }

    local ok, err = pcall(function()
        properties.with_isolated_state(function()
            fn(registry, queue, published_events)
        end)
    end)
    _G.game = prev_game
    if not ok then
        error(err)
    end
end

return {
    emit_and_on_roundtrip_with_wrapper_rehydration = function()
        run_with_mock_environment(function(registry, queue, published_events)
            local rh = authoring_context.gameplay("rh")

            local hero = mutation_window.execute_in_window(function()
                local h = registry.create("rh:actor.character.hero", {
                    stamina = 25,
                    gold = 100,
                })
                game.state.meta.player_actor_id = h.instance_id
                return h
            end)

            local sword = rh.def.item("weapon.iron_sword")

            local received_payload = nil
            local received_env = nil

            rh.on("crafting.completed", function(payload, env)
                received_payload = payload
                received_env = env
            end)

            -- Register module lifecycle to bind subscribers
            rh.register()

            -- Emit event with wrappers in payload
            mutation_window.execute_in_window(function()
                rh.emit("crafting.completed", {
                    crafter = hero,
                    result_item = sword,
                    quality = 5,
                })
            end)

            -- Verify queued event payload has tagged refs (no metatables)
            assert(#queue == 1, "emit must enqueue event in mutation window")
            assert(queue[1].event_id == "rh:event.crafting.completed", "Event ID must be canonicalized")
            assert(queue[1].payload.quality == 5)
            assert(queue[1].payload.crafter.__gv2_ref == "instance")
            assert(queue[1].payload.crafter.id == hero.instance_id)
            assert(queue[1].payload.result_item.__gv2_ref == "definition")
            assert(queue[1].payload.result_item.id == "rh:item.weapon.iron_sword")

            -- Simulate event bus delivering committed event
            local item = queue[1]
            game.events.publish(item)

            assert(received_payload ~= nil, "Subscriber must be invoked")
            assert(received_payload.quality == 5)

            -- Verify hero wrapper rehydrated and reads current state
            assert(type(received_payload.crafter) == "table", "crafter must be a table wrapper")
            assert(received_payload.crafter.instance_id == hero.instance_id, "Instance ID must match")
            assert(received_payload.crafter.stamina == 25, "Must read current state from rehydrated actor")

            -- Verify definition wrapper rehydrated
            assert(type(received_payload.result_item) == "table", "result_item must be a definition wrapper")
            assert(received_payload.result_item.id == "rh:item.weapon.iron_sword", "Definition ID must match")
            assert(received_payload.result_item.price == 10, "Must read definition properties")
        end)
    end,

    action_accepts_command_descriptor_and_rejects_closure = function()
        run_with_mock_environment(function(registry)
            local rh = authoring_context.gameplay("rh")
            local sword = rh.def.item("weapon.iron_sword")

            -- 1. action() with command name
            local act1 = rh.action("shop.buy_sword", sword, 1)
            assert(act1.command_id == "rh:command.shop.buy_sword", "command_id must be canonicalized")
            assert(act1.args[1].__gv2_ref == "definition")
            assert(act1.args[1].id == "rh:item.weapon.iron_sword")
            assert(act1.args[2] == 1)

            -- 2. action() with full Stable ID
            local act2 = rh.action("rh:command.travel", { target_location_id = "rh:location.city.tavern" })
            assert(act2.command_id == "rh:command.travel")
            assert(act2.args.target_location_id == "rh:location.city.tavern")

            -- 3. action() with closure must be rejected
            local closure_err = false
            local ok, err = pcall(function()
                rh.action(function() return "do_something" end)
            end)
            if not ok then
                closure_err = true
                assert(string.find(tostring(err), "ActionClosureDisallowed"), "Expected ActionClosureDisallowed error, got: " .. tostring(err))
            end
            assert(closure_err, "Arbitrary closure passed to action() must be rejected")
        end)
    end,

    button_and_show_screen_reject_raw_strings = function()
        run_with_mock_environment(function(registry)
            local rh = authoring_context.gameplay("rh")
            local sword = rh.def.item("weapon.iron_sword")

            -- 1. Raw string in button() text must be rejected
            local raw_btn_err = false
            local ok1, err1 = pcall(function()
                rh.button("Buy Sword", rh.action("shop.buy_sword", sword))
            end)
            if not ok1 then
                raw_btn_err = true
                assert(string.find(tostring(err1), "RawStringDisallowed"), "Expected RawStringDisallowed, got: " .. tostring(err1))
            end
            assert(raw_btn_err, "Raw string in button() must be rejected")

            -- 2. Valid button (key derived from command + arg)
            local btn = rh.button(rh.text("action.buy_sword"), rh.action("shop.buy_sword", sword))
            assert(btn.key == "shop_buy_sword_weapon_iron_sword", "Key should be derived from command_id and entity arg, got: " .. tostring(btn.key))
            assert(btn.text.text_id == "rh:text.action.buy_sword")
            assert(btn.binding.command_id == "rh:command.shop.buy_sword")

            -- 3. Raw string in show_screen() description must be rejected
            local raw_desc_err = false
            local ok2, err2 = pcall(function()
                rh.show_screen({
                    template = "location.market",
                    description = "Market square description raw string",
                    buttons = { btn },
                })
            end)
            if not ok2 then
                raw_desc_err = true
                assert(string.find(tostring(err2), "RawStringDisallowed"), "Expected RawStringDisallowed, got: " .. tostring(err2))
            end
            assert(raw_desc_err, "Raw string in show_screen description must be rejected")

            -- 4. Raw string in show_screen() buttons list must be rejected
            local raw_btn_list_err = false
            local ok3, err3 = pcall(function()
                rh.show_screen({
                    template = "location.market",
                    description = rh.text("screen.market.description"),
                    buttons = { "Raw button string" },
                })
            end)
            if not ok3 then
                raw_btn_list_err = true
                assert(string.find(tostring(err3), "RawStringDisallowed"), "Expected RawStringDisallowed, got: " .. tostring(err3))
            end
            assert(raw_btn_list_err, "Raw string button in show_screen buttons list must be rejected")

            -- 5. Valid show_screen() publishes screen request
            local published = rh.show_screen({
                template = "location.market",
                description = rh.text("screen.market.description"),
                buttons = { btn },
            })
            assert(published.screen_id == "rh:screen.location.market")
            assert(published.fields.description.value.text.text_id == "rh:text.screen.market.description")
            assert(#published.fields.buttons.value.items == 1)

            local pending = screen_requests.take_pending()
            assert(pending ~= nil, "Screen request must be published to screen_requests")
            assert(pending.screen_id == "rh:screen.location.market")
        end)
    end,

    button_key_derivation_and_rejection_of_text = function()
        run_with_mock_environment(function(registry)
            local rh = authoring_context.gameplay("rh")
            local sword = rh.def.item("weapon.iron_sword")

            -- 1. Command without args
            local btn_no_args = rh.button(rh.text("action.save"), rh.action("game.save"))
            assert(btn_no_args.key == "game_save", "Key should be derived from command: " .. tostring(btn_no_args.key))

            -- 2. Command with args
            local btn_args = rh.button(rh.text("action.buy"), rh.action("shop.buy", sword))
            assert(btn_args.key == "shop_buy_weapon_iron_sword", "Key should incorporate arg: " .. tostring(btn_args.key))

            -- 3. Explicit key precedence
            local btn_custom = rh.button(rh.text("action.buy"), rh.action("shop.buy", sword), "my_custom_key")
            assert(btn_custom.key == "my_custom_key", "Explicit key must take precedence")

            -- 4. Rejection of TextSpec table as key
            local ok_t1, err_t1 = pcall(function()
                rh.button(rh.text("action.buy"), rh.action("shop.buy"), rh.text("action.buy"))
            end)
            assert(not ok_t1 and string.find(tostring(err_t1), "TextDisallowedAsKey"), "TextSpec as key must be rejected: " .. tostring(err_t1))

            -- 5. Rejection of text Stable ID string as key
            local ok_t2, err_t2 = pcall(function()
                rh.button(rh.text("action.buy"), rh.action("shop.buy"), "rh:text.action.buy")
            end)
            assert(not ok_t2 and string.find(tostring(err_t2), "TextDisallowedAsKey"), "text Stable ID as key must be rejected: " .. tostring(err_t2))

            -- 6. Rejection of invalid key grammar
            local ok_bad_key, err_bad_key = pcall(function()
                rh.button(rh.text("action.buy"), rh.action("shop.buy"), "Invalid Key!")
            end)
            assert(not ok_bad_key and string.find(tostring(err_bad_key), "InvalidButtonKey"), "Invalid key grammar must be rejected: " .. tostring(err_bad_key))

            -- 7. show_screen rejects duplicate button keys
            local ok_dup, err_dup = pcall(function()
                rh.show_screen({
                    template = "location.market",
                    description = rh.text("screen.market.description"),
                    buttons = { btn_custom, btn_custom },
                })
            end)
            assert(not ok_dup and string.find(tostring(err_dup), "UiElementKeyDuplicate"), "Duplicate button key must be rejected by show_screen: " .. tostring(err_dup))
        end)
    end,

    text_spec_canonical_ids_and_error_convention = function()
        run_with_mock_environment(function(registry)
            local rh = authoring_context.gameplay("rh")

            -- 1. text() generates canonical TextSpec
            local t1 = rh.text("screen.market.description")
            assert(t1.text_id == "rh:text.screen.market.description")
            assert(t1.style == "default")

            local t2 = rh.text("core:text.button.ok", { count = 3 }, "bold")
            assert(t2.text_id == "core:text.button.ok")
            assert(t2.args.count == 3)
            assert(t2.style == "bold")

            -- 2. text() rejects empty or non-string
            local bad_text_err = false
            local ok, err = pcall(function()
                rh.text("")
            end)
            if not ok then
                bad_text_err = true
                assert(string.find(tostring(err), "InvalidTextKey"), "Expected InvalidTextKey error")
            end
            assert(bad_text_err, "Empty text key must be rejected")
        end)
    end,
}
