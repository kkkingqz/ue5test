---
title: CoreBoundaryMigration Archive Summary
status: archived
version: 1.0
updated: 2026-08-17
---

# CoreBoundaryMigration: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Привести код и контент в соответствие с : убрать из ядра демо-контент и игровую экономику, дать пакету точку расширения обёртки инстанса и перенести схемы `item`, `location` и большую часть `actor` в игровой пакет

**Результат:** в `core` не остаётся ни одной сущности, выражающей правила текущей игры

## Этапы и задачи

### M1 — Demo Out

демонстрационный экран и его контент перестают быть частью движка

- `CBM-01` — Завести пакет `sample`
- `CBM-02` — Перенести демо-экран и его модуль
- `CBM-03` — Развязать проверки от демо

### M2 — Instance Extension

поведение инстанса определяет пакет, идентичность — ядро

- `CBM-04` — Ввести реестр декораторов обёртки
- `CBM-05` — Собирать обёртку через декоратор
- `CBM-06` — Определить поведение при отсутствии регистрации
- `CBM-07` — Перенести экономику в `rh`
- `CBM-08` — Синхронизировать contract

### M3 — Schema Migration

предметная модель игры описана её пакетом, ядро описывает только то, что требует runtime

- `CBM-09` — Разрешить пакету привязывать схему существующего kind
- `CBM-10` — Реестр ссылочных полей состояния
- `CBM-11` — Перенести схемы `item` и `location`
- `CBM-12` — Урезать `actor_v1` до `discriminator`
- `CBM-13` — Синхронизировать contract и фикстуры

### M4 — Boundary Gate

новая игровая сущность в ядре ломает CI

- `CBM-14` — Гейт на игровые сущности в ядре
- `CBM-15` — Закрыть список известных нерегистраций
- `CBM-16` — Синхронизировать документацию

## Актуальные нормативные источники

- [Overview](../../Architecture/Overview.md)
- [Modding](../../Architecture/Modding.md)
- [DefinitionEnvelopeAndSchemaRules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/CoreBoundaryMigration) содержит исходные task-файлы, acceptance criteria и evidence.
