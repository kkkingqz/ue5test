---
title: Package Set Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - README.md
  - ../../../Architecture/Modding.md
---

# M1 — Package Set

> **Материализует:** [Modding § Load order](../../../Architecture/Modding.md) и [Build and Tooling](../../../Architecture/BuildAndTooling.md).
> **Задачи:** TSL-01…05.
> **Результат:** пакет становится подключаемым из данных; третий слой существует и пуст.

## Результат этапа

Ни один хост не знает имён пакетов. Пустой `textsystem` загружается наравне с остальными, и наблюдаемое поведение игры не меняется.

Этап обязателен для всего остального: пока набор зашит в C++, третий слой не может появиться иначе как правкой движка.

## Задачи

- [x] **TSL-01 — Создать ADR по трёхуровневой границе**
  - Done: ADR фиксирует три уровня и критерий выбора из трёх вопросов, расширяющий бинарный критерий [ADR-0026](../../../ADR/0026-core-and-gameplay-ownership.md); **отдельно записывает, что это осознанное исключение** из правила «применимость доказывается второй игрой» — второй текстовой игры нет, а основанием служит цена момента, а не доказанная применимость; фиксирует, что набор пакетов собирается из данных; перечисляет отвергнутое — Quest foundation без потребителей, Inventory в `textsystem`, отдельная иерархия классов Lua; принят до первой отметки `[x]` ниже.
  - Evidence: `Docs/ADR/0030-textsystem-layer-and-data-driven-package-set.md`, `Docs/ADR/README.md`.

- [x] **TSL-02 — Набор пакетов из данных в хостах**
  - Зависимости: TSL-01.
  - `Headless/Source/main.cpp` и `GV2RuntimeSubsystem.cpp` перечисляют `core` и `rh` по именам; пакет `sample` в набор по умолчанию не попадает.
  - Done: набор собирается обходом контейнерного каталога и упорядочивается по объявленным зависимостям пакетов либо по `mods.lock.json5`; ни один хост не содержит имён пакетов; порядок обхода файловой системы не влияет на результат; отсутствие обязательного `core` — типизированная диагностика; UE-хост стейджит контейнер целиком, а не перечисленные каталоги.
  - Evidence: `GV2ContentHostSupport::DiscoverPackagesFromContainer`, `IsContainerDirectory`, `Headless/Source/main.cpp`, `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, `Source/GV2/Private/Tests/GV2LuaSpecRunnerHostTests.cpp`, conformance test cases 8..11 в `PackageDiscoveryAndOrderConformance.cpp`.

- [x] **TSL-03 — Контейнерный режим `gv2-content`**
  - Зависимости: TSL-02.
  - `validate GameData` сегодня даёт `package.json5 package root has no package.json5`: инструмент принимает только корень пакета.
  - Done: команды принимают контейнерный каталог наравне с корнем пакета и списком корней; поведение одинаково для `validate`, `index`, `hash` и `coverage`; каталог без единого пакета — типизированный отказ; CTest покрывает контейнерную форму на замороженном корпусе.
  - Evidence: `DiscoverPackageSet` в `PackageLoader.cpp`, тесты 41a..41g в `test_authoring_tools.py`, CTest-кейсы `gv2_content_validate_gamedata_container`, `gv2_content_hash_gamedata_container`, `gv2_content_index_gamedata_container`, `gv2_content_coverage_gamedata_container`.

- [x] **TSL-04 — Пакет `textsystem`**
  - Зависимости: TSL-02.
  - Done: `GameData/textsystem` с манифестом, зависимостью от `core` и пустыми `definitions/`, `schemas/`, `scripts/`; `rh` объявляет зависимость от `textsystem` вместо `core`; порядок загрузки — `core`, `textsystem`, `rh`; `mods.lock.json5` содержит три пакета.
  - Evidence: `GameData/textsystem/package.json5`, `GameData/rh/package.json5`, `GameData/mods.lock.json5`.

- [x] **TSL-05 — Зелёное состояние с пустым `textsystem`**
  - Зависимости: TSL-03, TSL-04.
  - Done: полный `ctest`, `gv2-headless --self-test`, `--check-scripts` и Unreal automation проходят с загруженным пустым `textsystem`; `repository_content_hash` игрового набора изменился осознанно и однократно; замороженный корпус и его pinned-значения не тронуты; `mods.lock` воспроизводится побайтово.
  - Evidence: CTest 79/79 passed, `gv2-headless --self-test`, `gv2-headless --check-scripts` passed.

## Проверка milestone

- [x] В C++ не осталось имён пакетов.
- [x] Пустой `textsystem` не меняет наблюдаемого поведения игры.
- [x] `gv2-content` принимает контейнерный каталог.
- [x] Замороженный корпус и golden не изменились.
