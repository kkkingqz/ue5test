---
title: Conformance and Cleanup Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ModuleAndLifecycle.md
  - StateAndIsolation.md
  - MutationAndHashing.md
  - ../../../Architecture/BuildAndTooling.md
decisions:
  - ../../../ADR/0024-lua-spec-runner.md
---

# M4 — Conformance and Cleanup

## Результат этапа

Унаследованный файл `GV2LuaLifecycleConformance.cpp` удалён из кодовой базы, сборка очищена, CI-гейт `validate_host_conformance_parity.py` и документация синхронизированы.

## Задачи

- [x] **LSM-07 — Удаление `GV2LuaLifecycleConformance` и обновление сборки**
  - Описание: Удалить `Source/GV2RuntimeCore/Private/GV2LuaLifecycleConformance.cpp` и заголовок `GV2LuaLifecycleConformance.h`; удалить вызовы из `Headless/Source/main.cpp` и `GV2RuntimeCoreTests.cpp`; обновить `Source/CMakeLists.txt`.
  - Done: Файлы удалены, проект собирается без ошибок.
  - Evidence: Удалены `Source/GV2RuntimeCore/Private/GV2LuaLifecycleConformance.cpp` (5326 строк) и `Source/GV2RuntimeCore/Public/GV2RuntimeCore/Testing/GV2LuaLifecycleConformance.h`; удалены вызовы из `Headless/Source/main.cpp` и `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`; `Source/CMakeLists.txt` обновлён; проект пересобран CMake без предупреждений.

- [x] **LSM-08 — Синхронизация гейтов CI и документации**
  - Описание: Удалить `GV2LuaLifecycleConformance.cpp` из `LEGACY_LUA_RULE_CONFORMANCE_FILES` в `Tools/Content/validate_host_conformance_parity.py`; обновить `BuildAndTooling.md`, `ImplementationStatus.md` и счётчики conformance entry points (с 25 до 24).
  - Done: Все CTest тесты, Python скрипты и документация валидны.
  - Evidence: Обновлены `Tools/Content/validate_host_conformance_parity.py` (24 entry points), `Docs/Architecture/BuildAndTooling.md` (v2.1) и `Docs/Status/ImplementationStatus.md` (v1.12); `validate_docs.py` (108 docs valid) и `validate_host_conformance_parity.py` (24 entry points) зелёные; CTest (57/57 passed).

## Проверка milestone

- [x] `GV2LuaLifecycleConformance.cpp` отсутствует в кодовой базе.
- [x] `validate_host_conformance_parity.py` и `validate_docs.py` проходят успешно.
- [x] Все CTest тесты и Headless self-test зелёные.
