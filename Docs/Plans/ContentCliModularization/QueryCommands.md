---
title: Query Commands Tasks
status: draft
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - SupportComponents.md
  - ../../Architecture/BuildAndTooling.md
---

# M2 — Query Commands

## Результат этапа

Инспекционные и справочные команды (`inspect`, `describe`, `refs`, `index`, `hash`) вынесены в отдельные файлы в `Tools/Content/Source/Commands/`.

## Задачи

- [x] **CCM-04 — Модуляризация команд `inspect` и `describe`**
  - Описание: Создать `Tools/Content/Source/Commands/InspectCommand.h`/`.cpp` и `DescribeCommand.h`/`.cpp`. Перенести логику отображения определений, source coordinates (`--provenance`) и динамического справочника полей схем.
  - Done: Команды изолированы в модули.
  - Evidence: Созданы `InspectCommand.h`/`.cpp` и `DescribeCommand.h`/`.cpp`; CTest (57/57 passed).

- [x] **CCM-05 — Модуляризация команд `refs`, `index`, `hash`**
  - Описание: Создать `Tools/Content/Source/Commands/RefsCommand.h`/`.cpp`, `IndexCommand.h`/`.cpp` и `HashCommand.h`/`.cpp`. Перенести поиск обратных ссылок по JSON Pointer, экспорт канонического индекса и вычисление `content_hash`.
  - Done: Команды изолированы в модули.
  - Evidence: Созданы `RefsCommand.h`/`.cpp`, `IndexCommand.h`/`.cpp`, `HashCommand.h`/`.cpp` и `Support/PackageLoader.h`/`.cpp`; CTest (57/57 passed).

## Проверка milestone

- [x] Все 5 query-команд компилируются как отдельные единицы трансляции.
- [x] Формат вывода текстового и JSON режима идентичен существующему.
