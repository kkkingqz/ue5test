-- Save Path (SAV-08/09/10/11, plan SaveAndLoad, ADR-0021).
-- Builds the opaque save container and writes it through the host's
-- slot-scoped storage primitive (game.save_slots.write, SAV-05/06). C++
-- never sees the state tree or the container's shape; everything here is
-- pure Lua on top of the one host capability the storage primitive grants.

local canonical_codec = require("core:module.runtime.canonical_codec")
local state_hasher = require("core:module.runtime.state_hasher")
local migrate = require("core:module.runtime.migrate")

local M = {
    id = "core:module.runtime.save",
}

-- SAV-08: version of the container *envelope* shape (which fields it
-- carries). Independent of canonical_codec.M.VERSION, which versions the
-- byte encoding used to serialize the envelope and the payload it carries.
M.SAVE_VERSION = 1

-- Builds the envelope table. save_id and repository_content_hash are
-- caller-supplied: this module owns the envelope's shape and the payload's
-- serialization, not identity or provenance policy, so the same
-- (state, save_id, repository_content_hash) always produces the exact same
-- envelope — no hidden module-local counter to make repeated saves diverge.
function M.build_envelope(state, save_id, repository_content_hash)
    if type(state) ~= "table" then
        error("SaveEnvelopeError: state must be a table", 2)
    end
    local payload = canonical_codec.serialize(state)
    -- SAV-18: the live game.state is always at this build's current
    -- section versions — nothing keeps a session running at a stale
    -- version, migration only ever happens once, on load, before state is
    -- ever assigned (SAV-17). So the envelope's section_versions is simply
    -- a fresh copy of what this build understands, not read from state.
    local section_versions = {}
    for section_id, version in pairs(migrate.CURRENT_SECTION_VERSIONS) do
        section_versions[section_id] = version
    end
    return {
        format_version = M.SAVE_VERSION,
        codec_version = canonical_codec.VERSION,
        save_version = M.SAVE_VERSION,
        save_id = save_id,
        -- Provenance only (ADR-0021 SAV-08): never a condition for loading.
        repository_content_hash = repository_content_hash or "",
        section_versions = section_versions,
        -- Computed over payload; equals state_hasher.hash_state(state)
        -- because hash_state does exactly sha256(canonical_codec.serialize(state)).
        integrity = state_hasher.sha256(payload),
        payload = payload,
    }
end

-- SAV-08: the envelope is serialized by the same codec as the payload it
-- carries, not a second format.
function M.serialize_envelope(envelope)
    return canonical_codec.serialize(envelope)
end

-- SAV-09: true only at a safe point to save — runtime phase must be idle
-- (this alone excludes ExecutingCommand, PumpingEvents, and a failed
-- session), and both the command and event queues must be empty. A save
-- attempted reentrantly from inside a command or event handler observes a
-- non-idle phase and is rejected here, before any write is attempted.
function M.is_safe_point()
    if not game or not game.runtime or game.runtime.phase ~= "idle" then
        return false
    end
    if game.commands and game.commands.get_queue_length and game.commands.get_queue_length() > 0 then
        return false
    end
    if game.events and game.events.get_queue_length and game.events.get_queue_length() > 0 then
        return false
    end
    return true
end

-- SAV-10/SAV-11: serializes game.state whole and writes the container to
-- slot_id through game.save_slots.write in one call. Never mutates
-- game.state. On any failure (not a safe point, no storage available, or
-- the storage primitive itself failing) returns false plus a typed error
-- code and performs no write; save_id is caller-supplied and this function
-- never advances or records it, so a failed call trivially never consumes
-- it — the same save_id can be retried unchanged. A successful write
-- relies entirely on the storage primitive's own atomicity (SAV-06): a
-- failure never touches the slot's previously-published content.
function M.save(slot_id, save_id, repository_content_hash)
    if not M.is_safe_point() then
        return false, "SaveNotAtSafePoint"
    end
    if not game or not game.save_slots or not game.save_slots.write then
        return false, "SaveSlotStorageUnavailable"
    end

    local envelope = M.build_envelope(game.state, save_id, repository_content_hash)
    local container = M.serialize_envelope(envelope)

    local ok, err = game.save_slots.write(slot_id, container)
    if not ok then
        return false, "SaveWriteFailed:" .. tostring(err)
    end
    return true
end

return M
