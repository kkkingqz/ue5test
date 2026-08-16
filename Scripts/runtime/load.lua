-- Cold Start Load (SAV-13/14/15/16/18, plan SaveAndLoad, ADR-0021).
-- Decodes and validates a save container before any canonical state is
-- assigned: envelope preflight (SAV-13), then redirect resolution and
-- referential integrity for reference fields (SAV-15/16, using the
-- registry SAV-14 put in state_validator), then migration planning
-- (SAV-18: downgrade rejected here, before any Lua module lifecycle hook
-- runs). C++ calls M.decode_and_prepare exactly once (GV2RuntimeSession's
-- cold-start path) and either gets back a fully-prepared state tree or a
-- typed error — never a partial tree.

local canonical_codec = require("core:module.runtime.canonical_codec")
local state_hasher = require("core:module.runtime.state_hasher")
local save = require("core:module.runtime.save")
local state_validator = require("core:module.runtime.state_validator")
local migrate = require("core:module.runtime.migrate")

local M = {
    id = "core:module.runtime.load",
}

-- Defends resolve_definition_id against a redirect cycle; a real content
-- package never has a chain this long (gv2-content rename keeps chains
-- flat in practice), so hitting this is itself evidence of a cycle.
local MAX_REDIRECT_HOPS = 32

-- SAV-13: envelope-level checks, before the payload is even decoded.
-- Returns the envelope table on success, or nil plus one of:
--   "SaveContainerCorrupt"          - not a well-formed envelope at all
--   "SaveFormatVersionUnknown"      - format_version/codec_version newer
--                                      than this build understands
--   "SaveVersionDowngradeUnsupported" - save_version newer than this
--                                      build's save.SAVE_VERSION
--   "SaveIntegrityMismatch"         - integrity does not match payload
function M.preflight(container_bytes)
    if type(container_bytes) ~= "string" then
        return nil, "SaveContainerCorrupt"
    end
    local ok, decoded = pcall(canonical_codec.deserialize, container_bytes)
    if not ok or type(decoded) ~= "table" then
        return nil, "SaveContainerCorrupt"
    end
    local envelope = decoded
    if type(envelope.format_version) ~= "number"
        or type(envelope.codec_version) ~= "number"
        or type(envelope.save_version) ~= "number"
        or type(envelope.payload) ~= "string"
        or type(envelope.integrity) ~= "string"
        or type(envelope.section_versions) ~= "table"
        or (envelope.packages ~= nil and type(envelope.packages) ~= "table")
        or (envelope.script_set_hash ~= nil and type(envelope.script_set_hash) ~= "string")
    then
        return nil, "SaveContainerCorrupt"
    end

    if envelope.format_version > save.SAVE_VERSION or envelope.codec_version > canonical_codec.VERSION then
        return nil, "SaveFormatVersionUnknown"
    end
    if envelope.save_version > save.SAVE_VERSION then
        return nil, "SaveVersionDowngradeUnsupported"
    end
    if state_hasher.sha256(envelope.payload) ~= envelope.integrity then
        return nil, "SaveIntegrityMismatch"
    end

    return envelope, nil
end

-- SAV-15/16: resolves definition_id through any chain of redirects to its
-- final canonical target. repository_get defaults to game.repository.get
-- (production) but can be overridden — game.repository is a read-only
-- table by design (GV2HostServices/GV2RuntimeSession), so this is the only
-- way specs exercise chain-following without real repository content.
-- Returns (canonical_id, nil) if the chain terminates in a live
-- definition, or (nil, "retired"|"unknown") if it terminates in a
-- tombstone, an untraceable gap, or a cycle.
function M.resolve_definition_id(id, repository_get)
    repository_get = repository_get or (game and game.repository and game.repository.get)
    if type(repository_get) ~= "function" then
        return nil, "unknown"
    end

    local candidate = id
    for _ = 1, MAX_REDIRECT_HOPS do
        local value, err = repository_get(candidate)
        if value ~= nil then
            return candidate, nil
        end
        if type(err) == "table" and err.code == "tombstoned" then
            return nil, "retired"
        end
        if type(err) == "table" and err.code == "not_found" and type(err.canonical_id) == "string" then
            candidate = err.canonical_id
        else
            return nil, "unknown"
        end
    end
    return nil, "unknown"
