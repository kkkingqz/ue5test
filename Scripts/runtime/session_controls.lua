-- Session Controls (CFC-09, plan SaveAndGameplay, CanonicalStateAndSave.md)
-- Owns core command handlers for host-level session operations (e.g. save).
-- Handlers validate input arguments and enqueue value-only control requests
-- to game.bridge. Handlers do NOT execute storage operations directly.

local stable_id = require("core:module.runtime.stable_id")

local M = {
    id = "core:module.runtime.session_controls",
}

function M.handle_save(request)
    local args = (type(request) == "table" and type(request.args) == "table") and request.args or {}
    local slot_id = args.slot_id

    if type(slot_id) ~= "string" or slot_id == "" then
        return {
            ok = false,
            error = {
                code = "core:error.save.invalid_slot",
                params = { slot_id = tostring(slot_id) },
            },
        }
    end

    if not game or not game.bridge or type(game.bridge.request_save) ~= "function" then
        return {
            ok = false,
            error = {
                code = "core:error.save.bridge_unavailable",
                params = {},
            },
        }
    end

    local ok, err = game.bridge.request_save(slot_id)
    if not ok then
        return {
            ok = false,
            error = {
                code = "core:error.save.request_rejected",
                params = { error = tostring(err) },
            },
        }
    end

    return { ok = true }
end

function M.handle_load(request)
    local args = (type(request) == "table" and type(request.args) == "table") and request.args or {}
    local slot_id = args.slot_id
    local revision = args.revision or "current"

    if type(slot_id) ~= "string" or slot_id == "" then
        return {
            ok = false,
            error = {
                code = "core:error.load.invalid_slot",
                params = { slot_id = tostring(slot_id) },
            },
        }
    end

    if revision ~= "current" and revision ~= "previous" then
        return {
            ok = false,
            error = {
                code = "core:error.load.invalid_revision",
                params = { revision = tostring(revision) },
            },
        }
    end

    if not game or not game.bridge or type(game.bridge.request_load) ~= "function" then
        return {
            ok = false,
            error = {
                code = "core:error.load.bridge_unavailable",
                params = {},
            },
        }
    end

    local ok, err = game.bridge.request_load(slot_id, revision)
    if not ok then
        return {
            ok = false,
            error = {
                code = "core:error.load.request_rejected",
                params = { error = tostring(err) },
            },
        }
    end

    return { ok = true }
end

function M.register(_ctx)
    if game and game.commands and game.commands.handlers and type(game.commands.handlers.register) == "function" then
        game.commands.handlers.register("core:command.session.save", M.handle_save, {
            replaceable = false,
        })
        game.commands.handlers.register("core:command.session.load", M.handle_load, {
            replaceable = false,
        })
    end
end

return M
