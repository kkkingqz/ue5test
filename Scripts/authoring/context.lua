-- DLA-05, DLA-06, DLA-08: Designer Authoring Module Context (ADR-0027)
-- Provides the `gameplay(package_id)` factory that produces module descriptors `M`:
--   - M.commands: proxy table collecting declared command handlers during module load
--   - M.fail(key, params): typed refusal returning <package_id>:error.<key>, enforces
--     the mutation rule (throws AuthoringFailAfterMutation if write_revision increased)
--   - M.action(cmd_desc, ...): semantic action constructor
--   - M.player: dynamic accessor proxy to game.instances.actors.player()
--   - M.world: dynamic accessor proxy to game.instances.world()
--   - M.actor(name): unique instance accessor (0 -> ActorInstanceNotFound, 1 -> wrapper, 2+ -> ActorInstanceAmbiguous)
--   - M.actors(name): list accessor for multiple instances
--   - M.register(ctx): lifecycle hook registering declared commands with deterministic ordering

local stable_id = require("core:module.runtime.stable_id")
local mutation_window = require("core:module.runtime.mutation_window")
local portable_value = require("core:module.runtime.portable_value")
local tagged_ref = require("core:module.authoring.tagged_ref")
local commands_module = require("core:module.authoring.commands")
local properties_module = require("core:module.authoring.properties")
local presentation_module = require("core:module.authoring.presentation")
local entity_extension_registry = require("core:module.runtime.entity_extension_registry")
local field_module = require("core:module.authoring.field")
local instance_registry = require("core:module.runtime.instance_registry")
local test_state_registry = require("core:module.runtime.test_state_registry")
local mode_store = require("core:module.runtime.mode_store")

local M = {
    id = "core:module.authoring.context",
}

local current_scope = { kind = "none" }
local all_declared_validators = {}
local all_referenced_services = {}
local is_frozen = false
local FAIL_SENTINEL = setmetatable({}, { __tostring = function() return "AuthoringFailRefusal" end })

