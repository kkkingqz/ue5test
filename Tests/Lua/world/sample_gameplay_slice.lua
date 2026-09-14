-- CFC-12: End-to-End Gameplay Slice Specification (TextSystem tier)
-- Verifies the full pipeline with no native C++ gameplay logic:
-- package-owned start -> bound command -> service mutation -> post-commit event ->
-- desired presentation -> typed save -> post-save mutation -> typed load ->
-- reconstructed UI -> continuation of deterministic PRNG stream and command.

local command_dispatcher = require("core:module.runtime.command_dispatcher")
local event_bus = require("core:module.runtime.event_bus")
local mutation_window = require("core:module.runtime.mutation_window")
local save = require("core:module.runtime.save")
local load = require("core:module.runtime.load")
local state_hasher = require("core:module.runtime.state_hasher")
local canonical_codec = require("core:module.runtime.canonical_codec")
local location_presenter = require("textsystem:module.presentation.location_presenter")

-- Independent golden constants (oracle) computed from the specification:
-- Seed "0123456789abcdef" with stream "sample:random_stream.scout"
local EXPECTED_PRNG_DRAW_1 = 0xe5a8dfaa
local EXPECTED_PRNG_DRAW_2 = 0x9fb4c2a9

return {
    gameplay_slice_full_lifecycle_and_stream_continuation = function()
        event_bus.with_isolated_subscribers(function()
            event_bus.clear_published_events()
            game.runtime.phase = "idle"
            game.runtime.seed_hex = "0123456789abcdef"
            mutation_window.execute_in_window(function()
                if game.state and game.state.meta then
                    game.state.meta.seed_hex = "0123456789abcdef"
                end
            end)

            local recorded_events = {}
            game.events.subscribers.register(
                "core:subscriber.test.scout_tracker",
                "sample:event.scout_completed",
                function(env)
                    table.insert(recorded_events, {
                        scout_count = env.payload.scout_count,
                        roll = env.payload.roll,
                        reward = env.payload.reward,
                    })
                end
            )

            local dispatcher = command_dispatcher.new()

            -- 1. Package-owned start: sample:command.start_game
            dispatcher.dispatch({
                command_id = "sample:command.start_game",
                args = {},
                sequence = 101,
            })

            local player = game.instances.actors.player()
            assert(player ~= nil, "player actor must be created by start_game")
            assert(player.current_location_id == "sample:location.hub" or (player.current_location and player.current_location.id == "sample:location.hub"),
                "player initial location must be sample:location.hub")
            assert(player.gold == 100, "player initial gold must be 100")
            assert(player.stamina == 50, "player initial stamina must be 50")
            assert(player.scout_count == 0, "scout_count must be initialized to 0")

            -- Presentation after start
            local doc1 = location_presenter.build_and_publish_screen()
            assert(doc1 ~= nil, "location presenter must produce a screen document")
            assert(doc1.fields ~= nil and doc1.fields.top_bar ~= nil, "top_bar field must exist")
            assert(doc1.fields.commands ~= nil and doc1.fields.commands.value ~= nil, "commands field must exist")

            -- Verify scout button exists in commands items
            local has_scout_btn = false
            for _, item in ipairs(doc1.fields.commands.value.items or {}) do
                if item.key == "scout_hub" then
                    has_scout_btn = true
                    assert(item.binding ~= nil and item.binding.command_id == "sample:command.scout",
                        "scout_hub button must bind to sample:command.scout")
                end
            end
            assert(has_scout_btn, "scout_hub button must be present in commands field")

            -- 2. Execute bound UI command (sample:command.scout)
            event_bus.clear_published_events()
            dispatcher.dispatch({
                command_id = "sample:command.scout",
                args = {},
                sequence = 102,
            })

            -- Verify service mutation
            assert(player.gold == 125, "player gold must increase by 25 after scout")
            assert(player.scout_count == 1, "scout_count must be 1")

            -- Verify post-commit event fact
            assert(#recorded_events == 1, "exactly one scout event must be recorded")
            assert(recorded_events[1].scout_count == 1, "event scout_count must be 1")
            assert(recorded_events[1].roll == EXPECTED_PRNG_DRAW_1,
                string.format("PRNG draw 1 must match oracle 0x%08x, got 0x%08x", EXPECTED_PRNG_DRAW_1, recorded_events[1].roll))
            assert(recorded_events[1].reward == 25, "event reward must be 25")

            -- Desired presentation update
            local doc2 = location_presenter.build_and_publish_screen()
            assert(doc2 ~= nil, "screen must resolve after scout")
            assert(doc2.fields.top_bar.value.primary_resource.args.gold == 125,
                "top_bar primary_resource must display 125 gold")

            -- 3. Typed save request
            local envelope = save.build_envelope(game.state, 1, "test_repo_hash")
            local container_bytes = save.serialize_envelope(envelope)
            assert(type(container_bytes) == "string" and #container_bytes > 0, "save container must be non-empty bytes")

            -- Verify saved integrity hash matches state_hasher
            local state_hash_before_post_save = state_hasher.hash_state(game.state)
            assert(envelope.integrity == state_hash_before_post_save, "envelope integrity must match state hash")

            -- 4. Post-save mutation in Session A (travel to east)
            dispatcher.dispatch({
                command_id = "sample:command.travel",
                args = { "sample:location.east" },
                sequence = 103,
            })
            assert(game.instances.world().current_location_id == "sample:location.east",
                "player must have travelled to east")
            local state_hash_post_travel = state_hasher.hash_state(game.state)
            assert(state_hash_post_travel ~= state_hash_before_post_save,
                "state hash must change after post-save travel")

            -- 5. Typed load of saved container into session
            local decoded_envelope = canonical_codec.deserialize(container_bytes)
            local restored_tree = canonical_codec.deserialize(decoded_envelope.payload)
            game.state = mutation_window.guard_state(restored_tree)

            assert(game.instances.world().current_location_id == "sample:location.hub",
                "restored location must be back at hub")
            local restored_player = game.instances.actors.player()
            assert(restored_player ~= nil, "restored player must exist")
            assert(restored_player.gold == 125, "restored player gold must be 125")
            assert(restored_player.scout_count == 1, "restored scout_count must be 1")

            local restored_hash = state_hasher.hash_state(game.state)
            assert(restored_hash == state_hash_before_post_save,
                "restored state hash must strictly match pre-travel saved hash")

            -- Reconstructed UI
            local doc3 = location_presenter.build_and_publish_screen()
            assert(doc3 ~= nil, "reconstructed screen must resolve")
            assert(doc3.fields.top_bar.value.primary_resource.args.gold == 125,
                "reconstructed top_bar must show 125 gold")

            -- 6. Continuation: execute next bound command (sample:command.scout)
            recorded_events = {}
            dispatcher.dispatch({
                command_id = "sample:command.scout",
                args = {},
                sequence = 104,
            })

            assert(restored_player.gold == 150, "player gold must become 150 after second scout")
            assert(restored_player.scout_count == 2, "scout_count must become 2")
            assert(#recorded_events == 1, "second scout event must be recorded")
            assert(recorded_events[1].scout_count == 2, "second event scout_count must be 2")
            assert(recorded_events[1].roll == EXPECTED_PRNG_DRAW_2,
                string.format("continuation PRNG draw 2 must match oracle 0x%08x, got 0x%08x", EXPECTED_PRNG_DRAW_2, recorded_events[1].roll))

            local doc4 = location_presenter.build_and_publish_screen()
            assert(doc4.fields.top_bar.value.primary_resource.args.gold == 150,
                "top_bar must update to 150 gold after continuation command")
        end)
    end,
}
