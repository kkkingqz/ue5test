---
title: Lua Authoring Reference Index
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - ../README.md
  - ../Architecture/AuthoringSurfaceContract.md
---

# Authoring Lua: справочник автора игры

Этот раздел помогает писать правила игры и desired presentation в package Lua. Он объясняет public authoring surface на примерах и не вводит архитектурных правил. Если пример расходится с [Lua Authoring Surface Contract](../Architecture/AuthoringSurfaceContract.md), прав contract.

## С чего начать

1. Для команд, сущностей, полей, сервисов и событий откройте [Lua Gameplay Reference](LuaGameplayReference.md).
2. Для текста, действий, кнопок, экранов, overlays, modals и tabs — [Presentation Authoring Reference](PresentationAuthoringReference.md).
3. Authoring-файл размещается в `<package>/scripts/authoring/` и получает инструменты через `_ENV`; импортировать authoring infrastructure вручную не нужно.

Рабочие production-примеры взяты из `GameData/rh`, `GameData/sample` и `GameData/textsystem`. Если инструмент пока не используется production package, рядом указан conformance fixture из `Tests/Lua/`; пример повторяет проверяемую им форму.

## Полный inventory

| Инструмент | Назначение | Где объясняется |
|---|---|---|
| `commands[key] = fn` | объявить Command Handler | [Gameplay: команды](LuaGameplayReference.md#объявить-команду) |
| `commands[key]` | получить `CommandDescriptor` | [Gameplay: descriptor](LuaGameplayReference.md#commanddescriptor-run-later-и-replaceable) |
| `descriptor:run(...)`, `:later(...)`, `:set_replaceable()`, `:replaceable()` | вызвать, отложить или разрешить замену команды | [Gameplay: descriptor](LuaGameplayReference.md#commanddescriptor-run-later-и-replaceable) |
| `validate(command_ref, name, fn)` | добавить read-only Validator | [Gameplay: validators](LuaGameplayReference.md#добавить-независимый-validator) |
| `fail(key, params)` | завершить Command/Validator typed refusal-ом | [Gameplay: отказ](LuaGameplayReference.md#отказать-без-частичной-мутации) |
| `Actor`, `Location`, `Quest`, `Item`, custom PascalCase | объявить fields и entity methods | [Gameplay: сущности](LuaGameplayReference.md#объявить-поля-и-методы-сущности) |
| `field.non_negative_integer`, `positive_integer`, `integer`, `number`, `string`, `boolean`, `enum`, `ref_definition`, `ref_instance` | описать runtime field | [Gameplay: fields](LuaGameplayReference.md#справочник-field) |
| `definition:reset(field_name)` | удалить sparse runtime override Definition field | [Gameplay: fields](LuaGameplayReference.md#справочник-field) |
| `player`, `world` | обратиться к текущим runtime objects | [Gameplay: accessors](LuaGameplayReference.md#получить-player-world-definition-и-actor) |
| `def.<kind>(name)`, `location(name)` | получить Definition wrapper | [Gameplay: accessors](LuaGameplayReference.md#получить-player-world-definition-и-actor) |
| `actor(name)`, `actors(name)` | найти один или все actor instances данного definition | [Gameplay: accessors](LuaGameplayReference.md#получить-player-world-definition-и-actor) |
| `instances.register_kind`, `instances.create` | зарегистрировать category и создать Runtime Instance | [Gameplay: instances](LuaGameplayReference.md#создать-runtime-instance) |
| `services.name = { ... }`, `services.name.method(...)` | объявить и вызвать Gameplay Service | [Gameplay: services](LuaGameplayReference.md#скоординировать-несколько-сущностей-через-service) |
| `emit(name, payload)`, `on(name, fn)` | опубликовать post-commit fact и подписаться | [Gameplay: events](LuaGameplayReference.md#опубликовать-и-обработать-event) |
| `actions[key] = binding` | связать semantic action с Command | [Presentation: actions](PresentationAuthoringReference.md#связать-semantic-action-с-command) |
| `action(target, ...)` | построить value-only command binding | [Presentation: action](PresentationAuthoringReference.md#построить-binding-для-элемента) |
| `text(key, args, style)` | построить `TextSpec` | [Presentation: text](PresentationAuthoringReference.md#создать-текст) |
| `button(text_spec, binding, key)` | построить кнопку со stable key | [Presentation: button](PresentationAuthoringReference.md#создать-кнопку) |
| `show_screen(spec)`, `show_route(spec)` | опубликовать route screen | [Presentation: route](PresentationAuthoringReference.md#показать-route-screen) |
| `show_overlay(key, spec)`, `close_overlay(key)` | управлять keyed overlay | [Presentation: overlay](PresentationAuthoringReference.md#показать-и-закрыть-overlay) |
| `show_modal(key, spec)`, `close_modal(key?)` | управлять modal stack | [Presentation: modal](PresentationAuthoringReference.md#показать-и-закрыть-modal) |
| `tab(key, title, screen, fields)` | построить вкладку | [Presentation: tabs](PresentationAuthoringReference.md#собрать-tabs) |
| `tab_container(spec)`, `tabs(spec)` | построить tab-container field | [Presentation: tabs](PresentationAuthoringReference.md#собрать-tabs) |

## Граница справочника

Здесь нет low-level registry API, Lua VM lifecycle, raw state layout или Widget implementation. Для этих задач нужны [Runtime Facade](../Architecture/RuntimeFacadeAndRegistries.md), [Lua Runtime](../Architecture/LuaRuntimeContract.md), [Canonical State](../Architecture/CanonicalStateAndSave.md) и [UI contracts](../UI/README.md).

Authoring API не отменяет обычный Lua 5.4: доступны безопасные `table`, `string`, `math`, `utf8`, `pairs`, `ipairs`, `type`, `assert`, `error`, `pcall`. Новый global создавать нельзя (`AuthoringGlobalWriteDisallowed`); используйте `local`.

`guard_validator_side_effect` может присутствовать в `_ENV` как служебный hook wrappers, но не входит в public authoring API и не должен вызываться игровым кодом.
