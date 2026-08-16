-- PKG-21: package composition and script_set_hash in save envelope
-- (core:module.runtime.save and core:module.runtime.load).

local canonical_codec = require("core:module.runtime.canonical_codec")
local save = require("core:module.runtime.save")
local load = require("core:module.runtime.load")

return {
    envelope_records_packages_and_script_set_hash = function()
        local state = { meta = { schema_version = 1 }, actors = {} }
        local custom_packages = { { package_id = "core" }, { package_id = "test_mod" } }
        local custom_hash = "111122223333444455556666777788889999aaaabbbbccccddddeeeeffff0000"

        local envelope = save.build_envelope(state, 1, "repo_hash", custom_hash, custom_packages)
        assert(envelope.script_set_hash == custom_hash, "script_set_hash must be recorded in envelope")
        assert(type(envelope.packages) == "table", "packages must be a table")
        assert(#envelope.packages == 2, "packages table must contain 2 entries")
        assert(envelope.packages[1].package_id == "core", "package 1 must be core")
        assert(envelope.packages[2].package_id == "test_mod", "package 2 must be test_mod")
    end,

    envelope_defaults_to_game_runtime_properties = function()
        local state = { meta = { schema_version = 1 } }
        local envelope = save.build_envelope(state, 1, "repo_hash")
        if game and game.runtime and game.runtime.script_set_hash then
            assert(envelope.script_set_hash == game.runtime.script_set_hash,
                "script_set_hash should default to game.runtime.script_set_hash")
        end
        if game and game.runtime and game.runtime.packages then
            assert(#envelope.packages == #game.runtime.packages,
                "packages should default to game.runtime.packages")
        end
    end,

    preflight_decodes_packages_and_script_set_hash = function()
        local state = { meta = { schema_version = 1 } }
        local custom_packages = { { package_id = "core" } }
        local custom_hash = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"
        local envelope = save.build_envelope(state, 1, "repo_hash", custom_hash, custom_packages)
        local container = save.serialize_envelope(envelope)

        local preflighted, err = load.preflight(container)
        assert(preflighted ~= nil, "preflight must succeed, got err=" .. tostring(err))
        assert(preflighted.script_set_hash == custom_hash, "preflight must decode script_set_hash")
        assert(type(preflighted.packages) == "table", "preflight must decode packages")
        assert(preflighted.packages[1].package_id == "core", "preflight packages[1] must be core")
    end,

    decode_and_prepare_rejects_missing_package = function()
        local state = {
            meta = { schema_version = 1, save_version = 1, save_id = "test_slot" },
            actors = {},
            item_instances = {},
            world = {},
            quests = {},
            mods = {}
        }
        local missing_packages = { { package_id = "core" }, { package_id = "nonexistent_mod_xyz" } }
        local envelope = save.build_envelope(state, 1, "repo_hash", "some_hash", missing_packages)
        local container = save.serialize_envelope(envelope)

        local decoded, err = load.decode_and_prepare(container)
        assert(decoded == nil, "decode_and_prepare must fail when a required package is missing")
        assert(string.find(err or "", "SaveMissingPackage") ~= nil,
            "error must be SaveMissingPackage, got: " .. tostring(err))
        assert(string.find(err or "", "nonexistent_mod_xyz") ~= nil,
            "error must name the missing package, got: " .. tostring(err))
    end,

    decode_and_prepare_accepts_matching_packages = function()
        local state = {
            meta = { schema_version = 1, save_version = 1, save_id = "test_slot" },
            actors = {},
            item_instances = {},
            world = {},
            quests = {},
            mods = {}
        }
        local envelope = save.build_envelope(state, 1, "repo_hash")
        local container = save.serialize_envelope(envelope)

        local decoded, err = load.decode_and_prepare(container)
        assert(decoded ~= nil, "decode_and_prepare must succeed with active session packages, got err=" .. tostring(err))
    end,
}
