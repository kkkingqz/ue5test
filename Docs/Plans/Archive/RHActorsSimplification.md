---
title: RHActorsSimplification Archive Summary
status: archived
version: 1.0
updated: 2026-08-19
---

# RHActorsSimplification: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Довести файл `GameData/rh/scripts/gameplay/actors.lua` до чистого декларативного описания доменной семантики RH: 1. Описание полей состояния через дескрипторы контрактов (`field.non_negative_integer()`). 2. Централизованная валидация инвариантов полей в точке записи вместо ручных проверок `validate_amount` в каждом методе. 3. Разделение структурных инвариантов состояния и геймплейных предусловий (`require_*`). 4. Инкапсуляция создания динамических сущностей (инвентарь/предметы) через фасад `instances.create()` без прямого импорта `instance_allocator` и обращения к `game.state`. 5. Устранение неявного fallback `self -> player`

**Результат:** декларативные контракты полей сущностей (`field.*`), разделение структурных инвариантов и геймплейных предусловий, обобщённое создание экземпляров (`instances.create`), чистое доменное описание `rh/scripts/gameplay/actors.lua` без низкоуровневых утечек runtime

## Этапы и задачи

### M1 — Field Contracts and Descriptors

модуль `core:module.authoring.field`, сборка дескрипторов полей через `EntityKind.field_name = field.*`, валидация при записи по композиции схем трёх источников

- `RAS-01` — ADR-0032: Field Contracts and Generic Instance Creation
- `RAS-02` — Модуль дескрипторов полей (`core:module.authoring.field`)
- `RAS-03` — Интеграция `field` с авторскими прототипами (`authoring_context.lua`)
- `RAS-04` — Валидация значения при записи в `ActorWrapper`
- `RAS-13` — Композиция схем вместо цепочки перекрытия
- `RAS-14` — Запрет повторного объявления поля
- `RAS-15` — Сохранить проверку вида актора

### M2 — Generic Instance Creation

обобщённый фасад `instances.create(kind, payload)` в авторском `_ENV`, нормализация ссылок и инкапсуляция `instance_allocator` и коллекций состояния

- `RAS-16` — Реестр видов экземпляров
- `RAS-05` — Механизм обобщённого создания экземпляров
- `RAS-06` — Нормализация дескрипторов и ссылок владельцев
- `RAS-07` — Экспорт фасада `instances.create` в авторское окружение `_ENV`

### M3 — RH Actors Migration

`actors.lua` в пакете `rh` переведён на декларативные контракты полей и явные доменные методы; удалены `RESOURCES`, `validate_amount`, fallback на player и прямой импорт `instance_allocator`

- `RAS-08` — Перевод полей `gold` и `stamina` на `field.non_negative_integer()`
- `RAS-09` — Упрощение методов ресурсов и удаление `RESOURCES` / `validate_amount`
- `RAS-10` — Перевод `Actor:add_item()` на `instances.create()` и очистка импортов
- `RAS-17` — Локальная проверка аргументов в `rh`

### M4 — Multi-Tier Validation and Specs

сквозное покрытие тестами на всех трёх уровнях (`Core`, `TextSystem`, `FullGame`), проверка детекции структурных нарушений, сохранение digest parity и чистоты save container

- `RAS-11` — Спеки Core и TextSystem tier для дескрипторов полей и `instances.create`
- `RAS-12` — Спеки FullGame tier и сквозная верификация

## Актуальные нормативные источники

- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)
- [CanonicalStateAndSave](../../Architecture/CanonicalStateAndSave.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/RHActorsSimplification) содержит исходные task-файлы, acceptance criteria и evidence.
