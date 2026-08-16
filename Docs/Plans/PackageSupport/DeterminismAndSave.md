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

- [ ] **PKG-20 — `ScriptSetHash` в run manifest**
  - Зависимости: PKG-16.
  - `FRunManifest` сегодня фиксирует `LuaReleaseNumber`, `RepositoryContentHash`, seed и принятые команды; набор скриптов — нет. Замещение модуля меняет прогон, а манифест этого не видит.
  - Done: `FRunManifest` получает `ScriptSetHash` — канонический хэш по упорядоченному списку `(module_id, [(package_id, source_hash), …])` для всей цепочки каждого модуля; хэш детерминирован и не зависит от порядка обхода файловой системы; расхождение при replay даёт отказ того же класса, что существующий `lua_release_mismatch`; golden-прогоны обновлены осознанно, одним change set.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-21 — Состав пакетов в сейве**
  - Зависимости: PKG-07, PKG-20.
  - [Modding](../../Architecture/Modding.md) уже требует хранить в сейве enabled mods, order, versions и fingerprints.
  - Done: конверт сейва фиксирует фактический набор пакетов, их порядок, версии и `ScriptSetHash`; сейв, снятый с замещённым модулем, отличим от сейва без мода; отсутствие пакета при загрузке обрабатывается общим правилом отсутствующего мода и не превращается в тихий откат на базовую реализацию; `save_version` поднимается, если меняется форма конверта.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-22 — Вывод цепочек в `--check-scripts`**
  - Зависимости: PKG-17.
  - Done: `gv2-headless --check-scripts` печатает по каждому замещённому `module_id` полную цепочку провайдеров в порядке применения; вывод остаётся машиночитаемым и детерминированным; незамещённые модули цепочку не печатают.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PKG-23 — Cross-host parity и синхронизация документации**
  - Зависимости: PKG-20–PKG-22.
  - Done: одинаковый набор пакетов даёт одинаковый `ScriptSetHash` и одинаковый run digest в UE и headless; golden-прогон с включённым мод-пакетом добавлен в CI; `HeadlessSimulationContract` описывает роль `ScriptSetHash` в digest, `CanonicalStateAndSave` — состав пакетов в конверте, `BuildAndTooling` — вывод `--check-scripts`; [Implementation Status](../../Status/ImplementationStatus.md) обновлён.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Замещение модуля меняет `ScriptSetHash` и обнаруживается при replay.
- [ ] Одинаковый набор пакетов даёт одинаковый digest в обоих хостах.
- [ ] Сейв фиксирует состав пакетов; сейв с модом отличим от сейва без него.
- [ ] `--check-scripts` печатает цепочку замещений детерминированно.
