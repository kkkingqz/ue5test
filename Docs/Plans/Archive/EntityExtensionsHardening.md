---
title: EntityExtensionsHardening Archive Summary
status: archived
version: 1.0
updated: 2026-08-19
---

# EntityExtensionsHardening: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** План закрывает замечания ревизии реализации Entity Authoring Extensions. Механизм работает и покрыт спеками; проблема в том, что три его гарантии заявлены, но не обеспечены, а одна конвенция расходится внутри одного файла

**Результат:** заморозка реестра расширений становится необходимой, скомпонованная таблица методов — единственным путём поиска, повторное объявление — ошибкой, приём метода — строго `self`

## Этапы и задачи

### M1 — Registry Integrity

заморозку невозможно обойти, скомпонованная таблица участвует в разрешении метода, повторное объявление отклоняется всегда

- `EEH-01` — Уточнить ADR-0031
- `EEH-02` — Закрыть утечку изменяемых записей через фасад
- `EEH-03` — Разрешение метода через скомпонованную таблицу
- `EEH-04` — Повторное объявление внутри модуля — ошибка
- `EEH-05` — Изоляция спек не протекает

### M2 — Receiver Contract

метод сущности отвечает о получателе, а не о том, кого он нашёл в глобальном состоянии

- `EEH-06` — Убрать переход к игроку из `textsystem`
- `EEH-07` — Одна конвенция приёма на файл
- `EEH-08` — Сквозная верификация

## Актуальные нормативные источники

- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)
- [CanonicalStateAndSave](../../Architecture/CanonicalStateAndSave.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/EntityExtensionsHardening) содержит исходные task-файлы, acceptance criteria и evidence.
