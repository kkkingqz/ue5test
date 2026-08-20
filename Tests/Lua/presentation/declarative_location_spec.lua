-- TSL-13: Declarative Screens and Location Presenter Verification
-- Verifies that location presenter constructs screens entirely from content definitions
-- with zero custom Lua presentation code.

local location_presenter = require("textsystem:module.presentation.location_presenter")
local screens = require("core:module.presentation.screen_requests")

local function find_button(buttons_field, key)
    if not buttons_field or not buttons_field.value or not buttons_field.value.items then
        return nil
    end
    for _, item in ipairs(buttons_field.value.items) do
        if item.key == key then
            return item
        end
    end
    return nil
end

return {
    declarative_market_screen_matches_schema = function()
        local req = location_presenter.build_screen_request("rh:location.city.market")
        assert(req ~= nil, "screen request must be generated for market")
        assert(req.screen_id == "rh:screen.location.market")

        -- Text is rich_text
        assert(req.fields.description ~= nil)
        assert(req.fields.description.schema_id == "core:schema.ui_field.rich_text.v3")
        assert(req.fields.description.value.text.text_id == "rh:text.screen.market.description")

        -- Buttons are button_list
        assert(req.fields.buttons ~= nil)
        assert(req.fields.buttons.schema_id == "core:schema.ui_field.button_list.v2")
        assert(#req.fields.buttons.value.items == 3)

        -- Action 1: buy_sword
        local b_sword = find_button(req.fields.buttons, "buy_sword")
        assert(b_sword ~= nil)
        assert(b_sword.text.text_id == "rh:text.action.buy_sword")
        assert(b_sword.binding.command_id == "rh:command.request_buy")
        assert(b_sword.binding.args.item == "rh:item.weapon.iron_sword")

        -- Action 2: buy_armor
        local b_armor = find_button(req.fields.buttons, "buy_armor")
        assert(b_armor ~= nil)
        assert(b_armor.text.text_id == "rh:text.action.buy_armor")
        assert(b_armor.binding.command_id == "rh:command.request_buy")
        assert(b_armor.binding.args.item == "rh:item.armor.leather_armor")

        -- Travel button
        local b_travel = find_button(req.fields.buttons, "travel_city_tavern")
        assert(b_travel ~= nil)
        assert(b_travel.binding.command_id == "rh:command.travel")
        assert(b_travel.binding.args[1] == "rh:location.city.tavern")
    end,

    declarative_tavern_screen_matches_schema = function()
        local req = location_presenter.build_screen_request("rh:location.city.tavern")
        assert(req ~= nil, "screen request must be generated for tavern")
        assert(req.screen_id == "rh:screen.location.tavern")

        assert(#req.fields.buttons.value.items == 4)

        local b_wait = find_button(req.fields.buttons, "wait_day")
        assert(b_wait ~= nil)
        assert(b_wait.text.text_id == "rh:text.action.wait_day")
        assert(b_wait.binding.command_id == "rh:command.time.wait_day")

        local b_work = find_button(req.fields.buttons, "do_work")
        assert(b_work ~= nil)
        assert(b_work.text.text_id == "rh:text.action.do_work")
        assert(b_work.binding.command_id == "rh:command.work.do_work")

        local b_m = find_button(req.fields.buttons, "travel_city_market")
        assert(b_m ~= nil)
        assert(b_m.binding.args[1] == "rh:location.city.market")

        local b_g = find_button(req.fields.buttons, "travel_city_gate")
        assert(b_g ~= nil)
        assert(b_g.binding.args[1] == "rh:location.city.gate")
    end,
}
