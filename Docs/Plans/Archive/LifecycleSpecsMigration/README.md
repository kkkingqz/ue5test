---
title: Lifecycle Specs Migration Implementation Plan
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../Architecture/BuildAndTooling.md
decisions:
  - ../../../ADR/0024-lua-spec-runner.md
  - ../../../ADR/0010-portable-runtime-and-headless-simulation.md
---

# План реализации Lifecycle Specs Migration

> **Архив.** План выполнен полностью (M1–M4) и больше не является источником задач. Нормативное поведение перенесено в [Headless Simulation Contract](../../../Architecture/HeadlessSimulationContract.md) и [Build and Tooling](../../../Architecture/BuildAndTooling.md). Документ сохраняется как implementation record.

## Цель

Перенести проверки правил рантайма и жизненного цикла Lua из монолитного унаследованного C++ conformance файла `GV2LuaLifecycleConformance.cpp` (5326 строк) в декларативные Lua-спеки в `Tests/Lua/lifecycle/` согласно [ADR-0024](../../../ADR/0024-lua-spec-runner.md).

## Состояние на входе

После завершения плана `TestArchitectureAndLuaSpecs` в C++ остаются три унаследованных набора со встроенным Lua (`GV2LuaLifecycleConformance` — 5326 строк, `GV2LuaRepositoryConformance` — 489 строк, `GV2ValidatorRegistryConformance` — 332 строки). Набор `GV2LuaLifecycleConformance` является самым крупным и проверяет:
1. Декларацию модулей, манифесты и порядок фаз жизненного цикла (`init`, `register`, `ready`).
2. Структуру канонического состояния `game.state`, прототипы секций и аллокатор `instance_id`.
3. Окно мутации `mutation_window`, изоляцию мутаций и канонический хэшер состояния `state_hasher`.
4. Семантику ошибок и восстановление.

## Принятые решения

- Все правила выражаются чистыми `.lua` файлами спек в `Tests/Lua/lifecycle/`.
- Тесты используют общую инфраструктуру Spec Runner (`GV2TestSupport::RunLuaSpecs`), исполняемую как в Headless, так и в UE Automation.
- Удаление `GV2LuaLifecycleConformance.cpp` сократит размер C++ кодовой базы более чем на 5300 строк и ускорит сборку `GV2RuntimeCore`.
- Скрипт валидации гейта `Tools/Content/validate_host_conformance_parity.py` синхронно обновляется (удаление из `LEGACY_LUA_RULE_CONFORMANCE_FILES`).

## Milestones

- [x] M1 — [Module and Lifecycle](ModuleAndLifecycle.md): обнаружение модулей, фазы `init`, `register`, `ready`, freeze.
- [x] M2 — [State and Isolation](StateAndIsolation.md): секции `game.state`, прототипы, `instance_allocator`.
- [x] M3 — [Mutation and Hashing](MutationAndHashing.md): `mutation_window`, защита от записи вне окна, `state_hasher`.
- [x] M4 — [Conformance and Cleanup](ConformanceAndCleanup.md): удаление `GV2LuaLifecycleConformance.cpp`, синхронизация гейтов и документации.

## Критический путь

```text
Module and Lifecycle
→ State and Isolation
→ Mutation and Hashing
→ Conformance and Cleanup
```

## Итоговый Definition of Done

- [x] Все сценарии жизненного цикла Lua покрыты спеками в `Tests/Lua/lifecycle/`.
- [x] Файл `GV2LuaLifecycleConformance.cpp` удалён из сборки CMake и UBT.
- [x] Общее число conformance entry points и список унаследованных файлов в `validate_host_conformance_parity.py` обновлены.
- [x] Все 57+ CTest тестов, Headless self-test и UE automation проходят успешно.
