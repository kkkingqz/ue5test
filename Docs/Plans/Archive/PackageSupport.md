---
title: PackageSupport Archive Summary
status: archived
version: 1.0
updated: 2026-08-16
---

# PackageSupport: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Сделать пакет полноценной единицей поставки: он несёт definitions, схемы, переводы, ресурсы **и Lua-код**, подключается в явном порядке, может заместить модуль ядра и получить замещённую реализацию, а его состав фиксируется в run digest и в сейве

**Результат:** пакет несёт контент и Lua-код, замещает модули ядра и попадает в digest и сейв

## Этапы и задачи

### M1 — Package Manifest

пакет объявляет свою identity, версию и совместимость документом, а не именем каталога

- `PKG-01` — Сделать манифест обязательным и владеющим identity
- `PKG-02` — Ввести диапазоны совместимости
- `PKG-03` — Ввести объявленные зависимости пакета
- `PKG-04` — Синхронизировать contract и tooling

### M2 — Discovery and Order

репозиторий собирается из набора пакетов в явном, воспроизводимом порядке

- `PKG-05` — Обнаружение набора корней
- `PKG-06` — Явный порядок и проверка зависимостей
- `PKG-07` — Lock-файл
- `PKG-08` — Перевести все три хоста на набор
- `PKG-09` — Фикстура реального мода и синхронизация документации

### M3 — Module Sealing

таблица экспорта неизменяема после инициализации, а замещаемость модуля объявлена явно

- `PKG-10` — Создать ADR по замещению модулей
- `PKG-11` — Заморозить таблицы экспорта
- `PKG-12` — Ввести объявление замещаемости
- `PKG-13` — Синхронизировать contract

### M4 — Modules from Packages

Lua приезжает из пакета, замещает модуль ядра и получает замещённую реализацию

- `PKG-14` — Обнаружение `scripts/` внутри пакета
- `PKG-15` — Атрибуция source пакетом
- `PKG-16` — Резолюция `module_id` по провайдерам
- `PKG-17` — `require_base()` и семантика цепочки
- `PKG-18` — Зависимости по объединению цепочки
- `PKG-19` — Фикстура замещения и синхронизация документации

### M5 — Determinism and Save

состав пакетов и набор скриптов входят в идентичность прогона и сейва

- `PKG-20` — `ScriptSetHash` в run manifest
- `PKG-21` — Состав пакетов в сейве
- `PKG-22` — Вывод цепочек в `--check-scripts`
- `PKG-23` — Cross-host parity и синхронизация документации

## Актуальные нормативные источники

- [Modding](../../Architecture/Modding.md)
- [BuildAndTooling](../../Architecture/BuildAndTooling.md)
- [LuaRuntimeContract](../../Architecture/LuaRuntimeContract.md)
- [CanonicalStateAndSave](../../Architecture/CanonicalStateAndSave.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/PackageSupport) содержит исходные task-файлы, acceptance criteria и evidence.
