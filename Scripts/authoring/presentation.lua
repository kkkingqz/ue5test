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
        local bound_default_args = nil

        if type(command_desc) == "table" then
            local act_id = command_desc.action_id or (command_desc.id and stable_id.is_kind(command_desc.id, "action") and command_desc.id)
            if act_id then
                if not (game and game.actions and game.actions.require) then
                    error("ActionNotBound: action '" .. tostring(act_id) .. "' is not bound to any command (actions registry not available)", 2)
                end
                local binding = game.actions.require(act_id)
                cmd_id = binding.command_id
                bound_default_args = binding.args
            elseif command_desc.command_id or command_desc.__command_id then
                cmd_id = command_desc.command_id or command_desc.__command_id
            else
                error("ActionInvalidDescriptor: expected command descriptor, action handle, or string, got table", 2)
            end
        elseif type(command_desc) == "string" then
            if stable_id.is_kind(command_desc, "action") then
                if not (game and game.actions and game.actions.require) then
                    error("ActionNotBound: action '" .. tostring(command_desc) .. "' is not bound to any command (actions registry not available)", 2)
                end
                local binding = game.actions.require(command_desc)
                cmd_id = binding.command_id
                bound_default_args = binding.args
            elseif stable_id.is_kind(command_desc, "command") then
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
        elseif num_args == 1 and type(raw_args[1]) == "table" and not raw_args[1].__gv2_ref and #raw_args[1] == 0 then
            if next(raw_args[1]) ~= nil then
                -- dictionary of named arguments
                canonical_args = tagged_ref.canonicalize_arg(raw_args[1], { allow_plain_id = true })
            else
                canonical_args = {}
            end
        else
            canonical_args = tagged_ref.canonicalize_args(raw_args)
        end

        if bound_default_args and next(bound_default_args) ~= nil then
            local merged = {}
            for k, v in pairs(bound_default_args) do
                merged[k] = v
            end
            if type(canonical_args) == "table" and not canonical_args.__gv2_ref and #canonical_args == 0 then
                for k, v in pairs(canonical_args) do
                    merged[k] = v
                end
                canonical_args = merged
            end
        end

        portable_value.validate(canonical_args, "action_args")

        return {
            command_id = cmd_id,
            args = canonical_args,
        }
    end
end

local function format_arg_segment(val)
    local s = nil
    if type(val) == "string" then
        s = val
    elseif type(val) == "table" and (val.__gv2_ref or val.id or val.instance_id) then
        s = val.id or val.instance_id
    elseif type(val) == "number" or type(val) == "boolean" then
        return tostring(val)
    end
    if s then
        local p = s:match("^[^:]+:[^.]+%.(.+)$")
        if p then
            return p:gsub("[^a-z0-9_]", "_")
        end
        return s:gsub("[^a-z0-9_]", "_")
    end
    return nil
end

local function derive_button_key_from_action(action_result)
    local cmd_id = action_result.command_id
    local _, _, path_str = cmd_id:match("^([^:]+):([^.]+)%.(.+)$")
    local base_key = (path_str or cmd_id):gsub("%.", "_")
    local args = action_result.args
    if not args or (type(args) == "table" and next(args) == nil) then
        return base_key
    end
    if type(args) == "table" then
        local parts = { base_key }
        if #args > 0 then
            for _, arg in ipairs(args) do
                local seg = format_arg_segment(arg)
                if seg and #seg > 0 then
                    table.insert(parts, seg)
                end
            end
        else
            local sorted_keys = {}
            for k in pairs(args) do
                table.insert(sorted_keys, k)
            end
            table.sort(sorted_keys)
            for _, k in ipairs(sorted_keys) do
                local seg = format_arg_segment(args[k])
                if seg and #seg > 0 then
                    table.insert(parts, tostring(k) .. "_" .. seg)
                end
            end
        end
        if #parts > 1 then
            return table.concat(parts, "_")
        end
    end
    return base_key
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
        if button_key ~= nil then
            if type(button_key) == "table" or (type(button_key) == "string" and (button_key:match("^[%a_][%w_]*:text%.") or button_key:match("^text:"))) then
                error("TextDisallowedAsKey: button key cannot be derived from or set to localizable text", 2)
            end
            if type(button_key) ~= "string" then
                error("InvalidButtonKey: button key must be a string", 2)
            end
            if not button_key:match("^[a-z0-9_.-@:]+$") or #button_key == 0 then
                error("InvalidButtonKey: button key must be a valid lowercase identifier, got '" .. tostring(button_key) .. "'", 2)
            end
        else
            button_key = derive_button_key_from_action(action_result)
        end

        return {
            key = button_key,
            text = text_spec,
            binding = action_result,
        }
    end
end

local active_document_state = {
    ui_instance_id = "ui@default",
    revision = 0,
    route = nil,
    overlays = {}, -- map key -> instance
    modals = {},   -- list of instances
}

function M.get_active_document_state()
    return active_document_state
end

