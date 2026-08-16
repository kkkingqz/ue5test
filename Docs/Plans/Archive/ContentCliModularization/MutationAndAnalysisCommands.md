---
title: Mutation and Analysis Commands Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - SupportComponents.md
  - ../../../Architecture/BuildAndTooling.md
decisions:
  - ../../../ADR/0023-stable-id-publication-freeze.md
---

# M3 — Mutation and Analysis Commands

## Результат этапа

Команды модификации контента (`new`, `rename`), полной валидации (`validate`) и анализа локализации (`coverage`) вынесены в отдельные модули в `Tools/Content/Source/Commands/`.

## Задачи

- [x] **CCM-06 — Модуляризация команд `new` и `rename`**
  - Описание: Создать `Tools/Content/Source/Commands/NewCommand.h`/`.cpp` и `RenameCommand.h`/`.cpp`. Перенести генерацию заготовок определений из схем и атомарное переименование ID с сохранением форматирования и проверкой `package.json5` freeze flags.
  - Done: Команды мутации вынесены в изолированные модули.
  - Evidence: Созданы `NewCommand.h`/`.cpp` и `RenameCommand.h`/`.cpp`; CTest (57/57 passed).

- [x] **CCM-07 — Модуляризация команд `validate` и `coverage`**
  - Описание: Создать `Tools/Content/Source/Commands/ValidateCommand.h`/`.cpp` (с поддержкой `--watch`) и `CoverageCommand.h`/`.cpp` (анализ покрытия PO-каталогов).
  - Done: Команды валидации и отчёта локализации вынесены в модули.
  - Evidence: Созданы `ValidateCommand.h`/`.cpp` и `CoverageCommand.h`/`.cpp`; CTest (57/57 passed).

## Проверка milestone

- [x] Команды `new` и `rename` корректно используют `Json5AstRewriter`.
- [x] Команда `validate --watch` корректно использует `DirectoryWatcher`.
- [x] Команда `coverage` сохраняет семантику информационного отчёта.
