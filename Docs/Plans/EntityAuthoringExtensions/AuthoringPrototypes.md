---
title: Authoring Prototypes and Environment Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - ExtensionRegistry.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0028-simplified-authoring-surface.md
  - ../../ADR/0031-entity-authoring-extensions.md
---

# M2 — Authoring Prototypes and Environment

> **Материализует:** [ADR-0031 § 2.1, 2.4](../../ADR/0031-entity-authoring-extensions.md) в части интеграции управляемых прототипов в `_ENV` authoring-скриптов.
> **Задачи:** EAE-05…07.
> **Результат:** в `_ENV` authoring-скриптов доступны `Actor`, `Location`, `Quest`, `Item`; методы объявляются синтаксисом `function EntityKind:method(...)`; `fail()` сохраняет контекст пакета объявления; managed-свойства валидируются по `effective method table`.

## Результат этапа

Авторские скрипты получают доступ к прототипам сущностей напрямую в лексическом окружении. Присваивание методов через `function EntityKind:method()` автоматически передаёт метод в `EntityExtensionRegistry` с привязкой к текущему пакету. Валидация managed-свойств (DLA-12) интегрирована со скомпонованными таблицами методов.

## Задачи

- [x] **EAE-05 — Контролируемые прокси-прототипы в `_ENV` (`authoring_context.lua`)**
  - Зависимости: EAE-04.
  - Done: `Scripts/authoring/context.lua` инжектирует в `_ENV` прокси для `Actor`, `Location`, `Quest`, `Item`; метаметод `__newindex` валидирует типы и делегирует регистрацию в `entity_extension_registry` с автоматической передачей `source_module` и `package_id`; метаметод `__index` возвращает методы для инспекции; не-прототипные присваивания в `_ENV` отклоняются с `AuthoringGlobalWriteDisallowed`.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/simplified_surface.lua:entity_authoring_prototypes_and_method_registration`.

- [x] **EAE-06 — Атрибуция пакета и контекст `fail()` в методах сущностей**
  - Зависимости: EAE-05.
  - Done: при вызове метода сущности, использующего `authoring_context.fail(key, params)` или `fail(key, params)`, код ошибки канонизируется пространством имён пакета, в котором метод был **объявлен** (`declaring_package_id`), независимо от вызывающей команды.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/simplified_surface.lua:entity_method_fail_package_attribution`.

- [x] **EAE-07 — Интеграция с валидацией Managed Properties (DLA-12)**
  - Зависимости: EAE-05.
  - Done: фаза заморозки `actor_registry.freeze()` проверяет наличие заявленных в схеме managed-операций (`operations: [...]`) в скомпонованной `effective method table` сущности (`game.entity_extensions`), поддерживая также обратную совместимость с legacy-декораторами.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `Tests/Lua/authoring/simplified_surface.lua:managed_properties_validation_with_entity_extensions`.

## Проверка milestone

- [x] Синтаксис `function Actor:test_method()` работает внутри authoring-скрипта без `register_type`.
- [x] Ошибки `fail()` внутри метода сущности имеют префикс пакета объявления метода.
- [x] Managed-поля схемы акторов корректно валидируются по `effective method table`.
- [x] Модули программиста и стандартный `_G` не подвергаются загрязнению глобальными переменными.
