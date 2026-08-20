---
title: Add UI Screen
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - PresentationAuthoringReference.md
---

# Добавить UI Screen

> **Помогает:** создать новый Screen на существующих Screen Field schemas и опубликовать его из Lua.
> **Нормативно:** [Screen Templates](../UI/ScreenTemplates.md), [UI Document](../UI/UIDocumentAndReconciliation.md), [Semantic Input](../UI/SemanticInput.md).
> **Источник примера:** `GameData/textsystem/scripts/presentation/location_presenter.lua`, `Content/UI/`.

## Шаги

1. Добавьте Definition kind `screen` по [Add Definition](AddDefinition.md).
2. Соберите Widget Blueprint на базе `WBP_ScreenBase` из существующих Dynamic Screen Elements. Editor asset создаётся и меняется только через `unreal-mcp`, затем compile/save.
3. Добавьте `screen_id → widget class + layer` в Screen Registry через Unreal Editor API.
4. Объявите в template полный набор `field_id`, соответствующие `schema_id` и required policy.
5. Постройте полный desired Screen Instance в Lua:

   ```lua
   show_screen({
       template = "rh:screen.shop",
       description = text("shop.description"),
       buttons = {
           button(text("shop.buy"), action("buy"), "buy"),
       },
   })
   ```

6. Проверьте invalid/missing/duplicate fields, atomic apply, binding и нужные viewport sizes. Если требуется новый field schema, задача переходит программисту: [Add Screen Field](../Guides/AddScreenField.md).

Для route, overlay, modal и tabs используйте формы из [Presentation Authoring Reference](PresentationAuthoringReference.md).

## Не делайте так

- Lua не получает Blueprint class/path и Widget names — только `screen_id` и value-only fields.
- Blueprint не хранит gameplay-state и не вызывает Lua callback.
- Raw display string, raw asset path и локальный механизм text/image/input запрещены.
- Отсутствующее поле — не patch: Lua публикует полный desired document текущей revision.
