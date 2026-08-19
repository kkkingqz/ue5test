-- Field Descriptors for Authoring Layer (ADR-0032, RAS-02)

local M = {
    id = "core:module.authoring.field",
}

local function make_descriptor(kind, default_opts, user_opts)
    local opts = user_opts or {}
    local descriptor = {
        __gv2_field_descriptor = true,
        kind = kind,
        storage = opts.storage or default_opts.storage or "runtime_state",
        write_policy = opts.write_policy or default_opts.write_policy or "plain",
        nullable = opts.nullable or false,
        required = opts.required ~= false,
        override = (opts.override == true),
    }
    if opts.default ~= nil then
        descriptor.default = opts.default
    end
    if opts.min ~= nil then
        descriptor.min = opts.min
    end
    if opts.max ~= nil then
        descriptor.max = opts.max
    end
    if opts.min_length ~= nil then
        descriptor.min_length = opts.min_length
    end
    if opts.max_length ~= nil then
        descriptor.max_length = opts.max_length
    end
    if opts.target_kind ~= nil then
        descriptor.target_kind = opts.target_kind
    end
    if opts.values ~= nil then
        descriptor.values = opts.values
    end
    if opts.operations ~= nil then
        descriptor.operations = opts.operations
    end
    return descriptor
end

function M.non_negative_integer(opts)
    local descriptor = make_descriptor("integer", { storage = "runtime_state", write_policy = "plain" }, opts)
    descriptor.min = (opts and opts.min) or 0
    return descriptor
end

function M.positive_integer(opts)
    local descriptor = make_descriptor("integer", { storage = "runtime_state", write_policy = "plain" }, opts)
    descriptor.min = (opts and opts.min) or 1
    return descriptor
end

function M.integer(opts)
    return make_descriptor("integer", { storage = "runtime_state", write_policy = "plain" }, opts)
end

function M.number(opts)
    return make_descriptor("number", { storage = "runtime_state", write_policy = "plain" }, opts)
end

function M.string(opts)
    return make_descriptor("string", { storage = "runtime_state", write_policy = "plain" }, opts)
end

function M.boolean(opts)
    return make_descriptor("boolean", { storage = "runtime_state", write_policy = "plain" }, opts)
end

function M.enum(values, opts)
    local descriptor = make_descriptor("enum", { storage = "runtime_state", write_policy = "plain" }, opts)
    descriptor.values = values
    return descriptor
end

function M.ref_definition(target_kind, opts)
    local descriptor = make_descriptor("ref_definition", { storage = "runtime_state", write_policy = "plain" }, opts)
    descriptor.target_kind = target_kind
    return descriptor
end

function M.ref_instance(target_kind, opts)
    local descriptor = make_descriptor("ref_instance", { storage = "runtime_state", write_policy = "plain" }, opts)
    descriptor.target_kind = target_kind
    return descriptor
end

return M
