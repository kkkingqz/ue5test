---
title: Presentation Authoring Reference
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/AuthoringSurfaceContract.md
  - ../UI/UIDocumentAndReconciliation.md
---

# Presentation Lua: практический справочник

> **Помогает:** строить value-only desired presentation — localized text, semantic bindings, Screen instances, overlays, modals и tabs.
> **Нормативно:** [Lua Authoring Surface Contract](../Architecture/AuthoringSurfaceContract.md), [UI Document and Reconciliation](../UI/UIDocumentAndReconciliation.md), [Screen Templates](../UI/ScreenTemplates.md), [Semantic Input](../UI/SemanticInput.md).
> **Источники примеров:** `GameData/textsystem/scripts/presentation/location_presenter.lua`, action bindings из `GameData/rh` и conformance fixture `Tests/Lua/authoring/events_and_presentation.lua`.

Presentation — про то, **что сейчас должно быть показано**, а не про Widget tree. Lua публикует полный desired document; UE выбирает Widget Blueprint по `screen_id` и применяет declared fields. В document передаются только values и Command bindings: callbacks, UObject, Widget names и raw asset paths запрещены.

## Связать Semantic Action с Command

Назначение: дать смысловому действию стабильный `action_id`, не привязывая контент к имени Lua function.

```lua
actions[action_key_or_id] = command_key_or_id
actions[action_key_or_id] = {
    command = command_key_or_id_or_descriptor,
    args = default_args_optional,
}
```

Production-пример из `GameData/rh/scripts/authoring/gameplay.lua`:

```lua
actions["textsystem:action.location.travel"] = "rh:command.travel"
actions["rh:action.buy_sword"] = {
    command = "buy",
    args = { item = "rh:item.weapon.iron_sword" },
}
```

Короткие keys получают namespace текущего package. Полные IDs нужны для cross-package binding. Default args должны быть portable и могут быть дополнены аргументами конкретного UI element.

Типичные ошибки: `InvalidActionKey`, `InvalidActionValue`, `InvalidActionBinding`; unresolved action при использовании даёт `ActionNotBound`.

## Построить binding для элемента

Назначение: создать value-only `{ command_id, args }`, который Presentation превратит в opaque runtime binding handle.

```lua
local binding = action(command_descriptor_or_action_or_id, ...)
```

Рабочие формы из conformance fixture:

```lua
local buy = action("shop.buy", def.item("weapon.iron_sword"), 1)
local travel = action("textsystem:action.location.travel", {
    target_location_id = "rh:location.city.tavern",
})
local wait = action(commands["time.wait_day"])
```

String может быть коротким Command key, полным Command ID или Action ID. Wrapper arguments автоматически превращаются в portable tagged references. Function/closure передавать нельзя (`ActionClosureDisallowed`); invalid descriptor/ID даёт `ActionInvalidDescriptor` или `ActionInvalidCommandId`.

## Создать текст

Назначение: создать локализуемый `TextSpec`, а не готовую display string.

```lua
local text_spec = text(text_key_or_id, args_optional, style_optional)
```

Примеры из `Tests/Lua/authoring/events_and_presentation.lua`:

```lua
local description = text("screen.market.description")
local amount = text("core:text.shop.price", { value = 25 }, "emphasis")
```

Короткий key становится `<package>:text.<key>`; style по умолчанию `default`. Физический font/size/color в Lua не задаются. Пустой или неверный key даёт `InvalidTextKey`.

User-facing raw string запрещена там, где ожидается `TextSpec`: это `RawStringDisallowed`, а не автоматическая локализация.

## Создать кнопку

Назначение: объединить `TextSpec`, Command binding и стабильную identity повторяемого элемента.

```lua
local item = button(text_spec, action_binding, key_optional)
```

Production-form из location presenter:

```lua
local travel_button = button(
    text("location.market.title"),
    action("textsystem:action.location.travel", "rh:location.city.market"),
    "travel_city_market"
)
```

Если `key` опущен, helper детерминированно выводит его из `command_id` и args. Для изменяемых/reorderable списков лучше задавать domain-stable key явно. Display text, `text_id` и индекс массива не являются identity.

Типичные ошибки: `RawStringDisallowed`, `InvalidTextSpec`, `InvalidActionBinding`, `InvalidButtonKey`, `TextDisallowedAsKey`, `UiElementKeyDuplicate`.

## Показать Route Screen

Назначение: заменить основной Screen Instance и опубликовать следующую полную revision UI document.

```lua
show_screen(screen_spec)
show_route(screen_spec) -- alias
```

Production-form из `GameData/textsystem/scripts/presentation/location_presenter.lua`:

```lua
local buttons = {
    button(
        text("action.travel_market"),
        action("textsystem:action.location.travel", "rh:location.city.market"),
        "travel_city_market"
    ),
}

show_screen({
    template = "rh:screen.location.tavern",
    description = text("location.tavern.description"),
    buttons = buttons,
})
```

`template` и `screen_id` — aliases; значение может быть коротким key, полным Screen Stable ID или screen Definition wrapper. `description` + `buttons` — удобная форма для стандартных schemas. Универсальная форма передаёт полную map полей:

