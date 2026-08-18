-- Dynamic Location Screen & Menu Builder for rh package (DLA-22)
-- Builds location screen with rich text description and button_list combining location actions and travel links.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("rh")
M.id = "rh:module.presentation.location_screen"

local LOCATION_CONFIG = {
    ["rh:location.city.market"] = {
        description_text_id = "rh:text.screen.market.description",
        actions = {
            {
                key = "buy_sword",
                text_id = "rh:text.action.buy_sword",
                command_name = "buy",
                args = { item = "rh:item.weapon.iron_sword" },
            },
            {
                key = "buy_armor",
                text_id = "rh:text.action.buy_armor",
                command_name = "buy",
                args = { item = "rh:item.armor.leather_armor" },
            },
        },
    },
    ["rh:location.city.tavern"] = {
        description_text_id = "rh:text.screen.tavern.description",
        actions = {
            {
                key = "wait_day",
                text_id = "rh:text.action.wait_day",
                command_name = "time.wait_day",
            },
            {
                key = "do_work",
                text_id = "rh:text.action.do_work",
                command_name = "work.do_work",
            },
        },
    },
    ["rh:location.city.gate"] = {
        description_text_id = "rh:text.screen.gate.description",
        actions = {},
    },
}

function M.build_screen_request(location_id)
    if not location_id then
        return nil
    end

    local loc = M.location(location_id)
    if not loc then
        return nil
    end

    local config = LOCATION_CONFIG[location_id] or {
        description_text_id = loc.title_text_id,
        actions = {},
    }

    local screen_id = (loc.screen_ids and loc.screen_ids[1]) or "rh:screen.location.market"
    local buttons = {}

    -- 1. Location Actions
    for _, act in ipairs(config.actions or {}) do
        table.insert(buttons, M.button(
            M.text(act.text_id),
            M.action(act.command_name, act.args or {}),
            act.key
        ))
    end

    -- 2. Travel Transitions to Connected Neighbors
    local connected_ids = loc.connected_location_ids or {}
    for _, conn_id in ipairs(connected_ids) do
        local conn_loc = M.location(conn_id)
        local path_str = conn_id:match("^[^:]+:[^.]+%.(.+)$") or conn_id
        path_str = path_str:gsub("%.", "_")
        table.insert(buttons, M.button(
            M.text(conn_loc.title_text_id),
            M.action("core:command.location.travel", { target_location_id = conn_id }),
            "travel_" .. path_str
        ))
    end

    return M.show_screen({
        template = screen_id,
        description = M.text(config.description_text_id),
        buttons = buttons,
    })
end

function M.build_and_publish_screen()
    local current_loc = M.world.current_location_id
    if not current_loc then
        return nil
    end

    return M.build_screen_request(current_loc)
end

function M.register(_ctx)
    if game and game.presentation and game.presentation.register_source then
        game.presentation.register_source(M.build_and_publish_screen)
    end
end

return M
