---
title: Presentation Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - LocationOwnership.md
  - ../../UI/ScreenTemplates.md
---

# M3 — Presentation

> **Материализует:** [Screen Templates](../../UI/ScreenTemplates.md) в части владения презентацией локации.
> **Задачи:** TSL-11…14.
> **Результат:** экран локации собирается `textsystem` из описания, а не Lua-строителем игры.

## Результат этапа

Добавление локации перестаёт требовать Lua: описание, действия и переходы задаются данными.

## Задачи

- [x] **TSL-11 — Семантические действия**
  - Зависимости: TSL-05.
  - Сегодня `action(commands.buy, sword)` возвращает пару `command_id` плюс аргументы. Именованное действие с поздним связыванием — **новый механизм**, а не переименование.
  - Done: kind `action` добавлен в реестр kind вместе с declarative schema binding, как требует [Stable ID Specification](../../Architecture/StableIDSpecification.md); реестр действий связывает `textsystem:action.*` с командой пакета на фазе `register` и замораживается; действие без привязки при использовании даёт типизированный отказ, а не молчаливое бездействие; дубликат привязки — отдельный отказ.
  - Evidence: `Scripts/runtime/action_registry.lua`, `Tests/Lua/actions/action_registry.lua`

- [x] **TSL-12 — Презентер локации в `textsystem`**
  - Зависимости: TSL-08, TSL-11.
  - Done: презентер живёт в `textsystem`, собирает экран из описания локации и списка связанных локаций и регистрируется источником презентации; кнопки переходов создают семантическое действие, а не команду напрямую; `rh` презентер не содержит; поведение меню не изменилось, спеки динамического меню проходят с прежними утверждениями о составе кнопок.
  - Evidence: `GameData/textsystem/scripts/presentation/location_presenter.lua`, `Tests/Lua/presentation/dynamic_menu.lua`

- [x] **TSL-13 — Декларативные экраны**
  - Зависимости: TSL-12.
  - Предусловия закрыты планом [ContentEditorPrerequisites](../Archive/ContentEditorPrerequisites/README.md).
  - Done: схема экрана `textsystem` получает описание, список семантических действий с параметрами и признак включения переходов в связанные локации; изменение схемы выполнено по классификации из [ADR-0029](../../ADR/0029-content-authoring-and-schema-evolution.md); экраны `rh` содержат только контент и конфигурацию; добавление локации не требует Lua — проверяется добавлением локации в рамках задачи и последующим откатом.
  - Evidence: `GameData/core/schemas/screen_v1.schema.json5`, `GameData/textsystem/schemas/action_v1.schema.json5`, `GameData/rh/definitions/screens.json5`, `Tests/Lua/presentation/declarative_location_spec.lua`

- [x] **TSL-14 — Удалить презентер `rh`**
  - Зависимости: TSL-13.
  - Done: `rh/scripts/presentation/location_screen.lua` удалён вместе с таблицей конфигурации локаций; описания и действия живут в определениях экранов; каталог `rh/scripts/presentation/` отсутствует.
  - Evidence: `GameData/rh/scripts/presentation/` удален, `GameData/rh/scripts/manifest.lua`

## Проверка milestone

- [x] Экран локации собирается без Lua на стороне игры.
- [x] Действие без привязки даёт типизированный отказ.
- [x] Добавление локации не требует Lua.
- [x] `rh/scripts/presentation/` отсутствует.