-- GAP-003/EGC-04: these four are the registry's first participants. snapshot/
-- restore/reset below sweep test_state_registry rather than naming variables
-- directly; a fifth participant (EGC-05's mode_store) registers itself the same
-- way, elsewhere, without touching this file.
test_state_registry.register_participant("authoring.current_scope", {
    snapshot = function() return current_scope end,
    restore = function(v) current_scope = v end,
    reset = function() current_scope = { kind = "none" } end,
})
test_state_registry.register_participant("authoring.all_declared_validators", {
    snapshot = function() return all_declared_validators end,
    restore = function(v) all_declared_validators = v end,
    reset = function() all_declared_validators = {} end,
})
test_state_registry.register_participant("authoring.all_referenced_services", {
    snapshot = function() return all_referenced_services end,
    restore = function(v) all_referenced_services = v end,
    reset = function() all_referenced_services = {} end,
})
test_state_registry.register_participant("authoring.is_frozen", {
    snapshot = function() return is_frozen end,
    restore = function(v) is_frozen = v end,
    reset = function() is_frozen = false end,
})

local function is_fail_result(val)
    return type(val) == "table" and val.__gv2_fail == FAIL_SENTINEL
end

function M.get_current_scope()
    return current_scope
end

function M.guard_validator_side_effect(operation_name)
    if current_scope.kind == "validator" then
        error("AuthoringValidatorSideEffectDisallowed: validator '" .. tostring(current_scope.validator_id)
            .. "' in package '" .. tostring(current_scope.package_id)
            .. "' attempted side-effect operation '" .. tostring(operation_name or "unknown") .. "'", 2)
    end
end

-- EGC-05/ADR-0051 §D3: `view` is a pure projection the presentation builder
-- calls, not a command operation -- by analogy with guard_validator_side_effect,
-- guarded by execution scope rather than by trusting every future caller.
-- Lives here (not in mode_store.lua) because only context.lua holds
-- current_scope, and mode_store must not require context.lua back (cycle).
function M.call_active_mode_view()
    if current_scope.kind == "command" then
        error("ModeViewCalledFromCommand: view() must be called by the presentation builder, not from within a command handler", 2)
    end
    local active_id = mode_store.active()
    if active_id == nil then
        return nil
    end
    local decl = mode_store.get_declaration(active_id)
    return decl.view_fn(_G.game and _G.game.state, mode_store.run_proxy())
end

local function canonicalize_error_id(package_id, key)
    if stable_id.is_kind(key, "error") then
        return key
    end

    if type(key) ~= "string" or key == "" then
        error("InvalidErrorKey: expected non-empty string error key, got " .. tostring(key), 3)
    end

    local canonical = package_id .. ":error." .. key
    if not stable_id.is_valid(canonical) then
        error("InvalidErrorKey: cannot construct valid error Stable ID from key '" .. key .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

local function canonicalize_actor_def_id(package_id, name)
    if stable_id.is_kind(name, "actor") then
        return name
    end

    if type(name) ~= "string" or name == "" then
        error("InvalidActorName: expected non-empty string actor name, got " .. tostring(name), 3)
    end

    local canonical = package_id .. ":actor." .. name
    if not stable_id.is_valid(canonical) then
        error("InvalidActorName: cannot construct valid actor definition Stable ID from name '" .. name .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

local function canonicalize_event_id(package_id, name)
    if stable_id.is_kind(name, "event") then
        return name
    end

    if type(name) ~= "string" or name == "" then
        error("InvalidEventName: expected non-empty string event name, got " .. tostring(name), 3)
    end

    local canonical = package_id .. ":event." .. name
    if not stable_id.is_valid(canonical) then
        error("InvalidEventName: cannot construct valid event Stable ID from name '" .. name .. "' (constructed '" .. canonical .. "')", 3)
    end

    return canonical
end

local function decode_authoring_args(raw_args)
    if raw_args == nil then
        return {}
    end
    local rehydrated = tagged_ref.rehydrate_args(raw_args)
    local is_array = (type(raw_args) == "table" and #raw_args > 0 and raw_args[1] ~= nil)
    if is_array then
        local args_list = {}
        for i = 1, #rehydrated do
            local arg = rehydrated[i]
            if type(arg) == "string" and stable_id.is_valid(arg) then
                if game and game.repository and game.repository.get and game.repository.get(arg) then
                    local ok_p, properties_mod = pcall(require, "core:module.authoring.properties")
                    if ok_p and properties_mod and properties_mod.wrap_definition then
                        arg = properties_mod.wrap_definition(arg)
                    end
                end
            end
            args_list[i] = arg
        end
        return args_list
    elseif type(raw_args) == "table" and next(raw_args) == nil then
        return {}
    else
        return { rehydrated }
    end
end
M.decode_authoring_args = decode_authoring_args

function M.create_player_proxy()
    return setmetatable({}, {
        __index = function(_, key)
            if not (game and game.instances and game.instances.actors and game.instances.actors.player) then
                error("PlayerActorNotFound: actors registry is not available", 2)
            end
            local player = game.instances.actors.player()
            if not player then
                error("PlayerActorNotFound: player actor does not exist in state.actors", 2)
            end
            return player[key]
        end,
        __newindex = function(_, key, val)
            if not (game and game.instances and game.instances.actors and game.instances.actors.player) then
                error("PlayerActorNotFound: actors registry is not available", 2)
            end
            local player = game.instances.actors.player()
            if not player then
                error("PlayerActorNotFound: player actor does not exist in state.actors", 2)
            end
            player[key] = val
        end,
    })
end

function M.create_world_proxy()
    return setmetatable({}, {
        __index = function(_, key)
            if not (game and game.instances and game.instances.world) then
                error("WorldInstanceNotFound: world registry is not available", 2)
            end
            local w = game.instances.world()
            if not w then
                error("WorldInstanceNotFound: game.instances.world() returned nil", 2)
            end
            return w[key]
        end,
        __newindex = function(_, key, val)
            if not (game and game.instances and game.instances.world) then
                error("WorldInstanceNotFound: world registry is not available", 2)
            end
            local w = game.instances.world()
            if not w then
                error("WorldInstanceNotFound: game.instances.world() returned nil", 2)
            end
            w[key] = val
        end,
    })
end

function M.create_entity_prototype_proxy(entity_kind, pkg_id, opt_mod_id)
    local mod_id = opt_mod_id or (pkg_id .. ":authoring." .. entity_kind:lower())
    local proxy = {}
    local mt = {
        __newindex = function(_, member_name, val)
            if type(member_name) ~= "string" or member_name == "" then
                error("InvalidMemberName: expected non-empty string member name for " .. tostring(entity_kind) .. ", got " .. tostring(member_name), 2)
            end
            if type(val) == "table" and val.__gv2_field_descriptor then
                properties_module.register_schema(entity_kind, {
                    fields = {
                        [member_name] = val,
                    },
                })
            elseif type(val) == "function" then
                if game and game.entity_extensions and game.entity_extensions.register then
                    game.entity_extensions.register(mod_id, pkg_id, entity_kind, member_name, val)
                else
                    entity_extension_registry.register(mod_id, pkg_id, entity_kind, member_name, val)
                end
            else
                error("InvalidPrototypeAssignment: expected function or field descriptor for " .. tostring(entity_kind) .. "." .. tostring(member_name) .. ", got " .. type(val), 2)
            end
        end,
        __index = function(_, member_name)
            if game and game.entity_extensions and game.entity_extensions.get_method then
                local fn = game.entity_extensions.get_method(entity_kind, member_name)
                if fn ~= nil then
                    return fn
                end
            end
            local schema = properties_module.get_schema(entity_kind)
            if schema and schema.fields and schema.fields[member_name] then
                return schema.fields[member_name]
            end
            return nil
        end,
        __tostring = function(_)
            return entity_kind .. "AuthoringPrototype"
        end,
    }
    setmetatable(proxy, mt)
    return proxy
end

function M.gameplay(package_id, opt_module_id)
    if type(package_id) ~= "string" or package_id == "" then
        error("InvalidPackageId: package_id must be non-empty string, got " .. tostring(package_id), 2)
    end

    local commands_proxy, proxy_ctrl = commands_module.create_commands_proxy(package_id)
    local subscribers = {}
    local declared_actions = {}
    local declared_services = {}
    local declared_services_order = {}
    local is_module_frozen = false

    local services_proxy = setmetatable({}, {
        __newindex = function(_, key, val)
            if is_module_frozen or (proxy_ctrl and proxy_ctrl.is_frozen and proxy_ctrl.is_frozen()) then
                error("ServiceDeclarationAfterFreeze: cannot declare service '" .. tostring(key) .. "' after freeze in package '" .. package_id .. "'", 2)
            end

            if type(key) ~= "string" or key == "" or not key:match("^[a-z][a-z0-9_]*$") then
                error("InvalidServiceName: service name must be a single lowercase alphanumeric segment, got '" .. tostring(key) .. "'", 2)
            end

            if type(val) ~= "table" then
                error("InvalidServiceImplementation: service implementation must be a table, got " .. type(val), 2)
            end

            for field_name, field_val in pairs(val) do
                if type(field_val) ~= "function" then
                    error("ServiceFieldNotFunction: service '" .. key .. "' field '" .. tostring(field_name) .. "' must be a function, got " .. type(field_val), 2)
                end
            end

            local service_id = package_id .. ":service." .. key
            if declared_services[key] ~= nil or (game and game.services and game.services.exists and game.services.exists(service_id)) then
                error("ServiceDuplicateDeclaration: service '" .. key .. "' (id '" .. service_id .. "') is already declared in package '" .. package_id .. "'", 2)
            end

            local immutable_service = {}
            local service_mt = {
                __index = function(_, k)
                    return val[k]
                end,
                __newindex = function(_, k, _)
                    error("ServiceImmutableAfterRegistration: service '" .. key .. "' is immutable after registration", 2)
                end,
                __tostring = function(_)
                    return "GameplayService(" .. service_id .. ")"
                end,
            }
            setmetatable(immutable_service, service_mt)

            local decl = {
                service_id = service_id,
                name = key,
                impl = immutable_service,
                raw_table = val,
                declaring_package = package_id,
            }
            declared_services[key] = decl
            table.insert(declared_services_order, decl)

            if game and game.services and game.services.register and not (game.services.exists and game.services.exists(service_id)) then
                game.services.register(service_id, immutable_service)
            end

            return immutable_service
        end,

        __index = function(_, key)
            if type(key) ~= "string" or key == "" then
                error("InvalidServiceName: expected string key, got " .. type(key), 2)
            end

            if stable_id.is_kind(key, "service") then
                local ref = {
                    declaring_package = package_id,
                    requested_id = key,
                }
                table.insert(all_referenced_services, ref)

                if game and game.services and game.services.exists and game.services.exists(key) then
                    return game.services.get(key)
                end

                local lazy_proxy = setmetatable({}, {
                    __index = function(_, method_name)
                        if not (game and game.services and game.services.exists and game.services.exists(key)) then
                            error("ServiceTargetMissing: service '" .. tostring(key) .. "' requested by package '" .. package_id .. "' is not registered", 2)
                        end
                        local svc = game.services.get(key)
                        local fn = svc[method_name]
                        if fn == nil then
                            error("ServiceMethodNotFound: method '" .. tostring(method_name) .. "' not found on service '" .. tostring(key) .. "'", 2)
                        end
                        return fn
                    end,
                    __newindex = function(_, _, _)
                        error("ServiceImmutableAfterRegistration: service '" .. tostring(key) .. "' is immutable after registration", 2)
                    end,
                    __tostring = function(_)
                        return "LazyGameplayService(" .. key .. ")"
                    end,
                })
                return lazy_proxy
            end

            local decl = declared_services[key]
            if decl then
                return decl.impl
            end

            error("UnknownServiceKey: service '" .. tostring(key) .. "' is not declared in package '" .. package_id .. "'", 2)
        end,
    })

    -- EGC-05/ADR-0051 §D2: `modes.<key> = { start, view }` declares a mode the
    -- same way `services.<key> = {...}` declares a service (duplicate/late
    -- declaration rejected the same way); `modes.<key>.<field> = field.*(...)`
    -- then declares run content on the handle it returned, the same way
    -- `Actor.gold = field.*(...)` declares an entity field. The seven verb
    -- names (ADR-0051's six plus EGC-07's draw_u32) are reserved and can
    -- never be used as a mode key.
    local RESERVED_MODE_KEYS = {
        start = true, require_active = true, state = true,
        finish = true, cancel = true, active = true, draw_u32 = true,
    }
    local declared_mode_handles = {}

    local function resolve_mode_id(key_or_id)
        if type(key_or_id) ~= "string" or key_or_id == "" then
            error("InvalidModeReference: expected mode key or Stable ID, got " .. tostring(key_or_id), 2)
        end
        if stable_id.is_kind(key_or_id, "mode") then
            return key_or_id
        end
        return package_id .. ":mode." .. key_or_id
    end

    local function mode_fail(key, params)
        return M.fail(key, params, "core")
    end

    local function make_run_wrapper(decl)
        local run_proxy = mode_store.run_proxy()
        return setmetatable({}, {
            __index = function(_, key)
                local val = run_proxy[key]
                if val ~= nil then
                    return val
                end
                local field_spec = decl.fields[key]
                return field_spec and field_spec.default or nil
            end,
            __newindex = function(_, key, value)
                local field_spec = decl.fields[key]
                if not field_spec then
                    error("ModeRunFieldUnknown: field '" .. tostring(key) .. "' is not declared for mode '" .. decl.mode_id .. "'", 2)
                end
                local write_policy = field_spec.write_policy or "plain"
                if write_policy == "read_only" then
                    error("ModeRunFieldReadOnly: cannot modify read_only field '" .. tostring(key) .. "' of mode '" .. decl.mode_id .. "'", 2)
                elseif write_policy == "managed" then
                    local ops_str = ""
                    if type(field_spec.operations) == "table" and #field_spec.operations > 0 then
                        ops_str = " (use domain operations: " .. table.concat(field_spec.operations, ", ") .. ")"
                    end
                    error("ModeRunFieldManaged: cannot assign directly to managed field '" .. tostring(key) .. "' of mode '" .. decl.mode_id .. "'" .. ops_str, 2)
                end
                local _, canonical_val = properties_module.validate_field_value(key, field_spec, value)
                run_proxy[key] = canonical_val
            end,
            __tostring = function(_)
                return "ModeRunWrapper(" .. decl.mode_id .. ")"
            end,
        })
    end

    local function modes_active()
        return mode_store.active()
    end

    local function modes_require_active(opt_key_or_id)
        local active_id = mode_store.active()
        if active_id == nil then
            mode_fail("mode.not_active", {})
        end
        if opt_key_or_id ~= nil then
            local expected_id = resolve_mode_id(opt_key_or_id)
            if expected_id ~= active_id then
                mode_fail("mode.not_active", { expected_mode_id = expected_id, active_mode_id = active_id })
            end
        end
        return active_id
    end

    local function modes_state()
        local active_id = mode_store.active()
        if active_id == nil then
            mode_fail("mode.not_active", {})
        end
        local decl = mode_store.get_declaration(active_id)
        return make_run_wrapper(decl)
    end

    local function modes_start(key_or_id, ...)
        if mode_store.active() ~= nil then
            mode_fail("mode.already_active", { active_mode_id = mode_store.active() })
        end
        local mode_id = resolve_mode_id(key_or_id)
        local decl = mode_store.get_declaration(mode_id)
        if decl == nil then
            error("UnknownModeId: mode '" .. mode_id .. "' is not declared", 2)
        end

        mode_store.begin(mode_id)

        for field_name, field_spec in pairs(decl.fields) do
            if field_spec.kind == "random_stream" then
                if not (game and game.random and game.random.next_u32) then
                    error("GameRandomNotAvailable: game.random is not available to seed mode '" .. mode_id .. "'", 2)
                end
                local seed = game.random.next_u32("core:random_stream.mode_seed")
                mode_store.seed_random_stream_field(field_name, seed)
            end
        end

        return decl.start_fn(...)
    end

    local function modes_finish()
        if mode_store.active() == nil then
            mode_fail("mode.not_active", {})
        end
        mode_store.end_mode("finished")
    end

    local function modes_cancel()
        if mode_store.active() == nil then
            mode_fail("mode.not_active", {})
        end
        mode_store.end_mode("cancelled")
    end

    -- EGC-07: the authoring-facing draw call a mode's own commands need to
    -- make a roll (e.g. the fixture's `roll_dice`), so content never has to
    -- reach past `modes` into core:module.runtime.mode_store directly to
    -- exercise its own declared field.random_stream(). Validates the field
    -- is actually a random_stream of the active mode before stepping it, so
    -- a typo'd field name fails the same way an unknown run field would.
    local function modes_draw_u32(field_name)
        local active_id = mode_store.active()
        if active_id == nil then
            mode_fail("mode.not_active", {})
        end
        local decl = mode_store.get_declaration(active_id)
        local field_spec = decl.fields[field_name]
        if not field_spec or field_spec.kind ~= "random_stream" then
            error("ModeRunFieldNotRandomStream: field '" .. tostring(field_name)
                .. "' is not a declared field.random_stream() of mode '" .. active_id .. "'", 2)
        end
        return mode_store.draw_u32(field_name, field_spec)
    end

    local MODE_VERBS = {
        start = modes_start,
        require_active = modes_require_active,
        state = modes_state,
        finish = modes_finish,
        cancel = modes_cancel,
        active = modes_active,
        draw_u32 = modes_draw_u32,
    }

    local modes_proxy = setmetatable({}, {
        __newindex = function(_, key, val)
            if RESERVED_MODE_KEYS[key] then
                error("ModeReservedKeyName: '" .. tostring(key) .. "' is a reserved modes.* operation and cannot be declared as a mode key", 2)
            end
            if is_module_frozen or (proxy_ctrl and proxy_ctrl.is_frozen and proxy_ctrl.is_frozen()) then
                error("ModeDeclarationAfterFreeze: cannot declare mode '" .. tostring(key) .. "' after freeze in package '" .. package_id .. "'", 2)
            end
            if type(key) ~= "string" or key == "" or not key:match("^[a-z][a-z0-9_]*$") then
                error("InvalidModeName: mode name must be a single lowercase alphanumeric segment, got '" .. tostring(key) .. "'", 2)
            end
            if type(val) ~= "table" or type(val.start) ~= "function" then
                error("InvalidModeDeclaration: mode '" .. key .. "' must declare a 'start' function", 2)
            end
            if type(val.view) ~= "function" then
                error("ModeMissingView: mode '" .. key .. "' must declare a 'view' function", 2)
            end
            if declared_mode_handles[key] ~= nil then
                error("ModeDuplicateDeclaration: mode '" .. key .. "' is already declared in package '" .. package_id .. "'", 2)
            end

            local mode_id = package_id .. ":mode." .. key
            local fields = {}
            local decl = {
                mode_id = mode_id,
                start_fn = val.start,
                view_fn = val.view,
                fields = fields,
                declaring_package = package_id,
            }
            mode_store.declare(mode_id, decl)

            local handle = setmetatable({}, {
                __newindex = function(_, field_name, field_val)
                    if is_module_frozen or (proxy_ctrl and proxy_ctrl.is_frozen and proxy_ctrl.is_frozen()) then
                        error("ModeFieldDeclarationAfterFreeze: cannot declare field '" .. tostring(field_name) .. "' for mode '" .. mode_id .. "' after freeze", 2)
                    end
                    if type(field_val) ~= "table" or not field_val.__gv2_field_descriptor then
                        error("InvalidModeFieldDescriptor: mode '" .. key .. "' field '" .. tostring(field_name) .. "' must be a field.* descriptor, got " .. type(field_val), 2)
                    end
                    if fields[field_name] ~= nil then
                        error("ModeFieldDuplicateDeclaration: mode '" .. key .. "' field '" .. tostring(field_name) .. "' is already declared", 2)
                    end
                    if field_val.kind == "random_stream" then
                        for existing_name, existing_spec in pairs(fields) do
                            if existing_spec.kind == "random_stream" then
                                error("ModeMultipleRandomStreams: mode '" .. key .. "' already declares a random_stream field '" .. existing_name .. "'", 2)
                            end
                        end
                    end
                    fields[field_name] = field_val
                end,
                __index = function(_, field_name)
                    return fields[field_name]
                end,
                __tostring = function(_)
                    return "ModeHandle(" .. mode_id .. ")"
                end,
            })
            declared_mode_handles[key] = handle
        end,
        __index = function(_, key)
            if MODE_VERBS[key] then
                return MODE_VERBS[key]
            end
            local handle = declared_mode_handles[key]
            if handle then
                return handle
            end
            error("UnknownModeKey: '" .. tostring(key) .. "' is not a modes operation or declared mode in package '" .. package_id .. "'", 2)
        end,
    })

    local actions_proxy = setmetatable({}, {
        __newindex = function(_, key, val)
            local action_id
            if stable_id.is_kind(key, "action") then
                action_id = key
            elseif type(key) == "string" and key ~= "" then
                action_id = package_id .. ":action." .. key
            else
                error("InvalidActionKey: expected string key, got " .. type(key), 2)
            end

            local cmd_id = nil
            local args = {}
            if type(val) == "string" then
                if stable_id.is_kind(val, "command") then
                    cmd_id = val
                else
                    cmd_id = package_id .. ":command." .. val
                end
            elseif type(val) == "table" then
                local raw_cmd = val.command_id or val.command or val.__command_id
                if type(raw_cmd) == "string" then
                    if stable_id.is_kind(raw_cmd, "command") then
                        cmd_id = raw_cmd
                    else
                        cmd_id = package_id .. ":command." .. raw_cmd
                    end
                elseif type(raw_cmd) == "table" and raw_cmd.command_id then
                    cmd_id = raw_cmd.command_id
                else
                    error("InvalidActionBinding: expected command_id string or descriptor in table", 2)
                end
                args = val.args or {}
            else
                error("InvalidActionValue: expected string command ID or binding table, got " .. type(val), 2)
            end

            declared_actions[action_id] = {
                command_id = cmd_id,
                args = args,
            }
        end,
        __index = function(_, key)
            local action_id
            if stable_id.is_kind(key, "action") then
                action_id = key
            elseif type(key) == "string" and key ~= "" then
                action_id = package_id .. ":action." .. key
            else
                return nil
            end
            return declared_actions[action_id]
        end,
    })

    local mod = {}

    mod.commands = commands_proxy
    mod.actions = actions_proxy
    mod.services = services_proxy
    mod.modes = modes_proxy
    mod.action = presentation_module.create_action_helper(package_id)
    mod.button = presentation_module.create_button_helper(package_id)
    mod.text = presentation_module.create_text_helper(package_id)
    mod.show_screen = presentation_module.create_show_screen_helper(package_id)
    mod.show_route = mod.show_screen
    mod.show_overlay = presentation_module.create_show_overlay_helper(package_id)
    mod.close_overlay = presentation_module.create_close_overlay_helper(package_id)
    mod.show_modal = presentation_module.create_show_modal_helper(package_id)
    mod.close_modal = presentation_module.create_close_modal_helper(package_id)
    mod.tab = presentation_module.create_tab_helper(package_id)
    mod.tab_container = presentation_module.create_tab_container_helper(package_id)
    mod.tabs = mod.tab_container
    mod.player = M.create_player_proxy()
    mod.world = M.create_world_proxy()
    mod.Actor = M.create_entity_prototype_proxy("Actor", package_id, opt_module_id)
    mod.Location = M.create_entity_prototype_proxy("Location", package_id, opt_module_id)
    mod.Quest = M.create_entity_prototype_proxy("Quest", package_id, opt_module_id)
    mod.Item = M.create_entity_prototype_proxy("Item", package_id, opt_module_id)
    mod.field = field_module

    local instances_facade = {
        create = function(kind, data)
            if game and game.instances and game.instances.create then
                return game.instances.create(kind, data)
            end
            return instance_registry.create(kind, data)
        end,
        register_kind = function(kind, opt_spec)
            if game and game.instances and game.instances.register_kind then
                return game.instances.register_kind(kind, opt_spec)
            end
            return instance_registry.register_kind(kind, opt_spec)
        end,
    }
    mod.instances = instances_facade

    local def_proxy = setmetatable({}, {
        __index = function(_, kind)
            return function(name)
                local full_id
                if type(name) == "table" and (name.id or name.definition_id) then
                    full_id = name.id or name.definition_id
                elseif type(name) == "string" and stable_id.is_valid(name) then
                    full_id = name
                elseif type(name) == "string" then
                    full_id = package_id .. ":" .. kind .. "." .. name
                else
                    error("InvalidDefinitionName: expected string or definition handle, got " .. type(name), 2)
                end
                return properties_module.wrap_definition(full_id)
            end
        end,
    })
    mod.def = def_proxy

    function mod.location(name)
        return def_proxy.location(name)
    end

    function mod.emit(event_name, payload)
        M.guard_validator_side_effect("emit")
        local canonical_event_id = canonicalize_event_id(package_id, event_name)
        local canonical_payload = tagged_ref.canonicalize_arg(payload or {})
        portable_value.validate(canonical_payload, "event_payload")

        if game and game.events and game.events.enqueue then
            game.events.enqueue({
                event_id = canonical_event_id,
                payload = canonical_payload,
            })
        elseif game and game.events and game.events.publish then
            game.events.publish({
                event_id = canonical_event_id,
                payload = canonical_payload,
            })
        else
            error("EventsNotAvailable: game.events is not available", 2)
        end
    end

    function mod.on(event_name, handler_fn)
        if type(handler_fn) ~= "function" then
            error("InvalidEventHandler: handler must be a function, got " .. type(handler_fn), 2)
        end
        local canonical_event_id = canonicalize_event_id(package_id, event_name)
        table.insert(subscribers, {
            event_name = event_name,
            event_id = canonical_event_id,
            handler = handler_fn,
        })
    end

    function M.fail(key, params, opt_pkg)
        if current_scope.kind ~= "command" and current_scope.kind ~= "validator" then
            error("AuthoringFailOutsideCommand: fail() can only be called from within an active command handler or validator", 2)
        end

        if current_scope.kind == "command" then
            -- ADR-0050 §2.3: the signal is a non-empty canonical slice of the
            -- current transaction's journal, not write_revision growth — the
            -- counter is not rolled back and does not distinguish category.
            if mutation_window.has_mutations("canonical") then
                error("AuthoringFailAfterMutation: command '" .. tostring(current_scope.command_id)
                    .. "' attempted to fail() after state mutation."
                    .. " Move precondition checks before state mutations.", 2)
            end
        end

        local pkg = opt_pkg or current_scope.package_id or "core"
        local canonical_code = canonicalize_error_id(pkg, key)
        local canonical_params = tagged_ref.canonicalize_arg(params or {}, { allow_plain_id = true })
        portable_value.validate(canonical_params, "fail_params")

        local fail_obj = {
            ok = false,
            error = {
                code = canonical_code,
                params = canonical_params,
            },
            __gv2_fail = FAIL_SENTINEL,
        }

        -- Non-local exit (SAS-10, SAS-11, CVA-05)
        error(fail_obj, 0)
    end

    function mod.fail(key, params)
        return M.fail(key, params, package_id)
    end

    mod.guard_validator_side_effect = M.guard_validator_side_effect

    local declared_validators = {}
    local declared_validators_order = {}

    function mod.validate(command_ref, validator_name, validator_fn)
        if is_module_frozen or (proxy_ctrl and proxy_ctrl.is_frozen and proxy_ctrl.is_frozen()) then
            error("AuthoringValidatorDeclarationAfterFreeze: cannot declare validator after freeze in package '" .. package_id .. "'", 2)
        end

        local target_command_id = nil
        if type(command_ref) == "table" and command_ref.command_id then
            if type(command_ref.command_id) == "string" and stable_id.is_kind(command_ref.command_id, "command") then
                target_command_id = command_ref.command_id
            end
        elseif type(command_ref) == "string" and stable_id.is_kind(command_ref, "command") then
            target_command_id = command_ref
        end

        if not target_command_id then
            error("InvalidAuthoringValidatorCommand: command_ref must be a CommandDescriptor or canonical command Stable ID, got " .. tostring(command_ref), 2)
        end

        if type(validator_name) ~= "string" or validator_name == "" or not validator_name:match("^[a-z0-9_]+$") then
            error("InvalidAuthoringValidatorName: validator_name must be a single lowercase alphanumeric segment, got " .. tostring(validator_name), 2)
        end

        if type(validator_fn) ~= "function" then
            error("InvalidAuthoringValidatorFunction: validator_fn must be a function, got " .. type(validator_fn), 2)
        end

        local val_key = target_command_id .. ":" .. validator_name
        if declared_validators[val_key] then
            error("AuthoringValidatorDuplicate: validator '" .. validator_name .. "' for command '" .. target_command_id .. "' is already declared in package '" .. package_id .. "'", 2)
        end

        local target_pkg, target_path = target_command_id:match("^([a-z0-9_]+):command%.([a-z0-9_.]+)$")
        if not target_pkg or not target_path then
            error("InvalidAuthoringValidatorCommand: cannot parse target command Stable ID '" .. target_command_id .. "'", 2)
        end
        local validator_id = package_id .. ":validator." .. target_pkg .. "." .. target_path .. "." .. validator_name

        local decl = {
            validator_id = validator_id,
            target_command_id = target_command_id,
            target_namespace = target_pkg,
            target_command_path = target_path,
            name = validator_name,
            declaring_package = package_id,
            fn = validator_fn,
        }
        declared_validators[val_key] = decl
        table.insert(declared_validators_order, decl)
        table.insert(all_declared_validators, decl)

        return decl
    end

    function mod.actor(name)
        local canonical_def_id = canonicalize_actor_def_id(package_id, name)
        if not (game and game.instances and game.instances.actors and game.instances.actors.ids) then
            error("ActorInstanceNotFound: actors registry is not available", 2)
        end

        local all_ids = game.instances.actors.ids()
        local matched = {}
        for _, id in ipairs(all_ids) do
            local inst = game.instances.actors.get(id)
            if inst and inst.definition_id == canonical_def_id then
                table.insert(matched, inst)
            end
        end

        if #matched == 0 then
            error("ActorInstanceNotFound: no instance found for actor definition '" .. canonical_def_id .. "'", 2)
        elseif #matched == 1 then
            return matched[1]
        else
            error("ActorInstanceAmbiguous: multiple instances (" .. #matched .. ") found for actor definition '" .. canonical_def_id .. "'. Use M.actors() instead.", 2)
        end
    end

    function mod.actors(name)
        local canonical_def_id = canonicalize_actor_def_id(package_id, name)
        if not (game and game.instances and game.instances.actors and game.instances.actors.ids) then
            return {}
        end

        local all_ids = game.instances.actors.ids()
        local matched = {}
        for _, id in ipairs(all_ids) do
            local inst = game.instances.actors.get(id)
            if inst and inst.definition_id == canonical_def_id then
                table.insert(matched, inst)
            end
        end
        return matched
    end

    function mod.register(_ctx)
        local declared_handlers, declared_order = proxy_ctrl.get_declarations()

        -- Deterministic sorted registration
        local sorted_keys = {}
        for _, key in ipairs(declared_order) do
            table.insert(sorted_keys, key)
        end
        table.sort(sorted_keys, function(a, b)
            return declared_handlers[a].command_id < declared_handlers[b].command_id
        end)

        for _, key in ipairs(sorted_keys) do
            local decl = declared_handlers[key]
            local cmd_id = decl.command_id
            local raw_handler = decl.handler

            local function wrapped_handler(request)
                local prev_scope = current_scope
                current_scope = {
                    kind = "command",
                    package_id = package_id,
                    command_id = cmd_id,
                }

                local args_list = M.decode_authoring_args(request.args)
                local ok, res = pcall(raw_handler, table.unpack(args_list, 1, #args_list))

                current_scope = prev_scope

                if not ok then
                    if is_fail_result(res) then
                        return {
                            ok = false,
                            error = res.error,
                        }
                    end
                    error(res, 0)
                end

                -- SAS-04: Implicit Command Success & Return Normalization
                if res == nil then
                    return { ok = true }
                elseif is_fail_result(res) then
                    return {
                        ok = false,
                        error = res.error,
                    }
                elseif type(res) == "table" and res.ok == true and res.value ~= nil and res.__gv2_fail == nil then
                    return res
                else
                    return {
                        ok = true,
                        value = res,
                    }
                end
            end

            if game and game.commands and game.commands.handlers and game.commands.handlers.register then
                game.commands.handlers.register(cmd_id, wrapped_handler, {
                    package_id = package_id,
                    replaceable = (decl.replaceable == true),
                })
            end
        end

        -- Register declared validators in declaration order (CVA-08)
        for _, decl in ipairs(declared_validators_order) do
            local validator_id = decl.validator_id
            local target_cmd_id = decl.target_command_id
            local raw_validator_fn = decl.fn
            local declaring_pkg = decl.declaring_package

            local function wrapped_validator(runtime_ctx)
                if runtime_ctx.command_id ~= target_cmd_id then
                    return true
                end

                local prev_scope = current_scope
                current_scope = {
                    kind = "validator",
                    package_id = declaring_pkg,
                    command_id = target_cmd_id,
                    validator_id = validator_id,
                }

                local args_list = M.decode_authoring_args(runtime_ctx.payload)
                local ok, res = pcall(raw_validator_fn, table.unpack(args_list, 1, #args_list))

                current_scope = prev_scope

                if not ok then
                    if is_fail_result(res) then
                        return false, res.error
                    end
                    error(res, 0)
                end

                return true
            end

            if game and game.commands and game.commands.validators and game.commands.validators.register then
                game.commands.validators.register(validator_id, {
                    validator_id = validator_id,
                    target_command_id = target_cmd_id,
                    declaring_package = declaring_pkg,
                    validator_name = decl.name,
                    validate = wrapped_validator,
                })
            end
        end

        -- Register accumulated event subscribers
        for i, sub in ipairs(subscribers) do
            local clean_name = sub.event_name:gsub("[^a-zA-Z0-9_]", "_"):lower()
            local sub_id = package_id .. ":subscriber.authoring." .. clean_name .. ".s" .. tostring(i)
            local raw_handler = sub.handler
            local function wrapped_subscriber(env)
                local prev_scope = current_scope
                current_scope = {
                    kind = "event",
                    package_id = package_id,
                    event_id = sub.event_id,
                }

                local raw_payload = env.payload or {}
                local rehydrated = tagged_ref.rehydrate_arg(raw_payload)
                local ok, res = pcall(raw_handler, rehydrated, env)

                current_scope = prev_scope

                if not ok then
                    error(res, 0)
                end
            end

            if game and game.events and game.events.subscribers and game.events.subscribers.register then
                game.events.subscribers.register(sub_id, sub.event_id, wrapped_subscriber)
            end
        end

        -- Register declared services in declaration order
        for _, decl in ipairs(declared_services_order) do
            if game and game.services and game.services.register and not (game.services.exists and game.services.exists(decl.service_id)) then
                game.services.register(decl.service_id, decl.impl)
            end
        end

        -- Register declared semantic actions
        if game and game.actions and game.actions.bind then
            for act_id, binding in pairs(declared_actions) do
                game.actions.bind(act_id, binding)
            end
        end

        is_module_frozen = true
        proxy_ctrl.freeze()
    end

    function mod.freeze()
        is_module_frozen = true
        proxy_ctrl.freeze()
    end

    mod.validate = mod.validate

    return mod
end

function M.verify_validator_targets()
    for _, decl in ipairs(all_declared_validators) do
        local target_id = decl.target_command_id
        local exists = false
        if game and game.commands and game.commands.handlers and game.commands.handlers.exists then
            exists = game.commands.handlers.exists(target_id)
        end
        if not exists then
            error("AuthoringValidatorTargetMissing: target command '" .. tostring(target_id)
                .. "' for validator '" .. tostring(decl.validator_id)
                .. "' in package '" .. tostring(decl.declaring_package) .. "' is not registered", 2)
        end
    end
end

function M.verify_service_targets()
    for _, ref in ipairs(all_referenced_services) do
        local req_id = ref.requested_id
        local exists = false
        if game and game.services and game.services.exists then
            exists = game.services.exists(req_id)
        end
        if not exists then
            error("ServiceTargetMissing: service '" .. tostring(req_id)
                .. "' requested by package '" .. tostring(ref.declaring_package) .. "' is not registered", 2)
        end
    end
end

function M.freeze()
    M.verify_validator_targets()
    M.verify_service_targets()
    mode_store.freeze()
    is_frozen = true
end

function M.is_frozen()
    return is_frozen
end

function M.snapshot_test_state()
    return test_state_registry.snapshot_all()
end

function M.restore_test_state(snap)
    test_state_registry.restore_all(snap)
end

function M.reset_test_state()
    test_state_registry.reset_all()
end

function M.create_authoring_environment(package_id, opt_module_id)
    local mod = M.gameplay(package_id, opt_module_id)
    local env_values = {
        commands = mod.commands,
        actions = mod.actions,
        services = mod.services,
        validate = mod.validate,
        player = mod.player,
        world = mod.world,
        def = mod.def,
        location = mod.location,
        actor = mod.actor,
        actors = mod.actors,
        fail = mod.fail,
        guard_validator_side_effect = M.guard_validator_side_effect,
        emit = mod.emit,
        on = mod.on,
        text = mod.text,
        button = mod.button,
        action = mod.action,
        show_screen = mod.show_screen,
        show_route = mod.show_route,
        show_overlay = mod.show_overlay,
        close_overlay = mod.close_overlay,
        show_modal = mod.show_modal,
        close_modal = mod.close_modal,
        tab = mod.tab,
        tab_container = mod.tab_container,
        tabs = mod.tabs,
        Actor = mod.Actor,
        Location = mod.Location,
        Quest = mod.Quest,
        Item = mod.Item,
        field = mod.field,
        instances = mod.instances,
        modes = mod.modes,
    }

    local env = {}
    setmetatable(env, {
        __index = function(_, key)
            if env_values[key] ~= nil then
                return env_values[key]
            end
            if type(key) == "string" and key:match("^[A-Z][a-zA-Z0-9_]*$") then
                if mod[key] then
                    return mod[key]
                end
                local proxy = M.create_entity_prototype_proxy(key, package_id, opt_module_id)
                mod[key] = proxy
                return proxy
            end
            return _G[key]
        end,
        __newindex = function(_, key, _)
            error("AuthoringGlobalWriteDisallowed: cannot assign global variable '" .. tostring(key) .. "' in authoring module", 2)
        end,
    })

    return mod, env
end

return M
