local screens = require("core:module.presentation.screen_requests")
local text = require("core:module.resources.text")

local M = {
    id = "sample:module.debug.start",
}
local checkbox_checked = false
local selected_class = nil
local player_name = ""

local function create_screen()
    local item_id = "sample:item.synthetic.placeholder"
    if game.repository then
        local items = game.repository.list("item")
        if items and #items > 0 then
            item_id = items[1]
        end
    end

    return screens.create("core:screen.test", {
        description = {
            schema_id = "core:schema.ui_field.rich_text.v3",
            value = {
                text = text.spec("core:text.screen.test.description", { player_name = "Игрок" }, "default"),
                spans = {
                    {
                        key = "integration",
                        span_id = "integration",
                        hover = {
                            title = text.spec("core:text.screen.test.hover_title", nil, "tooltip_title"),
                            description = text.spec("core:text.screen.test.hover_description"),
                        },
                        binding = {
                            command_id = "sample:command.test.inspect",
                            args = { target = item_id },
                        },
                    },
                },
            },
        },
        buttons = {
            schema_id = "core:schema.ui_field.button_list.v2",
            value = {
                items = {
                    {
                        key = "inspect",
                        text = text.spec("core:text.screen.test.inspect", nil, "button"),
                        binding = {
                            command_id = "sample:command.test.inspect",
                            args = { target = item_id },
                        },
                    },
                    {
                        key = "close",
                        text = text.spec("core:text.screen.test.close", nil, "button"),
                        binding = { command_id = "sample:command.test.close", args = {} },
                    },
                },
            },
        },
        checkbox = {
            schema_id = "core:schema.ui_field.checkbox.v1",
            value = {
                text = text.spec("core:text.screen.test.checkbox", nil, "default"),
                is_checked = checkbox_checked,
                binding = {
                    command_id = "sample:command.test.checkbox_changed",
                    args = {},
                },
            },
        },
        player_name = {
            schema_id = "core:schema.ui_field.input_field.v1",
            value = {
                text = text.spec("core:text.screen.test.name_label", nil, "default"),
                placeholder_text = text.spec("core:text.screen.test.name_placeholder", nil, "default"),
                value = player_name ~= "" and player_name or item_id,
                binding = {
                    command_id = "sample:command.test.name_changed",
                    args = {},
                },
            },
        },
        class_select = {
            schema_id = "core:schema.ui_field.dropdown_select.v1",
            value = {
                placeholder = text.spec("core:text.screen.test.dropdown_placeholder", nil, "default"),
                selected_key = selected_class,
                items = {
                    {
                        key = "warrior",
                        text = text.spec("core:text.screen.test.class_warrior", nil, "default"),
                    },
                    {
                        key = "mage",
                        text = text.spec("core:text.screen.test.class_mage", nil, "default"),
                    },
                    {
                        key = "rogue",
                        text = text.spec("core:text.screen.test.class_rogue", nil, "default"),
                    },
                },
                binding = {
                    command_id = "sample:command.test.dropdown_selected",
                    args = {},
                },
            },
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
        checkbox_checked = false
        selected_class = nil
        player_name = ""
        screens.publish(create_screen())
        return true
    end)

    game.commands.handlers.register("sample:command.test.checkbox_changed", function(request)
        assert(type(request.args.is_checked) == "boolean", "checkbox command requires is_checked")
        checkbox_checked = request.args.is_checked
        screens.publish(create_screen())
        return true
    end)

    game.commands.handlers.register("sample:command.test.dropdown_selected", function(request)
        assert(type(request.args.selected_key) == "string", "dropdown command requires selected_key")
        selected_class = request.args.selected_key
        screens.publish(create_screen())
        return true
    end)

    game.commands.handlers.register("sample:command.test.name_changed", function(request)
        assert(type(request.args.value) == "string", "name command requires value")
        player_name = request.args.value
        screens.publish(create_screen())
        return true
    end)
end

function M.start(_ctx)
    checkbox_checked = false
    selected_class = nil
    player_name = ""
    screens.publish(create_screen())
end

return M
