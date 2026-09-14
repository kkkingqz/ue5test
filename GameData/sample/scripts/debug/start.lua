local screens = require("core:module.presentation.screen_requests")
local text = require("core:module.resources.text")

local M = {
    id = "sample:module.debug.start",
}

local function get_sample_state()
    if not (game and game.state and game.state.sample_debug) then
        return {}
    end
    return game.state.sample_debug
end

local function create_screen()
    local s = get_sample_state()
    local checkbox_checked = s.checkbox_checked or false
    local selected_class = s.selected_class
    local player_name = s.player_name or ""

    local item_id = "sample:item.synthetic.placeholder"
    if game.repository then
        local items = game.repository.list("item")
        if items and #items > 0 then
            item_id = items[1]
        end
    end

    -- DUC-02: "greeting" is a plain UGV2TextWidgetBase base element (GreetingText),
    -- addressable as a Screen Field with no dedicated C++ class -- the other five
    -- children of WBP_Testscreen have no HostIdentity configured and remain static,
    -- exactly as before.
    return screens.create("core:screen.test", {
        greeting = {
            schema_id = "core:schema.ui_field.text.v1",
            value = { text = text.spec("sample:text.screen.test.greeting") },
        },
    })
end

function M.register(_ctx)
    if not game or not game.commands or not game.commands.handlers then
        return
    end

    game.commands.handlers.register("sample:command.test.force_error", function(_req)
        error("forced test runtime error")
    end)

    game.commands.handlers.register("sample:command.debug.start", function(_req)
        if game and game.state then
            game.state.sample_debug = {
                checkbox_checked = false,
                selected_class = nil,
                player_name = "",
            }
        end
        screens.publish(create_screen())
        return true
    end)

    game.commands.handlers.register("sample:command.test.checkbox_changed", function(request)
        assert(type(request.args.is_checked) == "boolean", "checkbox command requires is_checked")
        if game and game.state then
            if not game.state.sample_debug then
                game.state.sample_debug = {}
            end
            game.state.sample_debug.checkbox_checked = request.args.is_checked
        end
        screens.publish(create_screen())
        return true
    end)

    game.commands.handlers.register("sample:command.test.dropdown_selected", function(request)
        assert(type(request.args.selected_key) == "string", "dropdown command requires selected_key")
        if game and game.state then
            if not game.state.sample_debug then
                game.state.sample_debug = {}
            end
            game.state.sample_debug.selected_class = request.args.selected_key
        end
        screens.publish(create_screen())
        return true
    end)

    game.commands.handlers.register("sample:command.test.name_changed", function(request)
        assert(type(request.args.value) == "string", "name command requires value")
        if game and game.state then
            if not game.state.sample_debug then
                game.state.sample_debug = {}
            end
            game.state.sample_debug.player_name = request.args.value
        end
        screens.publish(create_screen())
        return true
    end)
end

function M.start(_ctx)
    if game and game.instances and game.instances.actors and game.instances.actors.player and game.instances.actors.player() ~= nil then
        return
    end

    if game and game.runtime and game.runtime.seed_hex and #game.runtime.seed_hex > 0 and game.commands and game.commands.handlers and game.commands.handlers.get("sample:command.start_game") then
        game.runtime.dispatch_command({
            command_id = "sample:command.start_game",
            args = {},
            source = "session_start",
        })
    elseif game and game.runtime and game.runtime.dispatch_command then
        game.runtime.dispatch_command({
            command_id = "sample:command.debug.start",
            args = {},
            source = "session_start",
        })
    else
        screens.publish(create_screen())
    end
end

return M