```lua
show_route({
    screen_id = "core:screen.shop",
    instance_key = "main",
    fields = {
        description = {
            schema_id = "core:schema.ui_field.rich_text.v3",
            value = { text = text("shop.description"), spans = {} },
        },
        buttons = {
            schema_id = "core:schema.ui_field.button_list.v2",
            value = { items = buttons },
        },
    },
})
```

Fields обязаны совпасть с contract выбранного Screen Template. Не передавайте Blueprint class/path, Widget names или частичный patch. Типичные ошибки: `InvalidShowScreenSpec`, `InvalidScreenTemplate`, `InvalidTextSpec`, `InvalidButtonsList`, `RawStringDisallowed`, `UiElementKeyMissing`, `UiElementKeyInvalid`, `UiElementKeyDuplicate`.

`show_screen`/`show_route` — side effect и запрещён внутри Validator (`AuthoringValidatorSideEffectDisallowed`).

## Показать и закрыть Overlay

Назначение: добавить или заменить keyed Screen Instance в overlay layer, затем удалить его по той же identity.

```lua
show_overlay(instance_key, screen_spec)
close_overlay(instance_key)
```

Минимальная форма следует текущей реализации `Scripts/authoring/presentation.lua`; первый production consumer пока не добавлен:

```lua
show_overlay("inventory", {
    template = "rh:screen.inventory",
    fields = {
        items = inventory_items_field,
    },
})

close_overlay("inventory")
```

`instance_key` — стабильная локальная identity, не display text. Повторный `show_overlay` с тем же key заменяет desired instance. Закрытие неизвестного key ничего не публикует. Пустой key даёт `InvalidInstanceKey`; остальные ошибки совпадают с Screen spec. Оба вызова запрещены в Validator.

## Показать и закрыть Modal

Назначение: добавить Screen Instance в modal stack и удалить конкретный либо верхний modal.

```lua
show_modal(instance_key, screen_spec)
close_modal(instance_key_optional)
```

Форма следует текущей реализации `Scripts/authoring/presentation.lua`; production consumer пока отсутствует:

```lua
show_modal("confirm_purchase", {
    template = "core:screen.confirmation",
    title = text("shop.confirm.title"),
    content = text("shop.confirm.description"),
    buttons = {
        button(text("action.confirm"), action("buy"), "confirm"),
        button(text("action.cancel"), action("close_dialog"), "cancel"),
    },
})

close_modal("confirm_purchase")
-- close_modal() закрывает один modal без явного key
```

Shorthand `title`/`content`/`buttons` строит поле `core:schema.ui_field.modal.v1`. Для другого template используйте `fields`. Modal key обязан быть non-empty (`InvalidInstanceKey`); raw title/content/button text даёт `RawStringDisallowed`. Вызовы запрещены в Validator.

## Собрать Tabs

Назначение: построить portable field для Screen Template с tab-container schema.

```lua
local tab_spec = tab(key, title_text_spec, screen_id_or_definition, fields_optional)
local field_value = tab_container({
    tabs = { tab_spec, ... },
    default_tab = key_optional,
})
local same_value = tabs({ ... }) -- alias tab_container
```

Форма следует `Scripts/authoring/presentation.lua`; production consumer пока отсутствует:

```lua
local shop_tabs = tab_container({
    default_tab = "weapons",
    tabs = {
        tab("weapons", text("shop.tabs.weapons"), "rh:screen.shop.weapons", {}),
        tab("armor", text("shop.tabs.armor"), "rh:screen.shop.armor", {}),
    },
})

show_screen({
    template = "rh:screen.shop",
    fields = { sections = shop_tabs },
})
```

Tab key — стабильная identity, не localized text. `tabs` обязан быть непустым array. Типичные ошибки: `InvalidTabKey`, `TextDisallowedAsKey`, `InvalidTextSpec`, `InvalidScreenTemplate`, `InvalidFields`, `InvalidTabContainerSpec`.

## Desired presentation и обновление экрана

Authoring helpers поддерживают один active UI document: route, keyed overlays и ordered modal stack. Каждый `show_*`/`close_*` публикует новую **полную** revision. Отсутствующий overlay означает удалить его; изменившийся `screen_id` означает заменить Screen Widget; совпавшие stable keys позволяют сохранить UI-local state.

Presentation builder должен быть чистой проекцией текущего canonical state и pinned definitions:

```text
canonical state + definitions -> complete desired document -> reconciliation
semantic input -> Command -> new canonical state -> new desired document
```

Не храните gameplay facts в Widget, не мутируйте state из Presentation и не передавайте Lua callback. UI отправляет только opaque binding handle; Command Dispatcher исполняет связанный `command_id`.

## Быстрая самопроверка

- Любой видимый текст создан через `text()`.
- Любое действие создано через `action()` и указывает на Command/semantic action, не closure.
- Repeated elements имеют стабильный lowercase key, не зависящий от текста или индекса.
- Screen spec содержит полный набор fields текущей revision.
- `screen_id` и `schema_id` соответствуют Screen Template contract.
- Lua не знает Blueprint class, Widget tree и raw UE asset path.
