-- UIF-21: RH Layered Presentation and Purchase Confirmation Modal Spec
-- Verifies that RH gameplay interacts with layered UI presentation:
--   1. Location screen published as route.
--   2. Requesting purchase opens confirmation modal on the modal stack.
--   3. Canceling purchase closes modal without state mutation.
--   4. Confirming purchase transfers item, deducts gold, and closes modal.

local mutation_window = require("core:module.runtime.mutation_window")
local presentation_authoring = require("core:module.authoring.presentation")
local location_presenter = require("textsystem:module.presentation.location_presenter")

local function ensure_game_state(initial_stamina, initial_gold)
    local player = game.instances.actors.player()
    local merchant = nil
    if not player then
        game.runtime.dispatch_command({
            command_id = "rh:command.start_game",
            args = {},
            sequence = 900,
        })
        player = game.instances.actors.player()
    end
    local npcs = game.instances.actors.find_by_discriminator("npc")
    if #npcs > 0 then
        merchant = npcs[1]
    else
        merchant = game.instances.actors.create("rh:actor.npc.merchant", {
            current_location = "rh:location.city.market",
            gold = 100,
            stamina = 50,
        })
    end
    if initial_stamina ~= nil then
        player.stamina = initial_stamina
    end
    if initial_gold ~= nil then
        player.gold = initial_gold
    end
    return player, merchant
end

return {
    rh_purchase_confirmation_modal_flow = function()
        mutation_window.execute_in_window(function()
            local player, merchant = ensure_game_state(20, 50)
            player.current_location_id = "rh:location.city.market"

            if not merchant:has_item("rh:item.weapon.iron_sword") then
                merchant:add_item("rh:item.weapon.iron_sword")
            end

            local initial_gold = player:get_gold()

            -- 1. Publish route (market location screen)
            location_presenter.build_and_publish_screen()
            local state = presentation_authoring.get_active_document_state()
            assert(state ~= nil, "active document state must exist")
            assert(state.route ~= nil, "route must exist")
            assert(state.route.screen_id == "rh:screen.location.market", "route screen must be market")
            assert(#state.modals == 0, "modal stack must initially be empty")

            -- 2. Dispatch request_buy -> opens modal
            game.runtime.dispatch_command({
                command_id = "rh:command.request_buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 910,
            })

            local modal_state = presentation_authoring.get_active_document_state()
            assert(#modal_state.modals == 1, "modal stack must contain 1 modal after request_buy")
            local modal = modal_state.modals[1]
            assert(modal.instance_key == "confirm_purchase", "modal instance_key must be confirm_purchase")
            assert(modal.fields.modal ~= nil, "modal field must exist")
            assert(modal.fields.modal.schema_id == "core:schema.ui_field.modal.v1", "schema must be modal.v1")
            assert(modal.fields.modal.value.title.text_id == "rh:text.shop.confirm.title", "title must match")
            assert(modal.fields.modal.value.content.text_id == "rh:text.shop.confirm.sword", "content must match sword")
            assert(#modal.fields.modal.value.buttons == 2, "modal must have 2 buttons")

            -- 3. Dispatch cancel_buy -> closes modal, state untouched
            game.runtime.dispatch_command({
                command_id = "rh:command.cancel_buy",
                args = {},
                sequence = 911,
            })

            local canceled_state = presentation_authoring.get_active_document_state()
            assert(#canceled_state.modals == 0, "modal stack must be empty after cancel_buy")
            assert(player:get_gold() == initial_gold, "gold must remain unchanged after cancel")
            assert(player:has_item("rh:item.weapon.iron_sword") == false, "player must not own sword after cancel")

            -- 4. Dispatch request_buy again, then confirm_buy -> purchase completes and modal closes
            game.runtime.dispatch_command({
                command_id = "rh:command.request_buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 912,
            })
            assert(#presentation_authoring.get_active_document_state().modals == 1, "modal must be reopened")

            game.runtime.dispatch_command({
                command_id = "rh:command.confirm_buy",
                args = { "rh:item.weapon.iron_sword" },
                sequence = 913,
            })

            local confirmed_state = presentation_authoring.get_active_document_state()
            assert(#confirmed_state.modals == 0, "modal stack must be empty after confirm_buy")
            assert(player:get_gold() == initial_gold - 10, "gold must be deducted by 10")
            assert(player:has_item("rh:item.weapon.iron_sword") == true, "player must now own sword")
            assert(merchant:has_item("rh:item.weapon.iron_sword") == false, "merchant must no longer own sword")
        end)
    end,
}
