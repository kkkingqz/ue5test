-- TextSystem Location Screen Presenter (TSL-12)
-- Declaratively builds location screen from location definition, screen definition,
-- and semantic actions, and registers as the presentation source.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("textsystem")
M.id = "textsystem:module.presentation.location_presenter"

function M.build_screen_request(location_id)
    if not location_id then
        return nil
    end

    local loc = M.location(location_id)
    if not loc then
        return nil
    end

    -- Локация без экрана — ошибка контента, а не повод подставить чужой:
    -- запасной идентификатор здесь означал бы данные игры внутри textsystem.
    local screen_id = loc.screen_ids and loc.screen_ids[1]
    if not screen_id then
        return nil
    end
    local screen_def = nil
    if game and game.repository and game.repository.get then
        screen_def = game.repository.get(screen_id)
    end

    local screen_data = (screen_def and screen_def.data) or {}
    local description_text_id = screen_data.description_text_id or loc.title_text_id

    local buttons = {}

    -- 1. Declarative Location Actions from screen definition
    for _, act in ipairs(screen_data.actions or {}) do
        table.insert(buttons, M.button(
            M.text(act.text_id),
            M.action(act.action_id, act.args or {}),
            act.key
        ))
    end

    -- 2. Travel Transitions to Connected Neighbors
    if screen_data.include_connected_locations ~= false then
        local connected_ids = loc.connected_location_ids or {}
        for _, conn_id in ipairs(connected_ids) do
            local conn_loc = M.location(conn_id)
            local path_str = conn_id:match("^[^:]+:[^.]+%.(.+)$") or conn_id
            path_str = path_str:gsub("%.", "_")
            local title_id = (conn_loc and conn_loc.title_text_id) or ("textsystem:text.location." .. path_str)
            table.insert(buttons, M.button(
                M.text(title_id),
                M.action("textsystem:action.location.travel", conn_id),
                "travel_" .. path_str
            ))
        end
    end

    return M.show_screen({
        template = screen_id,
        description = M.text(description_text_id),
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