function M.clear_active_document_for_test()
    active_document_state.ui_instance_id = "ui@default"
    active_document_state.revision = 0
    active_document_state.route = nil
    active_document_state.overlays = {}
    active_document_state.modals = {}
end

local function publish_current_document()
    local current_max = screens_module.get_current_revision(active_document_state.ui_instance_id)
    if active_document_state.revision <= current_max then
        active_document_state.revision = current_max + 1
    end
    local overlay_list = {}
    for _, inst in pairs(active_document_state.overlays) do
        table.insert(overlay_list, inst)
    end
    table.sort(overlay_list, function(a, b) return a.instance_key < b.instance_key end)

    local doc = screens_module.create_document(
        active_document_state.ui_instance_id,
        active_document_state.revision,
        active_document_state.route,
        overlay_list,
        active_document_state.modals
    )
    screens_module.publish(doc)
    return doc
end

local function build_screen_instance(package_id, spec, default_layer, default_key)
    if type(spec) ~= "table" then
        error("InvalidShowScreenSpec: specification must be a table", 3)
    end

    local template = spec.template or spec.screen_id
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
        error("InvalidScreenTemplate: template must be a screen definition handle or Stable ID of kind 'screen', got " .. tostring(template), 3)
    end

    if not stable_id.is_valid(screen_id) then
        error("InvalidScreenTemplate: invalid screen Stable ID '" .. tostring(screen_id) .. "'", 3)
    end

    local layer = spec.layer or default_layer
    local instance_key = spec.instance_key or default_key

    local fields = {}
    if spec.fields and type(spec.fields) == "table" then
        for k, v in pairs(spec.fields) do
            fields[k] = v
        end
    else
        if spec.title and spec.content then
            local title = spec.title
            if type(title) == "string" then
                error("RawStringDisallowed: modal title requires a TextSpec (use text(\"key\")), raw string is disallowed", 3)
            end
            local content = spec.content
            if type(content) == "string" then
                error("RawStringDisallowed: modal content requires a TextSpec (use text(\"key\")), raw string is disallowed", 3)
            end
            local buttons = spec.buttons or {}
            local seen_button_keys = {}
            for i, btn in ipairs(buttons) do
                if type(btn) == "string" or type(btn) ~= "table" or not btn.text or type(btn.text) == "string" then
                    error("RawStringDisallowed: button #" .. i .. " text is a raw string or missing; use text(\"key\")", 3)
                end
                if not btn.key or type(btn.key) ~= "string" or #btn.key == 0 then
                    error("UiElementKeyMissing: button #" .. i .. " is missing key", 3)
                end
                if not btn.key:match("^[a-z0-9_.-@:]+$") then
                    error("UiElementKeyInvalid: button #" .. i .. " key '" .. tostring(btn.key) .. "' is invalid", 3)
                end
                if seen_button_keys[btn.key] then
                    error("UiElementKeyDuplicate: duplicate button key '" .. tostring(btn.key) .. "'", 3)
                end
                seen_button_keys[btn.key] = true
            end
            fields["modal"] = {
                schema_id = "core:schema.ui_field.modal.v1",
                value = {
                    title = title,
                    content = content,
                    buttons = buttons,
                },
            }
        else
            local description = spec.description
            if description ~= nil then
                if type(description) == "string" then
                    error("RawStringDisallowed: show_screen() description requires a TextSpec (use text(\"key\")), raw string is disallowed", 3)
                end
                if type(description) ~= "table" or not description.text_id then
                    error("InvalidTextSpec: show_screen() description requires a valid TextSpec table with text_id", 3)
                end
                fields["description"] = {
                    schema_id = "core:schema.ui_field.rich_text.v3",
                    value = {
                        text = description,
                        spans = {},
                    },
                }
            end

            local buttons = spec.buttons or {}
            if type(buttons) ~= "table" then
                error("InvalidButtonsList: show_screen() buttons must be a list", 3)
            end

            local seen_button_keys = {}
            for i, btn in ipairs(buttons) do
                if type(btn) == "string" then
                    error("RawStringDisallowed: button #" .. i .. " is a raw string; use button(text(\"key\"), action(...))", 3)
                end
                if type(btn) ~= "table" or not btn.text or type(btn.text) == "string" then
                    error("RawStringDisallowed: button #" .. i .. " text is a raw string or missing; use text(\"key\")", 3)
                end
                if not btn.key or type(btn.key) ~= "string" or #btn.key == 0 then
                    error("UiElementKeyMissing: button #" .. i .. " is missing key", 3)
                end
                if not btn.key:match("^[a-z0-9_.-@:]+$") then
                    error("UiElementKeyInvalid: button #" .. i .. " key '" .. tostring(btn.key) .. "' is invalid", 3)
                end
                if btn.key:match("^[%a_][%w_]*:text%.") or btn.key:match("^text:") then
                    error("TextDisallowedAsKey: button #" .. i .. " key cannot be derived from localizable text", 3)
                end
                if seen_button_keys[btn.key] then
                    error("UiElementKeyDuplicate: duplicate button key '" .. tostring(btn.key) .. "' in show_screen()", 3)
                end
                seen_button_keys[btn.key] = true
            end

            fields["buttons"] = {
                schema_id = "core:schema.ui_field.button_list.v2",
                value = {
                    items = buttons,
                },
            }
        end
    end

    return screens_module.create_instance(screen_id, fields, instance_key, layer)
