---
title: Definition Extension and Multi-Tier Validation Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - ActorMigration.md
  - ../../Architecture/HeadlessSimulationContract.md
decisions:
  - ../../ADR/0024-lua-spec-runner.md
  - ../../ADR/0031-entity-authoring-extensions.md
---

# M4 — Definition Extension and Multi-Tier Validation

> **Материализует:** [ADR-0031 § 2.3](../../ADR/0031-entity-authoring-extensions.md) в части расширения определений (`Location`) и валидации изоляции уровней тестирования.
> **Задачи:** EAE-11…12.
> **Результат:** расширение `Location` через `function Location:method()`, контракт `self` для определений контента, сквозное покрытие тестами на трёх уровнях (`Core`, `TextSystem`, `FullGame`), проверка детекции конфликтов.

## Результат этапа

Механизм авторских расширений работает для определений контента (`Location`), обеспечивая чтение `def.data` и sparse-состояния. Реализованы спеки для `EntityExtensionRegistry`, проверки конфликтов методов, сквозной композиции и соблюдения изоляции уровней тестирования.

## Задачи

- [ ] **EAE-11 — Расширение Location через прототип в authoring-скриптах**
  - Зависимости: EAE-10.
  - Done: методы определений локаций (`is_connected`, `require_connected`) переведены на синтаксис `function Location:is_connected(target)` и `function Location:require_connected(target, opt_key)`; `properties.register_definition_type` интегрирован с `entity_extension_registry`; `self` внутри метода корректно адресует поля снимка репозитория и динамические свойства.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **EAE-12 — Спеки валидации конфликтов, композиции и изоляции уровней**
  - Зависимости: EAE-11.
  - Done: добавлены спеки Core tier (`Tests/Lua/actors/entity_extensions_core.lua`): регистрация, заморозка, обнаружение конфликта методов (`entity_extension.method_conflict`); спеки TextSystem tier: проверка `Actor` и `Location` без правил `rh`; спеки FullGame tier: сквозная композиция `textsystem` + `rh` и сохранение неизменяемости save-контейнера.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Методы `Location` объявляются декларативно и доступны на объектах определений локаций.
- [ ] Ошибка конфликта дубликатов методов (`entity_extension.method_conflict`) покрыта автоматическими тестами.
- [ ] Полный набор тестов `ctest` (79+), `validate_docs.py`, `validate_core_boundary.py` и `gv2-headless --check-scripts` проходит на 100%.
