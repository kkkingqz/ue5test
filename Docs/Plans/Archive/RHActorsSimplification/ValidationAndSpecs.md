---
title: Multi-Tier Validation and Specs Tasks
status: archived
version: 1.1
updated: 2026-08-19
depends_on:
  - RhActorsMigration.md
  - ../../../Architecture/HeadlessSimulationContract.md
decisions:
  - ../../../ADR/0024-lua-spec-runner.md
  - ../../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M4 — Multi-Tier Validation and Specs

> **Материализует:** [ADR-0032 § 1—8](../../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части многоуровневой валидации инвариантов, геймплейных отказов и кросс-хостового детерминизма.
> **Задачи:** RAS-11…12.
> **Результат:** сквозное покрытие тестами на всех трёх уровнях (`Core`, `TextSystem`, `FullGame`), проверка детекции структурных нарушений, сохранение digest parity и чистоты save container.

## Результат этапа

Поведение системы покрыто спеками на трёх уровнях тестирования. Проверены: отказ записи недопустимых значений полей, корректная выдача геймплейных отказов `fail()`, изоляция создания экземпляров, чистота сохраняемого состояния и совпадение хэшей в golden run.

## Задачи

- [x] **RAS-11 — Спеки Core и TextSystem tier для дескрипторов полей и `instances.create`**
  - Зависимости: RAS-10.
  - Done: добавлена спека `Tests/Lua/actors/field_contracts.lua` (Core tier): проверка `field.*` дескрипторов, валидации типов (`non_negative_integer`, `integer`, `string`), отклонения отрицательных/дробных/строковых значений при записи; проверка композиции схем (поле `Actor` действует при наличии схемы по `discriminator`), отказа `FieldAlreadyDeclared` без `override`, сохранения `ActorTypeNotRegistered` и отказа на неизвестный вид экземпляра; проверка фасада `instances.create`; спека TextSystem tier: изоляция методов актора в `textsystem` без RPG-полей.
  - Evidence: `Tests/Lua/actors/field_contracts.lua`, `Tests/Lua/world/current_location.lua`, golden-прогон.

- [x] **RAS-12 — Спеки FullGame tier и сквозная верификация**
  - Зависимости: RAS-11.
  - Done: обновлены спеки `Tests/Lua/economy/actor_rh_economy.lua`, `Tests/Lua/economy/travel_stamina.lua`, `Tests/Lua/authoring/simplified_surface.lua`: проверка работы `get/require/spend/add_gold`, `get/require/spend/add_stamina`, `add_item`, сохранения чистоты save-контейнера (`INV-001`, `INV-008`); проверка отклонения `spend_gold(-10)`; запуск golden replay с подтверждением, что изменились только `script_set_hash` и производный `digest_hash`.
  - Evidence: `Tests/Lua/actors/field_contracts.lua`, `Tests/Lua/economy/actor_rh_economy.lua`, golden-прогон.

## Проверка milestone

- [x] Попытка прямой записи недопустимого значения в поле сущности (`actor.gold = -5`, `actor.gold = 1.5`, `actor.gold = "abc"`) прерывается структурной ошибкой.
- [x] Геймплейный вызов `require_gold` при нехватке возвращает штатный отказ `fail("economy.insufficient_gold", ...)` без изменения `write_revision`.
- [x] Поле, объявленное на `Actor`, действует при наличии схемы по `discriminator`.
- [x] Повторное объявление поля без `override` и неизвестный `discriminator` отклоняются.
- [x] В golden изменились только `script_set_hash` и производный `digest_hash`.
- [x] Все 79+ тестов `ctest`, `validate_docs.py`, `validate_core_boundary.py`, `validate_host_conformance_parity.py` и `gv2-headless --check-scripts` проходят на 100%.
