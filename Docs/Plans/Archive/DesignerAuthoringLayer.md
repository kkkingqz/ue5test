---
title: DesignerAuthoringLayer Archive Summary
status: archived
version: 1.0
updated: 2026-08-18
---

# DesignerAuthoringLayer: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Дать геймдизайнеру Lua, на котором игровое правило выражается без Stable ID, реестров, конвертов команд и presentation DTO — и перевести на него текущий геймплей `rh`

**Результат:** геймплей `rh` написан на designer-facing Lua; фасад в теле команд не встречается

## Этапы и задачи

### M1 — Foundation

рантайм знает, была ли запись, сырое состояние недостижимо из геймплея, проверка переносимости одна на всех

- `DLA-01` — Создать ADR по designer-facing слою
- `DLA-02` — Ввести `write_revision` в окно мутации
- `DLA-03` — Изолировать сырое состояние
- `DLA-04` — Выделить общий примитив переносимого значения

### M2 — Commands

команда объявляется присваиванием в дескриптор, отказ типизирован, время исполнения видно в коде

- `DLA-05` — Дескриптор модуля и прокси команд
- `DLA-06` — Отложенная регистрация
- `DLA-07` — Канонизация аргументов и обратное превращение
- `DLA-08` — `fail()` с ключом и правилом мутации
- `DLA-09` — `run()` и `later()`

### M3 — Properties

поле знает, где хранится и как в него писать; ссылка знает, definition это или экземпляр

- `DLA-10` — Хранилище и политика записи в схеме
- `DLA-11` — Authoring-обёртка поля
- `DLA-12` — Managed-поля и штатные операции
- `DLA-13` — Два типа ссылок

### M4 — Runtime State

любой вид сущности получает runtime-поля без правок ядра; локация принадлежит персонажу

- `DLA-14` — Универсальная секция runtime-состояния
- `DLA-15` — Sparse-материализация
- `DLA-16` — Локация как свойство персонажа

### M5 — Events and Presentation

событие и экран описываются без конвертов и DTO, ни одной пользовательской строки в коде

- `DLA-17` — `emit` и `on`
- `DLA-18` — `action` и `button`
- `DLA-19` — `show_screen` и тексты
- `DLA-20` — Инструмент сбора текстов

### M6 — RH Migration

геймплей `rh` написан на designer-facing Lua, старый путь удалён

- `DLA-21` — Переписать команды `rh`
- `DLA-22` — Переписать presentation `rh`
- `DLA-23` — Удалить старый путь и синхронизировать документацию

## Актуальные нормативные источники

- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)
- [CanonicalStateAndSave](../../Architecture/CanonicalStateAndSave.md)
- [CommandsAndEvents](../../Architecture/CommandsAndEvents.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/DesignerAuthoringLayer) содержит исходные task-файлы, acceptance criteria и evidence.
