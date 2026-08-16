---
title: Package Setup Tasks
status: archived
version: 1.0
updated: 2026-08-16
depends_on:
  - README.md
  - ../../../Architecture/BuildAndTooling.md
---

# M1 — Package Setup

> **Материализует:** [Modding § Load order](../../../Architecture/Modding.md), [Build and Tooling](../../../Architecture/BuildAndTooling.md).
> **Задачи:** RH-01…04.
> **Результат:** пакет `rh` существует и загружается всеми хостами вместе с `core`.

## Результат этапа

Боевая сессия собирает репозиторий из двух пакетов. Этап завершается **при ещё пустом `rh`**: набор работает до того, как в него что-то переехало, поэтому любая поломка на M2 однозначно относится к переносу.

## Задачи

- [x] **RH-01 — Создать пакет `rh`**
  - `GameData/rh/package.json5` объявляет `package_id: "rh"`, `namespace: "rh"`, `version` и зависимость от `core`; создаются пустые `definitions/`, `localization/`.
  - Done: `gv2-content validate` принимает пакет; пустой пакет не является ошибкой; `rh` не объявляет schema bindings — он пользуется схемами `core`.
  - Evidence: `GameData/rh/package.json5`, `gv2-content validate GameData/core GameData/rh` возвращает ok (`5c69a802c5f82a876fca79fe0883555c44927c63e4af3a1e2cba3b14d056d3ca`).

- [x] **RH-02 — Перевести хосты на набор из двух пакетов**
  - Сегодня `Headless/Source/main.cpp` жёстко указывает `GameData/core`, `--content-root=` принимает одно значение, UE-provider настроен так же. Мульти-корневой путь (`DiscoverPackagesFromDirectories`, `FMultiPackageSourceProvider`) уже существует и используется только тестами.
  - Done: headless и UE-provider собирают набор `[core, rh]` в этом порядке; `--content-root=` принимает несколько значений либо каталог-контейнер пакетов, поведение задокументировано; одиночный корень остаётся рабочим частным случаем (его используют golden-прогон и фикстуры); ни один хост не собирает набор собственной логикой обхода каталогов.
  - Evidence: `Headless/Source/main.cpp` (`LoadContentRoots`, `LoadRuntimeSources`), `GV2RuntimeSubsystem.cpp` (`DefaultRepositoryPackageRoots`), `GV2LuaSpecRunnerHostTests.cpp`, `BuildAndTooling.md`.

- [x] **RH-03 — Lock-файл и staging игрового набора**
  - Зависимости: RH-01, RH-02.
  - Done: для набора `[core, rh]` генерируется и проверяется `mods.lock.json5`; `GV2.Build.cs` стейджит `GameData/rh` наравне с `GameData/core` (сейчас правило описано под «real GameDataRepository core package»); `Resources/rh/` включён в staging по тому же правилу, что `Resources/core/`; packaged build находит оба пакета.
  - Evidence: `GameData/mods.lock.json5`, `GV2.Build.cs`.

- [x] **RH-04 — Зафиксировать зелёное состояние с пустым `rh`**
  - Зависимости: RH-01–RH-03.
  - Done: полный `ctest`, `gv2-headless --self-test`, `--check-scripts` и Unreal automation проходят с загруженным пустым `rh`; `repository_content_hash` боевого набора изменился осознанно и однократно, golden-прогон и его digest — нет; `mods.lock` воспроизводится побайтово.
  - Evidence: 57/57 CTest пройдены, `gv2-headless --self-test` 1002 commands success, golden run digest `golden_headless_10_seed_42.digest.json5` неизменен.

## Проверка milestone

- [x] Оба хоста и CLI грузят `GameData/core` и `GameData/rh` в этом порядке.
- [x] Пустой `rh` не меняет наблюдаемого поведения игры.
- [x] Golden-прогон и его digest не изменились.
- [x] Packaged build содержит оба пакета.
