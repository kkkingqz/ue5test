-- LOC-06: TextSpec is constructed in Lua as an unresolved data structure { text_id, args, style }
-- without resolution or translation access on the Lua side.
local text = require("core:module.resources.text")

return {
    text_spec_constructs_canonical_envelope = function()
        local spec = text.spec("core:text.character.hero.name")
        assert(spec.text_id == "core:text.character.hero.name", "text_id must match")
        assert(type(spec.args) == "table", "args must be a table")
        assert(spec.style == nil, "style must be nil when omitted")
    end,

    text_spec_retains_named_args_and_style = function()
        local args = { player_name = "Arthur", level = 10 }
        local spec = text.spec("core:text.item.iron_sword.name", args, "inventory")
        assert(spec.text_id == "core:text.item.iron_sword.name")
        assert(spec.args.player_name == "Arthur")
        assert(spec.args.level == 10)
        assert(spec.style == "inventory")
    end,

    text_spec_rejects_invalid_text_id_kind = function()
        local ok, err = pcall(function()
            text.spec("core:item.iron_sword")
        end)
        assert(not ok, "text.spec must reject non-text kind")
    end,

    text_spec_rejects_invalid_style_token = function()
        local ok, err = pcall(function()
            text.spec("core:text.item.iron_sword.name", {}, "Invalid-Style!")
        end)
        assert(not ok, "text.spec must reject non-canonical style token")
    end,
}
