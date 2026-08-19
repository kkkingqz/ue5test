---
title: Entity Extension Registry Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0031-entity-authoring-extensions.md
---

# M1 — Extension Registry

> **Материализует:** [ADR-0031 § 2.2](../../ADR/0031-entity-authoring-extensions.md) в части централизованного реестра расширений и композиции методов.
> **Задачи:** EAE-01…04.
> **Результат:** `core:module.runtime.entity_extension_registry` собирает методы, валидирует конфликты и формирует неизменяемые effective method tables.

## Результат этапа

В ядре реализован централизованный реестр расширений сущностей. Реестр накапливает методы на этапе загрузки скриптов, валидирует уникальность имён методов, формирует упорядоченные композитные таблицы методов для каждого вида сущности и замораживается на фазе `register`.

## Задачи

- [x] **EAE-01 — ADR-0031: Архитектура расширения сущностей через авторские прототипы**
  - Принят ADR, фиксирующий декларативное расширение сущностей, запрет скрытого last-writer-wins, контракт `self` и правила композиции.
  - Evidence: `Docs/ADR/0031-entity-authoring-extensions.md`, `Docs/ADR/README.md`.

- [x] **EAE-02 — Модуль реестра расширений сущностей (`core:module.runtime.entity_extension_registry`)**
  - Зависимости: EAE-01.
  - Done: реализован модуль `Scripts/runtime/entity_extension_registry.lua`; поддерживает регистрацию метода `register(source_module, package_id, entity_kind, method_name, fn)`, валидацию типов, сборку методов по `entity_kind`, интроспекцию `describe(entity_kind)` и фазу заморозки `freeze()`.
  - Evidence: `Scripts/runtime/entity_extension_registry.lua`, `Scripts/bootstrap/main.lua`, `Scripts/bootstrap/manifest.lua`, `Tests/Lua/actors/entity_extension_registry.lua`.

- [x] **EAE-03 — Валидация конфликтов и дубликатов методов**
  - Зависимости: EAE-02.
  - Done: при попытке зарегистрировать дубликат метода с тем же именем на одном `entity_kind` из разных модулей/пакетов выбрасывается типизированная ошибка `entity_extension.method_conflict` с указанием `entity_kind`, `method_name`, `existing_source` и `new_source`; проверка отклоняет некорректные имена методов и не-функции.
  - Evidence: `Scripts/runtime/entity_extension_registry.lua`, тест `conflict_validation_different_packages` в `Tests/Lua/actors/entity_extension_registry.lua`.

- [x] **EAE-04 — Построение и кэширование `effective method table`**
  - Зависимости: EAE-03.
  - Done: при вызове `freeze()` реестр компилирует неизменяемую итоговую таблицу методов для каждого зарегистрированного `entity_kind`; методы доступны через прямой lookup `get_effective_methods(entity_kind)`; после заморозки регистрация новых методов блокируется ошибкой `EntityExtensionRegistryFrozen`.
  - Evidence: `Scripts/runtime/entity_extension_registry.lua`, тест `effective_method_table_compilation_and_freezing` в `Tests/Lua/actors/entity_extension_registry.lua`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`.

## Проверка milestone

- [x] Реестр регистрирует методы с атрибуцией источника и пакета.
- [x] Дубликаты методов между модулями вызывают ошибку `entity_extension.method_conflict`.
- [x] После `freeze()` таблица эффективных методов неизменяема.
- [x] Спеки Core tier для реестра (`Tests/Lua/actions/` или `Tests/Lua/actors/`) проходят успешно.
