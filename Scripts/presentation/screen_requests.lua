local stable_id = require("core:module.runtime.stable_id")

local M = {}
local pending_document = nil
local last_revisions_by_instance = {}

function M.create_instance(screen_id, fields, instance_key, layer)
    assert(stable_id.is_kind(screen_id, "screen"), "screen_id must be canonical")
    assert(type(fields) == "table", "fields must be an object")
    for field_id, field in pairs(fields) do
        assert(type(field_id) == "string" and field_id:match("^[a-z][a-z0-9_]*$"),
            "field_id must be canonical")
        assert(type(field) == "table", "field must be a table")
        assert(stable_id.is_kind(field.schema_id, "schema"), "field.schema_id must be canonical")
        assert(field.value ~= nil, "field.value is required")
    end

    return {
        layer = layer or "location_content",
        instance_key = instance_key or "main",
        screen_id = screen_id,
        fields = fields,
    }
end

function M.create(screen_id, fields)
    return M.create_instance(screen_id, fields, "main", "location_content")
end

local doc_mt = {
    __index = function(t, k)
        if k == "screen_id" then
            return t.route and t.route.screen_id
        elseif k == "fields" then
            return t.route and t.route.fields
        end
        return nil
    end
}

function M.create_document(ui_instance_id, revision, route, overlays, modals)
    ui_instance_id = ui_instance_id or "ui@default"
    revision = revision or 1

    local last_rev = last_revisions_by_instance[ui_instance_id]
    if last_rev and revision < last_rev then
        error("NonMonotonicRevision: revision " .. tostring(revision) .. " is less than previous " .. tostring(last_rev), 2)
    end
    last_revisions_by_instance[ui_instance_id] = revision

    local doc = {
        ui_instance_id = ui_instance_id,
        revision = revision,
        route = route,
        overlays = overlays or {},
        modals = modals or {},
    }
    setmetatable(doc, doc_mt)
    return doc
end

function M.publish(doc_or_screen)
    local ok_ctx, ctx_mod = pcall(require, "core:module.authoring.context")
    if ok_ctx and ctx_mod and ctx_mod.guard_validator_side_effect then
        ctx_mod.guard_validator_side_effect("show_screen")
    end
    assert(type(doc_or_screen) == "table", "document or screen request must be a table")

    if doc_or_screen.screen_id and doc_or_screen.fields then
        -- Normalize single screen request into a complete document envelope
        pending_document = {
            ui_instance_id = "ui@default",
            revision = (last_revisions_by_instance["ui@default"] or 0) + 1,
            route = doc_or_screen,
            overlays = {},
            modals = {},
        }
        setmetatable(pending_document, doc_mt)
        last_revisions_by_instance["ui@default"] = pending_document.revision
    else
        assert(type(doc_or_screen.ui_instance_id) == "string", "ui_instance_id is required")
        assert(type(doc_or_screen.revision) == "number", "revision is required")
        pending_document = doc_or_screen
        if getmetatable(pending_document) == nil then
            setmetatable(pending_document, doc_mt)
        end
    end
end

function M.take_pending()
    local doc = pending_document
    pending_document = nil
    return doc
end

function M.get_current_revision(ui_instance_id)
    return last_revisions_by_instance[ui_instance_id or "ui@default"] or 0
end

function M.clear_for_test()
    pending_document = nil
    last_revisions_by_instance = {}
end

return M
