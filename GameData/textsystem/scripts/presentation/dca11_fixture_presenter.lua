-- DCA-11: inventory_tabs data-driven UI fixture.
-- The chain is assembled only from Screen Templates, the generic tab
-- container, generic declared composites, and existing UI-field schemas:
-- screen -> tabs -> nested screen -> declared block -> item collection.
-- Two failure-demonstration commands (dca11_item_prepare_failure,
-- dca11_cycle) exist only to exercise the generic Prepare-time rejection
-- paths (unresolvable resource inside a collection item; DUC-11's
-- composition-cycle guard) on this real content, not to add new behavior.

local authoring = require("core:module.authoring.context")

local M = authoring.gameplay("textsystem")
M.id = "textsystem:module.presentation.dca11_fixture"

local ROOT_TEMPLATE_ID = "textsystem:screen.dca11_inventory_fixture"
local WEAPONS_TEMPLATE_ID = "textsystem:screen.dca11_inventory_weapons"
local CONSUMABLES_TEMPLATE_ID = "textsystem:screen.dca11_inventory_consumables"
local CATEGORY_SCHEMA_ID = "textsystem:schema.ui_field.inventory_category.v1"
local CATEGORY_FIELD_ID = "inventory_category"
local ICON_RESOURCE_ID = "textsystem:resource.ui.missing_icon"

local requested_fixture = nil

local function category_field(items)
    return {
        field_id = CATEGORY_FIELD_ID,
        schema_id = CATEGORY_SCHEMA_ID,
        value = { items = items },
    }
end

local function item(key, resource_id)
    return { key = key, resource_id = resource_id or ICON_RESOURCE_ID }
end

local function publish_baseline(weapon_items, consumable_items, weapons_screen_id)
    return M.show_screen({
        template = ROOT_TEMPLATE_ID,
        instance_key = "dca11_inventory",
        fields = {
            inventory_tabs = M.tab_container({
                default_tab = "weapons",
                tabs = {
                    M.tab(
                        "weapons",
                        M.text("textsystem:text.dca11.weapons_tab"),
                        weapons_screen_id or WEAPONS_TEMPLATE_ID,
                        { category_field(weapon_items) }),
                    M.tab(
                        "consumables",
                        M.text("textsystem:text.dca11.consumables_tab"),
                        CONSUMABLES_TEMPLATE_ID,
                        { category_field(consumable_items) }),
                },
            }),
        },
    })
end

function M.register(_ctx)
    game.commands.handlers.register("textsystem:command.debug.dca11_show", function(_req)
        requested_fixture = { kind = "baseline" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.dca11_update", function(_req)
        requested_fixture = { kind = "update" }
        return true
    end)

    -- The unresolvable resource_id is used only by the deepest-level Prepare
    -- atomicity test: a collection item two levels below the tab whose
    -- resource cannot be resolved must reject the whole document, not just
    -- that item.
    game.commands.handlers.register("textsystem:command.debug.dca11_item_prepare_failure", function(_req)
        requested_fixture = { kind = "item_prepare_failure" }
        return true
    end)

    -- The weapons tab's screen_id is deliberately set to the root template's
    -- own id, used only by the composition-cycle test.
    game.commands.handlers.register("textsystem:command.debug.dca11_cycle", function(_req)
        requested_fixture = { kind = "cycle" }
        return true
    end)

    game.commands.handlers.register("textsystem:command.debug.dca11_clear", function(_req)
        requested_fixture = nil
        return true
    end)
end

function M.build_requested_screen()
    if requested_fixture == nil then
        return nil
    end

    if requested_fixture.kind == "baseline" then
        return publish_baseline(
            { item("sword"), item("axe") },
            { item("potion"), item("elixir") })
    elseif requested_fixture.kind == "update" then
        return publish_baseline(
            { item("sword"), item("axe"), item("bow") },
            { item("potion") })
    elseif requested_fixture.kind == "item_prepare_failure" then
        return publish_baseline(
            { item("sword"), item("cursed_blade", "textsystem:resource.ui.does_not_exist") },
            { item("potion"), item("elixir") })
    elseif requested_fixture.kind == "cycle" then
        return publish_baseline(
            { item("sword"), item("axe") },
            { item("potion"), item("elixir") },
            ROOT_TEMPLATE_ID)
    end

    return nil
end

return M
