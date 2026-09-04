-- DCA-11: inventory_tabs fixture presenter spec.
-- Verifies the Lua-side envelope assembly for the four-level chain
-- (screen -> tabs -> nested screen -> declared block -> item collection).
-- The composite's real observability -- values reaching the icon widgets,
-- the deepest-level Prepare failure leaving no trace, and the DUC-11
-- composition-cycle guard rejecting the cyclic fixture -- is proven
-- separately against the real WBP assets in
-- Source/GV2/Private/Tests/GV2Dca11InventoryTabsTests.cpp.

local dca11_fixture = require("textsystem:module.presentation.dca11_fixture")

return {
    inventory_tabs_no_fixture_requested_returns_nil = function()
        assert(dca11_fixture.build_requested_screen() == nil,
            "no debug command issued yet -- fixture must stay unpublished")
    end,

    inventory_tabs_baseline_publishes_two_tabs_with_item_collections = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_show",
            args = {},
            sequence = 9201,
        })

        local req = dca11_fixture.build_requested_screen()
        assert(req ~= nil, "screen request must be generated after dca11_show")
        assert(req.screen_id == "textsystem:screen.dca11_inventory_fixture")

        local tabs_field = req.fields.inventory_tabs
        assert(tabs_field ~= nil, "inventory_tabs field must exist")
        assert(tabs_field.value.default_tab_key == "weapons")

        local tabs = tabs_field.value.tabs
        assert(#tabs == 2, "expected exactly two tabs")
        assert(tabs[1].key == "weapons")
        assert(tabs[1].screen_id == "textsystem:screen.dca11_inventory_weapons")
        assert(tabs[2].key == "consumables")
        assert(tabs[2].screen_id == "textsystem:screen.dca11_inventory_consumables")

        local weapons_items = tabs[1].fields[1].value.items
        assert(#weapons_items == 2)
        assert(weapons_items[1].key == "sword")
        assert(weapons_items[1].resource_id == "textsystem:resource.ui.missing_icon")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_clear",
            args = {},
            sequence = 9202,
        })
    end,

    inventory_tabs_update_changes_item_collections = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_update",
            args = {},
            sequence = 9203,
        })

        local req = dca11_fixture.build_requested_screen()
        local weapons_items = req.fields.inventory_tabs.value.tabs[1].fields[1].value.items
        assert(#weapons_items == 3, "update fixture adds a third weapon item")
        local consumable_items = req.fields.inventory_tabs.value.tabs[2].fields[1].value.items
        assert(#consumable_items == 1, "update fixture removes one consumable item")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_clear",
            args = {},
            sequence = 9204,
        })
    end,

    inventory_tabs_item_prepare_failure_fixture_carries_unresolvable_resource = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_item_prepare_failure",
            args = {},
            sequence = 9205,
        })

        local req = dca11_fixture.build_requested_screen()
        local weapons_items = req.fields.inventory_tabs.value.tabs[1].fields[1].value.items
        assert(weapons_items[2].key == "cursed_blade")
        assert(weapons_items[2].resource_id == "textsystem:resource.ui.does_not_exist",
            "this fixture only asserts the Lua-side envelope; the real Prepare-time " ..
            "rejection is proven against the actual widget tree in the C++ test")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_clear",
            args = {},
            sequence = 9206,
        })
    end,

    inventory_tabs_cycle_fixture_points_weapons_tab_at_the_root_screen = function()
        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_cycle",
            args = {},
            sequence = 9207,
        })

        local req = dca11_fixture.build_requested_screen()
        assert(req.fields.inventory_tabs.value.tabs[1].screen_id == "textsystem:screen.dca11_inventory_fixture",
            "the cycle fixture deliberately loops the weapons tab back to the root screen id")

        game.runtime.dispatch_command({
            command_id = "textsystem:command.debug.dca11_clear",
            args = {},
            sequence = 9208,
        })

        assert(dca11_fixture.build_requested_screen() == nil,
            "dca11_clear must unpublish the fixture")
    end,
}
