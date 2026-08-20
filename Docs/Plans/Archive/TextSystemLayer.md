---
title: TextSystemLayer Archive Summary
status: archived
version: 1.0
updated: 2026-08-18
---

# TextSystemLayer: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Выделить между движком и игрой переиспользуемую основу текстовой игры: локации, переходы, презентацию экрана локации

**Результат:** три слоя вместо двух; локация и переход принадлежат `textsystem`, стоимость перехода — игре

## Этапы и задачи

### M1 — Package Set

пакет становится подключаемым из данных; третий слой существует и пуст

- `TSL-01` — Создать ADR по трёхуровневой границе
- `TSL-02` — Набор пакетов из данных в хостах
- `TSL-03` — Контейнерный режим `gv2-content`
- `TSL-04` — Пакет `textsystem`
- `TSL-05` — Зелёное состояние с пустым `textsystem`

### M2 — Location Ownership

локация и переход принадлежат `textsystem`, стоимость перехода — игре, ядро о переходах не знает

- `TSL-06` — Базовый актор и его локация
- `TSL-07` — Операция перехода
- `TSL-08` — Топология локаций
- `TSL-09` — Переход как команда игры
- `TSL-10` — Убрать переход из ядра

### M3 — Presentation

экран локации собирается `textsystem` из описания, а не Lua-строителем игры

- `TSL-11` — Семантические действия
- `TSL-12` — Презентер локации в `textsystem`
- `TSL-13` — Декларативные экраны
- `TSL-14` — Удалить презентер `rh`

### M4 — Test Tiers

уровень проверяется без слоёв выше него

- `TSL-15` — Карта подкаталога в конфигурацию пакетов
- `TSL-16` — Расширить пакет-образец
- `TSL-17` — Разнести существующие спеки

### M5 — RH Cleanup

в `rh` остаются только правила и сущности игры

- `TSL-18` — Убрать из `actors.lua` ответственность `textsystem`
- `TSL-19` — Свести дублирование ресурсов
- `TSL-20` — Синхронизировать документацию

## Актуальные нормативные источники

- [Overview](../../Architecture/Overview.md)
- [Modding](../../Architecture/Modding.md)
- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)
- [ScreenTemplates](../../UI/ScreenTemplates.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/TextSystemLayer) содержит исходные task-файлы, acceptance criteria и evidence.
