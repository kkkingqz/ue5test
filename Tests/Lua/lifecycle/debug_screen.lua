-- RH-10 / CHR-06: Verify debug/start screen creation and command handlers
local handler_registry = require("core:module.runtime.handler_registry")

return {
    debug_screen_handles_start_command = function()
        local handler = game.commands.handlers.get("core:command.debug.start")
        assert(type(handler) == "function", "debug.start handler must be registered")
        local ok = handler({
            command_id = "core:command.debug.start",
            args = {},
        })
        assert(ok == true, "debug.start command must succeed and publish screen")
    end,

    debug_screen_handles_interactions = function()
        local cb_handler = game.commands.handlers.get("core:command.test.checkbox_changed")
        assert(type(cb_handler) == "function", "checkbox handler must be registered")
        local ok_cb = cb_handler({
            command_id = "core:command.test.checkbox_changed",
            args = { is_checked = true },
        })
        assert(ok_cb == true, "checkbox command must succeed")

        local name_handler = game.commands.handlers.get("core:command.test.name_changed")
        assert(type(name_handler) == "function", "name handler must be registered")
        local ok_name = name_handler({
            command_id = "core:command.test.name_changed",
            args = { value = "Tester" },
        })
        assert(ok_name == true, "name command must succeed")

        local dd_handler = game.commands.handlers.get("core:command.test.dropdown_selected")
        assert(type(dd_handler) == "function", "dropdown handler must be registered")
        local ok_dd = dd_handler({
            command_id = "core:command.test.dropdown_selected",
            args = { selected_key = "warrior" },
        })
        assert(ok_dd == true, "dropdown command must succeed")
    end,
}
