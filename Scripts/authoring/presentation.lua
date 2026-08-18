-- DLA-18, DLA-19: Designer Presentation Layer (ADR-0027)
-- Provides designer-facing helpers for text, actions, buttons, and screen publishing:
--   - text(key, args, style): creates a TextSpec table, rejects non-string/empty keys
--   - action(command_desc, ...): constructs semantic input binding { command_id, args },
--     rejects arbitrary closures with ActionClosureDisallowed
--   - button(text_spec, action_result, key_opt): constructs a button item,
--     rejects raw strings with RawStringDisallowed
--   - show_screen(spec): builds and publishes a screen request, rejects raw strings

local stable_id = require("core:module.runtime.stable_id")
local portable_value = require("core:module.runtime.portable_value")
local tagged_ref = require("core:module.authoring.tagged_ref")
local text_module = require("core:module.resources.text")
local screens_module = require("core:module.presentation.screen_requests")

local M = {
    id = "core:module.authoring.presentation",
}

function M.create_text_helper(package_id)
    return function(key, args, style)
        if type(key) ~= "string" or key == "" then
            error("InvalidTextKey: text key must be a non-empty string, got " .. tostring(key), 2)
        end
        local text_id
        if stable_id.is_kind(key, "text") then
            text_id = key
        else
            text_id = package_id .. ":text." .. key
            if not stable_id.is_valid(text_id) then
                error("InvalidTextKey: cannot construct valid text Stable ID from key '" .. key .. "' (constructed '" .. text_id .. "')", 2)
            end
        end
        return text_module.spec(text_id, args, style or "default")
    end
end

function M.create_action_helper(package_id)
    return function(command_desc, ...)
        if type(command_desc) == "function" then
            error("ActionClosureDisallowed: action() requires a command descriptor (e.g. commands.foo), arbitrary closures cannot produce semantic input", 2)
        end

        local cmd_id = nil
        if type(command_desc) == "table" and (command_desc.command_id or command_desc.__command_id) then
            cmd_id = command_desc.command_id or command_desc.__command_id
        elseif type(command_desc) == "string" then
            if stable_id.is_kind(command_desc, "command") then
                cmd_id = command_desc
            else
                cmd_id = package_id .. ":command." .. command_desc
            end
        else
            error("ActionInvalidDescriptor: expected command descriptor or string, got " .. type(command_desc), 2)
        end

        if not stable_id.is_valid(cmd_id) then
            error("ActionInvalidCommandId: invalid command Stable ID '" .. tostring(cmd_id) .. "'", 2)
        end

        local num_args = select("#", ...)
        local raw_args = { ... }
        local canonical_args
        if num_args == 0 then
            canonical_args = {}
        elseif num_args == 1 and type(raw_args[1]) == "table" and not raw_args[1].__gv2_ref and #raw_args[1] == 0 and next(raw_args[1]) ~= nil then
            -- dictionary of named arguments
            canonical_args = tagged_ref.canonicalize_arg(raw_args[1], { allow_plain_id = true })
        else
            canonical_args = tagged_ref.canonicalize_args(raw_args)
        end

        portable_value.validate(canonical_args, "action_args")

        return {
            command_id = cmd_id,
            args = canonical_args,
        }
    end
end

function M.create_button_helper(package_id)
    return function(text_spec, action_result, key_opt)
        if type(text_spec) == "string" then
            error("RawStringDisallowed: button() requires a TextSpec (use text(\"key\")), raw string is disallowed", 2)
        end
        if type(text_spec) ~= "table" or not text_spec.text_id then
            error("InvalidTextSpec: button() requires a valid TextSpec table with text_id", 2)
        end

        if type(action_result) ~= "table" or not action_result.command_id then
            error("InvalidActionBinding: button() requires an action binding returned by action()", 2)
        end

        local button_key = key_opt
        if not button_key then
            local cmd_id = action_result.command_id
            local _, _, path_str = cmd_id:match("^([^:]+):([^.]+)%.(.+)$")
            button_key = (path_str or cmd_id):gsub("%.", "_")
        end

        return {
            key = button_key,
            text = text_spec,
            binding = action_result,
        }
    end
end

function M.create_show_screen_helper(package_id)
    return function(spec)
        if type(spec) ~= "table" then
            error("InvalidShowScreenSpec: show_screen() requires a table specification", 2)
        end

        local template = spec.template
        local screen_id = nil
        if type(template) == "string" then
            if stable_id.is_kind(template, "screen") then
                screen_id = template
            else
                screen_id = package_id .. ":screen." .. template
            end
        elseif type(template) == "table" and template.id and stable_id.is_kind(template.id, "screen") then
            screen_id = template.id
        elseif type(template) == "table" and template.definition_id and stable_id.is_kind(template.definition_id, "screen") then
            screen_id = template.definition_id
        else
            error("InvalidScreenTemplate: template must be a screen definition handle or Stable ID of kind 'screen', got " .. tostring(template), 2)
        end

        if not stable_id.is_valid(screen_id) then
            error("InvalidScreenTemplate: invalid screen Stable ID '" .. tostring(screen_id) .. "'", 2)
        end

        local description = spec.description
        if type(description) == "string" then
            error("RawStringDisallowed: show_screen() description requires a TextSpec (use text(\"key\")), raw string is disallowed", 2)
        end
        if type(description) ~= "table" or not description.text_id then
            error("InvalidTextSpec: show_screen() description requires a valid TextSpec table with text_id", 2)
        end

        local buttons = spec.buttons or {}
        if type(buttons) ~= "table" then
            error("InvalidButtonsList: show_screen() buttons must be a list", 2)
        end

        for i, btn in ipairs(buttons) do
            if type(btn) == "string" then
                error("RawStringDisallowed: button #" .. i .. " is a raw string; use button(text(\"key\"), action(...))", 2)
            end
            if type(btn) ~= "table" or not btn.text or type(btn.text) == "string" then
                error("RawStringDisallowed: button #" .. i .. " text is a raw string or missing; use text(\"key\")", 2)
            end
        end

        local screen_req = screens_module.create(screen_id, {
            description = {
                schema_id = "core:schema.ui_field.rich_text.v3",
                value = {
                    text = description,
                    spans = {},
                },
            },
            buttons = {
                schema_id = "core:schema.ui_field.button_list.v2",
                value = {
                    items = buttons,
                },
            },
        })

        screens_module.publish(screen_req)
        return screen_req
    end
end

return M
