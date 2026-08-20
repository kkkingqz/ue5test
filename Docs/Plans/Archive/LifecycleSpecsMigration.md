---
title: LifecycleSpecsMigration Archive Summary
status: archived
version: 1.0
updated: 2026-08-15
---

# LifecycleSpecsMigration: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Перенести проверки правил рантайма и жизненного цикла Lua из монолитного унаследованного C++ conformance файла `GV2LuaLifecycleConformance.cpp` (5326 строк) в декларативные Lua-спеки в `Tests/Lua/lifecycle/` согласно

**Результат:** Перенести проверки правил рантайма и жизненного цикла Lua из монолитного унаследованного C++ conformance файла `GV2LuaLifecycleConformance.cpp` (5326 строк) в декларативные Lua-спеки в `Tests/Lua/lifecycle/` согласно

## Этапы и задачи

### M1 — Module and Lifecycle

Правила обнаружения модулей, порядок разрешения графа зависимостей и вызовы хуков жизненного цикла (`init`, `register`, `ready`) покрыты декларативными спеками в `Tests/Lua/lifecycle/`

- `LSM-01` — Спека на регистрацию и топологический порядок модулей
- `LSM-02` — Спека на хуки жизненного цикла и freeze реестров

### M2 — State and Isolation

Структура канонического состояния `game.state`, изоляция секций, запрет прямых циклических ссылок и правила генерации `instance_id` покрыты декларативными спеками в `Tests/Lua/lifecycle/`

- `LSM-03` — Спека на структуру и валидатор секций `game.state`
- `LSM-04` — Спека на аллокатор `instance_id` и изоляцию прототипов

### M3 — Mutation and Hashing

Механизм `mutation_window`, изоляция мутаций состояния и канонический хэшер состояния `core:module.runtime.state_hasher` покрыты декларативными спеками в `Tests/Lua/lifecycle/`

- `LSM-05` — Спека на окно мутации `mutation_window`
- `LSM-06` — Спека на канонический хэшер состояния `state_hasher`

### M4 — Conformance and Cleanup

Унаследованный файл `GV2LuaLifecycleConformance.cpp` удалён из кодовой базы, сборка очищена, CI-гейт `validate_host_conformance_parity.py` и документация синхронизированы

- `LSM-07` — Удаление `GV2LuaLifecycleConformance` и обновление сборки
- `LSM-08` — Синхронизация гейтов CI и документации

## Актуальные нормативные источники

- [HeadlessSimulationContract](../../Architecture/HeadlessSimulationContract.md)
- [BuildAndTooling](../../Architecture/BuildAndTooling.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/LifecycleSpecsMigration) содержит исходные task-файлы, acceptance criteria и evidence.
