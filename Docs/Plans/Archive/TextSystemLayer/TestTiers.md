---
title: Test Tiers Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - PackageSet.md
  - ../../../Architecture/HeadlessSimulationContract.md
---

# M4 — Test Tiers

> **Материализует:** [Headless Simulation Contract](../../../Architecture/HeadlessSimulationContract.md) в части конфигураций сессий для спеков.
> **Задачи:** TSL-15…17.
> **Результат:** уровень проверяется без слоёв выше него.

## Результат этапа

Спека ядра не может незаметно опереться на `textsystem`, а спека `textsystem` — на `rh`. Подкаталоги `Tests/Lua/` строго отнесены к уровням тестирования (`Core`, `TextSystem`, `FullGame`, `FixtureCommands`), а необъявленные подкаталоги отвергаются.

## Задачи

- [x] **TSL-15 — Карта подкаталога в конфигурацию пакетов**
  - Зависимости: TSL-05.
  - Разбиение сегодня бинарно: фикстурная сессия задана литералом `{"commands"}` в `LuaSpecRunner.cpp`, остальное идёт на боевой сессии.
  - Done: подкаталог `Tests/Lua/` объявляет требуемую конфигурацию пакетов; конфигураций три — ядро (`core`), ядро плюс `textsystem` с образцом (`core` + `textsystem` + `sample`), полный игровой набор (`core` + `textsystem` + `rh`); карта живёт в одном месте (`GV2TestSupport`) и используется обоими хостами; подкаталог без объявленной конфигурации отвергается с кодом 16.
  - Evidence: `GV2TestSupport::ValidateAllSubtreesRegistered`, `GV2TestSupport::GetSubtreesForTier`, `Headless/Source/main.cpp`, `GV2LuaSpecRunnerHostTests.cpp`.

- [x] **TSL-16 — Расширить пакет-образец**
  - Зависимости: TSL-15.
  - Отдельный `sample_text_game` не заводится: два демонстрационных пакета разойдутся.
  - Done: существующий `sample` получает актора (`sample:actor.character.hero`), три локации со связностью (`sample:location.hub`, `sample:location.east`, `sample:location.west`), декларативные экраны, тексты и базовое перемещение без расхода выносливости, достаточные для проверки `textsystem` без `rh`; `sample` по-прежнему не входит в игровой набор по умолчанию и подключается конфигурацией сессии.
  - Evidence: `GameData/sample/definitions/`, `GameData/sample/scripts/authoring/gameplay.lua`, `gv2-content validate GameData`.

- [x] **TSL-17 — Разнести существующие спеки**
  - Зависимости: TSL-16.
  - Done: `actions`, `actors`, `events`, `lifecycle`, `resources`, `save` отнесены к Core tier; `world` отнесён к TextSystem tier; `authoring`, `economy`, `presentation` отнесены к FullGame tier; спеки, проверяющие правила `rh`, вынесены в `economy/` и не выполняются на конфигурациях без `rh`; проверено, что спека нижнего уровня при обращении к верхнему слою падает (`ActorDefinitionNotFound`).
  - Evidence: `Tests/Lua/economy/`, `Tests/Lua/world/`, `Tests/Lua/lifecycle/mutation_window.lua`, CTest 79/79 passed.

## Проверка milestone

- [x] Спеки ядра проходят без `textsystem` и `rh`.
- [x] Спеки `textsystem` проходят без `rh`.
- [x] Подкаталог без объявленной конфигурации отвергается (exit code 16).
- [x] Опора на слой выше своего уровня обнаруживается падением.
