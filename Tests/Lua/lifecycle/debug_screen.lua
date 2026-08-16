-- RH-10: Verify debug/start screen creation is decoupled from specific item entities
local start_mod = require("core:module.debug.start")

return {
    debug_screen_handles_start_command = function()
        local ok = start_mod.handle_command({
            command_id = "core:command.debug.start",
            args = {},
        })
        assert(ok == true, "debug.start command must succeed and publish screen")
    end,

    debug_screen_handles_interactions = function()
        local ok_cb = start_mod.handle_command({
            command_id = "core:command.test.checkbox_changed",
            args = { is_checked = true },
        })
        assert(ok_cb == true, "checkbox command must succeed")

        local ok_name = start_mod.handle_command({
            command_id = "core:command.test.name_changed",
            args = { value = "Tester" },
        })
        assert(ok_name == true, "name command must succeed")

        local ok_dd = start_mod.handle_command({
            command_id = "core:command.test.dropdown_selected",
            args = { selected_key = "warrior" },
        })
        assert(ok_dd == true, "dropdown command must succeed")
    end,
}