end

function M.create_show_screen_helper(package_id)
    return function(spec)
        local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
        if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
            ctx_mod.guard_validator_side_effect("show_screen")
        end

        local instance_key = spec and spec.instance_key or "main"
        if type(instance_key) ~= "string" or #instance_key == 0 then
            error("InvalidInstanceKey: route key must be a non-empty string", 2)
        end
        local inst = build_screen_instance(package_id, spec, "location_content", instance_key)
        active_document_state.route = inst
        return publish_current_document()
    end
end

function M.create_show_overlay_helper(package_id)
    return function(key, spec)
        local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
        if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
            ctx_mod.guard_validator_side_effect("show_overlay")
        end
        if type(key) ~= "string" or #key == 0 then
            error("InvalidInstanceKey: overlay key must be a non-empty string", 2)
        end
        local inst = build_screen_instance(package_id, spec, "overlay_stack", key)
        active_document_state.overlays[key] = inst
        return publish_current_document()
    end
end

function M.create_close_overlay_helper(_package_id)
    return function(key)
        local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
        if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
            ctx_mod.guard_validator_side_effect("close_overlay")
        end
        if type(key) == "string" and active_document_state.overlays[key] ~= nil then
            active_document_state.overlays[key] = nil
            return publish_current_document()
        end
        return nil
    end
end

function M.create_show_modal_helper(package_id)
    return function(key, spec)
        local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
        if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
            ctx_mod.guard_validator_side_effect("show_modal")
        end
        if type(key) ~= "string" or #key == 0 then
            error("InvalidInstanceKey: modal key must be a non-empty string", 2)
        end
        local inst = build_screen_instance(package_id, spec, "modal_stack", key)
        table.insert(active_document_state.modals, inst)
        return publish_current_document()
    end
end

function M.create_close_modal_helper(_package_id)
    return function(key)
        local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
        if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
            ctx_mod.guard_validator_side_effect("close_modal")
        end
        local new_modals = {}
        local removed = false
        for _, m in ipairs(active_document_state.modals) do
            if m.instance_key == key or (key == nil and not removed) then
                removed = true
            else
                table.insert(new_modals, m)
            end
        end
        if removed then
            active_document_state.modals = new_modals
            return publish_current_document()
        end
        return nil
    end
end

function M.create_tab_helper(package_id)
    return function(key, title_spec, screen_spec, fields_opt)
        if type(key) ~= "string" or not key:match("^[a-z0-9_.-@:]+$") or #key == 0 then
            error("InvalidTabKey: tab key must be a non-empty canonical identifier, got " .. tostring(key), 2)
        end
        if key:match("^[%a_][%w_]*:text%.") or key:match("^text:") then
            error("TextDisallowedAsKey: tab key cannot be derived from or set to localizable text", 2)
        end
        if type(title_spec) == "string" then
            error("RawStringDisallowed: tab() requires a TextSpec for title (use text(\"key\")), raw string is disallowed", 2)
        end
        if type(title_spec) ~= "table" or not title_spec.text_id then
            error("InvalidTextSpec: tab() requires a valid TextSpec table with text_id", 2)
        end

        local screen_id = nil
        if type(screen_spec) == "string" then
            if stable_id.is_kind(screen_spec, "screen") then
                screen_id = screen_spec
            else
                screen_id = package_id .. ":screen." .. screen_spec
            end
        elseif type(screen_spec) == "table" and screen_spec.id and stable_id.is_kind(screen_spec.id, "screen") then
            screen_id = screen_spec.id
        elseif type(screen_spec) == "table" and screen_spec.definition_id and stable_id.is_kind(screen_spec.definition_id, "screen") then
            screen_id = screen_spec.definition_id
        else
            error("InvalidScreenTemplate: tab screen must be a screen definition handle or Stable ID of kind 'screen', got " .. tostring(screen_spec), 2)
        end

        local fields = fields_opt or {}
        if type(fields) ~= "table" then
            error("InvalidFields: tab fields must be a table", 2)
        end

        return {
            key = key,
            title = title_spec,
            screen_id = screen_id,
            fields = fields,
        }
    end
end

function M.create_tab_container_helper(package_id)
    return function(spec)
        if type(spec) ~= "table" then
            error("InvalidTabContainerSpec: tab_container requires a table specification", 2)
        end
        local tabs = spec.tabs or spec
        if type(tabs) ~= "table" or #tabs == 0 then
            error("InvalidTabContainerSpec: tabs list must be a non-empty array", 2)
        end

        local default_tab_key = spec.default_tab or spec.default_tab_key

        return {
            schema_id = "core:schema.ui_field.tab_container.v1",
            value = {
                default_tab_key = default_tab_key,
                tabs = tabs,
            },
        }
    end
end

return M
