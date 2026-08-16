---
title: Determinism and Save Tasks
status: draft
version: 1.0
updated: 2026-08-16
depends_on:
  - ModulesFromPackages.md
  - ../../Architecture/HeadlessSimulationContract.md
  - ../../Architecture/CanonicalStateAndSave.md
---

# M5 — Determinism and Save

> **Материализует:** [Headless Simulation Contract](../../Architecture/HeadlessSimulationContract.md), [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md).
> **Задачи:** PKG-20…23.
> **Результат:** состав пакетов и набор скриптов входят в идентичность прогона и сейва.

## Результат этапа

Замещение модуля перестаёт быть невидимым: оно меняет digest прогона, фиксируется в сейве и отображается в выводе `--check-scripts`.

## Задачи

- [x] **PKG-20 — `ScriptSetHash` в run manifest**
  - Зависимости: PKG-16.
  - `FRunManifest` сегодня фиксирует `LuaReleaseNumber`, `RepositoryContentHash`, seed и принятые команды; набор скриптов — нет. Замещение модуля меняет прогон, а манифест этого не видит.
  - Done: `FRunManifest` получает `ScriptSetHash` — канонический хэш по упорядоченному списку `(module_id, [(package_id, source_hash), …])` для всей цепочки каждого модуля; хэш детерминирован и не зависит от порядка обхода файловой системы; расхождение при replay даёт отказ того же класса, что существующий `lua_release_mismatch`; golden-прогоны обновлены осознанно, одним change set.
  - Evidence: `FRunManifest` и `FRunDigest` содержат поле `ScriptSetHash`; `FRuntimeSession` детерминированно вычисляет `ScriptSetHash` по упорядоченному списку `(module_id, [(package_id, source_hash), …])`; `ReplayRunManifest` проверяет `ScriptSetHash` и при несовпадении возвращает `core:fault.run_manifest.script_set_hash_mismatch`; `GV2RunManifestConformance` и `GV2RunDigestConformance` проверяют валидацию, сериализацию и чувствительность к изменению `ScriptSetHash`; golden fixtures `golden_headless_10_seed_42.manifest.json5` и `.digest.json5` обновлены.

- [x] **PKG-21 — Состав пакетов в сейве**
  - Зависимости: PKG-07, PKG-20.
  - [Modding](../../Architecture/Modding.md) уже требует хранить в сейве enabled mods, order, versions и fingerprints.
  - Done: конверт сейва фиксирует фактический набор пакетов, их порядок, версии и `ScriptSetHash`; сейв, снятый с замещённым модулем, отличим от сейва без мода; отсутствие пакета при загрузке обрабатывается общим правилом отсутствующего мода и не превращается в тихий откат на базовую реализацию; `save_version` поднимается, если меняется форма конверта.
  - Evidence: `Scripts/runtime/save.lua` (`build_envelope` и `save`) записывает `script_set_hash` и `packages`; `Scripts/runtime/load.lua` (`preflight` и `decode_and_prepare`) проверяет наличие всех пакетов сейва в `game.runtime.packages`, возвращая типизированную ошибку `SaveMissingPackage: <id>`; проверено новой Lua-спекой `Tests/Lua/save/package_composition_save.lua`.

- [x] **PKG-22 — Вывод цепочек в `--check-scripts`**
  - Зависимости: PKG-17.
  - Done: `gv2-headless --check-scripts` печатает по каждому замещённому `module_id` полную цепочку провайдеров в порядке применения; вывод остаётся машиночитаемым и детерминированным; незамещённые модули цепочку не печатают.
  - Evidence: `FRuntimeSession::GetReplacedModules()` и `FRuntimeSession::CheckScripts` возвращают `FReplacedModuleInfo`; `Headless/Source/main.cpp` выводит `script_set_hash` и массив `replaced_modules` с цепочками провайдеров; проверено в `GV2RuntimeCoreTests.cpp` (`FGV2LuaModulePackageOverrideTest`) и CTest `gv2_headless_check_scripts`.

- [x] **PKG-23 — Cross-host parity и синхронизация документации**
  - Зависимости: PKG-20–PKG-22.
  - Done: одинаковый набор пакетов даёт одинаковый `ScriptSetHash` и одинаковый run digest в UE и headless; golden-прогон с включённым мод-пакетом добавлен в CI; `HeadlessSimulationContract` описывает роль `ScriptSetHash` в digest, `CanonicalStateAndSave` — состав пакетов в конверте, `BuildAndTooling` — вывод `--check-scripts`; [Implementation Status](../../Status/ImplementationStatus.md) обновлён.
  - Evidence: CTest (57/57 passed), `gv2-headless --self-test` (1000 команд) и `gv2-headless --check-scripts` зелёные; cross-host golden digest parity тест `GV2.Runtime.Session.CrossHostDigestParity` и conformance-тесты согласованы; обновлены контракты `HeadlessSimulationContract.md`, `CanonicalStateAndSave.md`, `BuildAndTooling.md`, `ImplementationStatus.md`, `PackageSupport/README.md`.

## Проверка milestone

- [x] Замещение модуля меняет `ScriptSetHash` и обнаруживается при replay.
- [x] Одинаковый набор пакетов даёт одинаковый digest в обоих хостах.
- [x] Сейв фиксирует состав пакетов; сейв с модом отличим от сейва без него.
- [x] `--check-scripts` печатает цепочку замещений детерминированно.
