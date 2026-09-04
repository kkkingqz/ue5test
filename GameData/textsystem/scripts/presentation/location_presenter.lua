-- TextSystem Location Screen Presenter (TSL-12)
-- Declaratively builds location screen from location definition, screen definition,
-- and semantic actions, and registers as the presentation source.

local authoring = require("core:module.authoring.context")
local duc10_fixture = require("textsystem:module.presentation.duc10_fixture")
local dca09_fixture = require("textsystem:module.presentation.dca09_fixture")
local dca10_fixture = require("textsystem:module.presentation.dca10_fixture")

local M = authoring.gameplay("textsystem")
M.id = "textsystem:module.presentation.location_presenter"

-- This is the UE template identity, not a gameplay screen Definition ID.
-- `screen_ids` below continues to select the Definition that supplies values.
local LOCATION_SCREEN_TEMPLATE_ID = "textsystem:screen.location"
local LOCATION_SCREEN_INSTANCE_KEY = "location"

local function extension_with_field(definition, field)
    for _, block in pairs((definition and definition.extensions) or {}) do
        if type(block) == "table" and block[field] ~= nil then return block end
    end
    return {}
end

local function current_player()
    return game and game.instances and game.instances.actors and game.instances.actors.player
        and game.instances.actors.player() or nil
end

local function player_values()
    local player = current_player()
    local gold = player and player.get_gold and player:get_gold() or (player and player.gold) or 0
    local stamina = player and player.get_stamina and player:get_stamina() or (player and player.stamina) or 0
    local actor_def = player and game and game.repository and game.repository.get and game.repository.get(player.definition_id) or nil
    local actor_extension = extension_with_field(actor_def, "name_text_id")
    local items = {}
    for _, item in pairs((game and game.state and game.state.item_instances) or {}) do
        if player and item.owner_id == player.instance_id and item.instance_id then
            local item_def = game.repository and game.repository.get and game.repository.get(item.definition_id)
            if item_def and item_def.data and item_def.data.icon_resource_id then
                -- Identity is the item instance, never the icon or the array position:
                -- acquiring an item must not reassign the widgets of the other items.
                table.insert(items, { key = item.instance_id, resource_id = item_def.data.icon_resource_id })
            end
        end
    end
    table.sort(items, function(a, b) return a.key < b.key end)
    return player, gold, stamina, actor_extension, items
end

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
    local scene_data = extension_with_field(screen_def, "background_resource_id")
    if not scene_data.background_resource_id and not scene_data.characters then
        scene_data = extension_with_field(screen_def, "characters")
    end
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
                M.action("textsystem:action.location.travel", { target = conn_id }),
                "travel_" .. path_str
            ))
        end
    end

    local characters = {}
    if type(scene_data.characters) == "table" then
        for _, char_entry in ipairs(scene_data.characters) do
            if type(char_entry) == "table" then
                local res_id = char_entry.resource_id or "textsystem:resource.ui.missing_character"
                local key = char_entry.key
                if key and key ~= "" then
                    table.insert(characters, {
                        key = key,
                        resource_id = res_id,
                    })
                end
            end
        end
    end

    local player, gold, stamina, actor_extension, item_icons = player_values()
    local day = (game and game.state and game.state.meta and game.state.meta.day) or 1
    return M.show_screen({
        template = LOCATION_SCREEN_TEMPLATE_ID,
        instance_key = LOCATION_SCREEN_INSTANCE_KEY,
        fields = {
            top_bar = { schema_id = "textsystem:schema.ui_field.location_top_bar.v1", value = {
                day = M.text("textsystem:text.location.day", { day = day }),
                location = M.text(loc.title_text_id),
                primary_resource = M.text("textsystem:text.location.gold", { gold = gold }),
            } },
            player_status = { schema_id = "textsystem:schema.ui_field.location_player_status.v1", value = {
                portrait_resource_id = actor_extension.portrait_resource_id or "textsystem:resource.ui.missing_portrait",
                name = M.text(actor_extension.name_text_id or loc.title_text_id),
                meters = { { key = "stamina", percent = math.min(1, stamina / 100), label = M.text("textsystem:text.location.stamina", { stamina = stamina }) } },
                items = item_icons,
                effects = {},
            } },
            scene = { schema_id = "textsystem:schema.ui_field.location_scene.v1", value = {
                background_tile_resource_id = "core:resource.ui.old_paper_tile_256",
                background_resource_id = scene_data.background_resource_id or "textsystem:resource.ui.missing_background",
                characters = characters,
                context_text = M.text(description_text_id),
            } },
            commands = { schema_id = "textsystem:schema.ui_field.location_commands.v1", value = { items = buttons } },
        },
    })
end

function M.build_and_publish_screen()
    local duc10_document = duc10_fixture.build_requested_screen()
    if duc10_document ~= nil then
        return duc10_document
    end

    local dca09_document = dca09_fixture.build_requested_screen()
    if dca09_document ~= nil then
        return dca09_document
    end

    local dca10_document = dca10_fixture.build_requested_screen()
    if dca10_document ~= nil then
        return dca10_document
    end

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
