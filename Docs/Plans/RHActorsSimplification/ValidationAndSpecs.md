---
title: Multi-Tier Validation and Specs Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - RhActorsMigration.md
  - ../../Architecture/HeadlessSimulationContract.md
decisions:
  - ../../ADR/0024-lua-spec-runner.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M4 — Multi-Tier Validation and Specs

> **Материализует:** [ADR-0032 § 1, 3, 4](../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части многоуровневой валидации инвариантов, геймплейных отказов и кросс-хостового детерминизма.
> **Задачи:** RAS-11…12.
> **Результат:** сквозное покрытие тестами на всех трёх уровнях (`Core`, `TextSystem`, `FullGame`), проверка детекции структурных нарушений, сохранение digest parity и чистоты save container.

## Результат этапа

Поведение системы покрыто спеками на трёх уровнях тестирования. Проверены: отказ записи недопустимых значений полей, корректная выдача геймплейных отказов `fail()`, изоляция создания экземпляров, чистота сохраняемого состояния и совпадение хэшей в golden run.

## Задачи

- [ ] **RAS-11 — Спеки Core и TextSystem tier для дескрипторов полей и `instances.create`**
  - Зависимости: RAS-10.
  - Done: добавлена спека `Tests/Lua/actors/field_contracts.lua` (Core tier): проверка `field.*` дескрипторов, валидации типов (`non_negative_integer`, `integer`, `string`), отклонения отрицательных/дробных/строковых значений при записи; проверка фасада `instances.create` для создания динамических сущностей; спека TextSystem tier: изоляция методов актора в `textsystem` без RPG-полей.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RAS-12 — Спеки FullGame tier и сквозная верификация**
  - Зависимости: RAS-11.
  - Done: обновлены спеки `Tests/Lua/economy/actor_rh_economy.lua`, `Tests/Lua/economy/travel_stamina.lua`, `Tests/Lua/authoring/simplified_surface.lua`: проверка работы `get/require/spend/add_gold`, `get/require/spend/add_stamina`, `add_item`, сохранения чистоты save-контейнера (`INV-001`, `INV-008`); запуск golden replay и подтверждение digest parity.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Попытка прямой записи недопустимого значения в поле сущности (`actor.gold = -5`, `actor.gold = 1.5`, `actor.gold = "abc"`) прерывается структурной ошибкой.
- [ ] Геймплейный вызов `require_gold` при нехватке возвращает штатный отказ `fail("economy.insufficient_gold", ...)` без изменения `write_revision`.
- [ ] Все 79+ тестов `ctest`, `validate_docs.py`, `validate_core_boundary.py`, `validate_host_conformance_parity.py` и `gv2-headless --check-scripts` проходят на 100%.
