---
title: Module Discovery Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - AuthoringEnvironment.md
  - ../../../Architecture/BuildAndTooling.md
---

# M2 — Module Discovery

> **Материализует:** [Build and Tooling](../../../Architecture/BuildAndTooling.md) и [Lua Runtime Contract § Module loader](../../../Architecture/LuaRuntimeContract.md).
> **Задачи:** SAS-06…09.
> **Результат:** добавление файла не требует правки другого файла.

## Результат этапа

Ручной `manifest.lua` исчезает из исходников пакета. Детерминированный граф модулей остаётся — он становится генерируемым артефактом.

## Задачи

- [x] **SAS-06 — Детерминированное обнаружение и генерируемый манифест**
  - Зависимости: SAS-01.
  - Done: инструмент сборки обходит `scripts/` пакета и строит канонический манифест; порядок обхода файловой системы не влияет на результат; манифест воспроизводится побайтово при неизменных входах; рантайм по-прежнему отвергает незаявленный источник — проверяется сгенерированный манифест, а не отсутствие проверки; расхождение сгенерированного и зафиксированного манифеста — типизированная диагностика, а не тихая перегенерация.
  - Evidence: `Tools/Content/generate_manifest.py`, `Tools/Content/test_authoring_tools.py:37a-c`.

- [x] **SAS-07 — Правила идентичности модуля**
  - Зависимости: SAS-06.
  - `module_id` замещаемого модуля — публичная поверхность: мод нацеливается на него по имени, цепочка провайдеров входит в `ScriptSetHash`.
  - Done: ID выводится из пути **только для незамещаемых** модулей (`scripts/runtime/actors.lua` → `rh:module.runtime.actors`); модуль, помеченный `replaceable`, обязан объявить ID явно, иначе отвергается диагностикой; переименование файла незамещаемого модуля не ломает ничего, переименование файла замещаемого — не меняет его ID.
  - Evidence: `Tools/Content/generate_manifest.py:derive_module_id_from_path`, `Tools/Content/test_authoring_tools.py:37d`.

- [x] **SAS-08 — Зависимости из литеральных `require`**
  - Зависимости: SAS-06.
  - Done: статический скан собирает зависимости из литеральных `require("…")`; `require(variable)` в автообнаруживаемом модуле отвергается диагностикой; отношение замещения (`require_base()`) статическим сканом не выводится и требует явных метаданных — это записано вместе с причиной; собранный граф проходит те же проверки на циклы, недостижимость и отсутствующие зависимости.
  - Evidence: `Tools/Content/generate_manifest.py:scan_dependencies_and_validate`, `Tools/Content/test_authoring_tools.py:37e,37f`.

- [x] **SAS-09 — Редкие метаданные и синхронизация contract**
  - Зависимости: SAS-07, SAS-08.
  - Done: `replaceable`, цель замещения и особые зависимости жизненного цикла объявляются в `package.json5` либо компактным блоком для программиста; обычный authoring-скрипт таких сведений не содержит; [Build and Tooling](../../../Architecture/BuildAndTooling.md) описывает обнаружение и генерацию, [Lua Runtime Contract](../../../Architecture/LuaRuntimeContract.md) — правила идентичности.
  - Evidence: `GameData/rh/package.json5:modules`, `Docs/Architecture/BuildAndTooling.md`, `Docs/Architecture/LuaRuntimeContract.md`.

## Проверка milestone

- [x] Новый designer-файл подхватывается без правки других файлов.
- [x] Сгенерированный манифест воспроизводится побайтово.
- [x] Замещаемый модуль без явного ID отвергается.
- [x] `require(variable)` в автообнаруживаемом модуле отвергается.
