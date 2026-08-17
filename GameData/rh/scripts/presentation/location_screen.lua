-- Dynamic Location Screen & Menu Builder for rh package (TGS-08, TGS-09)
-- Builds location screen with rich text description and button_list combining location actions and travel links.

local screens = require("core:module.presentation.screen_requests")
local text = require("core:module.resources.text")

local M = {
    id = "rh:module.presentation.location_screen",
}

local LOCATION_CONFIG = {
    ["rh:location.city.market"] = {
        description_text_id = "rh:text.screen.market.description",
        actions = {
            {
                key = "buy_sword",
                text_id = "rh:text.action.buy_sword",
                command_id = "rh:command.shop.buy_sword",
            },
            {
                key = "buy_armor",
                text_id = "rh:text.action.buy_armor",
                command_id = "rh:command.shop.buy_armor",
            },
        },
    },
    ["rh:location.city.tavern"] = {
        description_text_id = "rh:text.screen.tavern.description",
        actions = {
            {
                key = "wait_day",
                text_id = "rh:text.action.wait_day",
                command_id = "rh:command.time.wait_day",
            },
            {
                key = "do_work",
                text_id = "rh:text.action.do_work",
                command_id = "rh:command.work.do_work",
            },
        },
    },
    ["rh:location.city.gate"] = {
        description_text_id = "rh:text.screen.gate.description",
        actions = {},
    },
}

function M.build_screen_request(location_id)
    if not location_id or not game or not game.repository then
        return nil
    end

    local loc_def = game.repository.get(location_id)
    if not loc_def or not loc_def.data then
        return nil
    end

    local config = LOCATION_CONFIG[location_id] or {
        description_text_id = loc_def.data.title_text_id,
        actions = {},
    }

    local screen_id = loc_def.data.screen_ids and loc_def.data.screen_ids[1] or "rh:screen.location.market"
    local button_items = {}

    -- 1. Location Actions
    if config.actions then
        for _, act in ipairs(config.actions) do
            table.insert(button_items, {
                key = act.key,
                text = text.spec(act.text_id, nil, "button"),
                binding = {
                    command_id = act.command_id,
                    args = {},
                },
            })
        end
    end

    -- 2. Travel Transitions to Connected Neighbors
    local connected_ids = loc_def.data.connected_location_ids
    if type(connected_ids) == "table" then
        for _, conn_id in ipairs(connected_ids) do
            local conn_def = game.repository.get(conn_id)
            if conn_def and conn_def.data then
                local _, _, path_str = conn_id:match("^([^:]+):([^.]+)%.(.+)$")
                path_str = (path_str or conn_id):gsub("%.", "_")
                table.insert(button_items, {
                    key = "travel_" .. path_str,
                    text = text.spec(conn_def.data.title_text_id, nil, "button"),
                    binding = {
                        command_id = "core:command.location.travel",
                        args = { target_location_id = conn_id },
                    },
                })
            end
        end
    end

    return screens.create(screen_id, {
        description = {
            schema_id = "core:schema.ui_field.rich_text.v3",
            value = {
                text = text.spec(config.description_text_id, nil, "default"),
                spans = {},
            },
        },
        buttons = {
            schema_id = "core:schema.ui_field.button_list.v2",
            value = {
                items = button_items,
            },
        },
    })
end

function M.build_and_publish_screen()
    local current_loc = game.instances and game.instances.world and game.instances.world().current_location_id
    if not current_loc then
        return nil
    end

    local req = M.build_screen_request(current_loc)
    if req then
        screens.publish(req)
    end
    return req
end

function M.register_handlers(_ctx)
    if game and game.events and game.events.subscribers and game.events.subscribers.register then
        game.events.subscribers.register(
            "rh:subscriber.location_screen_updater",
            "core:event.location.enter",
            function(_env)
                M.build_and_publish_screen()
            end
        )
    end
end

M.LOCATION_CONFIG = LOCATION_CONFIG

return M
