---
title: Package Setup Tasks
status: draft
version: 1.0
updated: 2026-08-16
depends_on:
  - README.md
  - ../../Architecture/BuildAndTooling.md
---

# M1 — Package Setup

> **Материализует:** [Modding § Load order](../../Architecture/Modding.md), [Build and Tooling](../../Architecture/BuildAndTooling.md).
> **Задачи:** RH-01…04.
> **Результат:** пакет `rh` существует и загружается всеми хостами вместе с `core`.

## Результат этапа

Боевая сессия собирает репозиторий из двух пакетов. Этап завершается **при ещё пустом `rh`**: набор работает до того, как в него что-то переехало, поэтому любая поломка на M2 однозначно относится к переносу.

## Задачи

- [ ] **RH-01 — Создать пакет `rh`**
  - `GameData/rh/package.json5` объявляет `package_id: "rh"`, `namespace: "rh"`, `version` и зависимость от `core`; создаются пустые `definitions/`, `localization/`.
  - Done: `gv2-content validate` принимает пакет; пустой пакет не является ошибкой; `rh` не объявляет schema bindings — он пользуется схемами `core`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RH-02 — Перевести хосты на набор из двух пакетов**
  - Сегодня `Headless/Source/main.cpp` жёстко указывает `GameData/core`, `--content-root=` принимает одно значение, UE-provider настроен так же. Мульти-корневой путь (`DiscoverPackagesFromDirectories`, `FMultiPackageSourceProvider`) уже существует и используется только тестами.
  - Done: headless и UE-provider собирают набор `[core, rh]` в этом порядке; `--content-root=` принимает несколько значений либо каталог-контейнер пакетов, поведение задокументировано; одиночный корень остаётся рабочим частным случаем (его используют golden-прогон и фикстуры); ни один хост не собирает набор собственной логикой обхода каталогов.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RH-03 — Lock-файл и staging игрового набора**
  - Зависимости: RH-01, RH-02.
  - Done: для набора `[core, rh]` генерируется и проверяется `mods.lock.json5`; `GV2.Build.cs` стейджит `GameData/rh` наравне с `GameData/core` (сейчас правило описано под «real GameDataRepository core package»); `Resources/rh/` включён в staging по тому же правилу, что `Resources/core/`; packaged build находит оба пакета.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RH-04 — Зафиксировать зелёное состояние с пустым `rh`**
  - Зависимости: RH-01–RH-03.
  - Done: полный `ctest`, `gv2-headless --self-test`, `--check-scripts` и Unreal automation проходят с загруженным пустым `rh`; `repository_content_hash` боевого набора изменился осознанно и однократно, golden-прогон и его digest — нет; `mods.lock` воспроизводится побайтово.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Оба хоста и CLI грузят `GameData/core` и `GameData/rh` в этом порядке.
- [ ] Пустой `rh` не меняет наблюдаемого поведения игры.
- [ ] Golden-прогон и его digest не изменились.
- [ ] Packaged build содержит оба пакета.