end

local function reference_error(field_label, id, reason, path)
    local code = reason == "retired" and "SaveReferenceRetired" or "SaveReferenceUnknown"
    return code .. ": " .. field_label .. " '" .. tostring(id) .. "' at " .. path .. " does not resolve"
end

-- SAV-15/16: walks the decoded state tree in place, rewriting every
-- reference field (generic definition_id, plus every field declared in
-- state_validator.definition_reference_fields) to its final canonical
-- target. Fails fast on the first reference that does not resolve —
-- "Отсутствие ID — всегда ошибка" (README.md) applies uniformly to
-- retired and unknown alike, distinguished only by error code.
local function rewrite_references(val, path, repository_get)
    if type(val) ~= "table" then
        return true
    end
    if game and game.null and val == game.null then
        return true
    end

    if val.definition_id ~= nil and type(val.definition_id) == "string" then
        local canonical_id, reason = M.resolve_definition_id(val.definition_id, repository_get)
        if not canonical_id then
            return false, reference_error("definition_id", val.definition_id, reason, path)
        end
        val.definition_id = canonical_id
    end

    for field_name, _ in pairs(state_validator.definition_reference_fields) do
        local ref_value = val[field_name]
        if ref_value ~= nil and type(ref_value) == "string" then
            local canonical_id, reason = M.resolve_definition_id(ref_value, repository_get)
            if not canonical_id then
                return false, reference_error(field_name, ref_value, reason, path)
            end
            val[field_name] = canonical_id
        end
    end

    for k, v in pairs(val) do
        local ok, err = rewrite_references(v, path .. "." .. tostring(k), repository_get)
        if not ok then
            return false, err
        end
    end
    return true
end

M.rewrite_references = rewrite_references

-- SAV-13/15/16/18: the single entry point GV2RuntimeSession's cold-start
-- path calls. Returns (state_tree, nil) only when preflight, decode, every
-- reference in the tree resolves, and every section's saved version is not
-- newer than this build's; otherwise (nil, typed_error_code) and the
-- caller must not assign any state. On success, the list of sections that
-- still need a module's migrate_state hook to run is stashed on
-- game.runtime.pending_section_migrations for the "migrate_state" lifecycle
-- phase (SAV-18/19/20) that runs next, between this call and
-- "restore_instances".
function M.decode_and_prepare(container_bytes)
    local envelope, preflight_err = M.preflight(container_bytes)
    if not envelope then
        return nil, preflight_err
    end

    local ok, decoded = pcall(canonical_codec.deserialize, envelope.payload)
    if not ok or type(decoded) ~= "table" then
        return nil, "SaveContainerCorrupt"
    end

    if envelope.packages and type(envelope.packages) == "table" and game and game.runtime and game.runtime.packages then
        local loaded_pkgs = {}
        for _, pkg in ipairs(game.runtime.packages) do
            local pid = type(pkg) == "table" and pkg.package_id or pkg
            if pid then
                loaded_pkgs[pid] = true
            end
        end
        for _, pkg in ipairs(envelope.packages) do
            local pid = type(pkg) == "table" and pkg.package_id or pkg
            if pid and not loaded_pkgs[pid] then
                return nil, "SaveMissingPackage: " .. tostring(pid)
            end
        end
    end

    local rewrite_ok, rewrite_err = rewrite_references(decoded, "state", nil)
    if not rewrite_ok then
        return nil, rewrite_err
    end

    local pending, plan_err = migrate.plan_migrations(envelope.section_versions)
    if not pending then
        return nil, plan_err
    end
    if game and game.runtime then
        game.runtime.pending_section_migrations = pending
    end

    return decoded, nil
end

return M
