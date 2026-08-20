---
title: LocalizationPipeline Archive Summary
status: archived
version: 1.0
updated: 2026-08-15
---

# LocalizationPipeline: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Развести идентичность текста и его содержимое: репозиторий владеет `text_id`, внешний PO-каталог владеет переводами, резолвинг принадлежит host-у. План материализует

**Результат:** Развести идентичность текста и его содержимое: репозиторий владеет `text_id`, внешний PO-каталог владеет переводами, резолвинг принадлежит host-у. План материализует

## Этапы и задачи

### M1 — Text Identity

Definition kind `text` остаётся реестром идентификаторов и хранит исходную строку, но перестаёт быть местом хранения переводов

- `LOC-01` — Изменить схему `text`
- `LOC-02` — Зафиксировать назначение исходной строки

### M2 — Catalog Format

Переводы лежат в PO-каталогах внутри package root, ключуются `text_id` и не влияют ни на discovery пакета, ни на его хэш

- `LOC-03` — Определить размещение и формат каталога
- `LOC-04` — Реализовать чтение PO

### M3 — Host Resolution

UE разрешает `TextSpec` в отображаемый текст выбранной locale. Headless и Lua переводов не видят

- `LOC-05` — Собрать host-артефакт из PO
- `LOC-06` — Разрешать `TextSpec` в Presentation
- `LOC-07` — Зафиксировать fallback

### M4 — Coverage Tooling

Полнота перевода видна отчётом и не является условием валидности контента

- `LOC-08` — Реализовать отчёт о полноте
- `LOC-09` — Определить участие в CI
- `LOC-10` — Синхронизировать документацию

## Актуальные нормативные источники

- [PresentationSnapshotAndEffects](../../UI/PresentationSnapshotAndEffects.md)
- [WidgetRegistry](../../UI/WidgetRegistry.md)
- [DefinitionEnvelopeAndSchemaRules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/LocalizationPipeline) содержит исходные task-файлы, acceptance criteria и evidence.
