---
title: ContentAuthoringTools Archive Summary
status: archived
version: 1.0
updated: 2026-08-15
---

# ContentAuthoringTools: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Сделать ручное создание контента ненужным там, где оно механическое, и дать автору обратную связь за секунды. План развивает CLI-часть Content Diagnostics and Tooling Proposal

**Результат:** Сделать ручное создание контента ненужным там, где оно механическое, и дать автору обратную связь за секунды. План развивает CLI-часть Content Diagnostics and Tooling Proposal

## Этапы и задачи

### M1 — Schema Driven Authoring

Автор получает справочник полей и валидную заготовку из тех же схем, которые использует рантайм. Ручное воспроизведение конверта и угадывание полей больше не нужны

- `CAT-01` — Реализовать `gv2-content describe`
- `CAT-02` — Реализовать `gv2-content new`
- `CAT-03` — Описать команды в контракте

### M2 — Script Feedback

Автор узнаёт за секунды, что его Lua-модуль загружается и объявлен в манифесте корректно, не запуская геймплей и не открывая UE

- `CAT-04` — Реализовать проверку module graph
- `CAT-05` — Сделать ошибку пригодной для автора
- `CAT-06` — Включить проверку в CI и контракт

### M3 — Rename Support

До заморозки идентификаторов переименование выполняется инструментом: автор видит все обратные ссылки и переписывает их одной командой, а не поиском по репозиторию

- `CAT-07` — Реализовать `gv2-content refs`
- `CAT-08` — Реализовать `gv2-content rename`
- `CAT-09` — Уточнить область инварианта неповторного использования
- `CAT-10` — Ограничить область применения инструмента

### M4 — Live Loop

Автор видит ошибку сразу после сохранения файла, а редактор подсказывает существующие идентификаторы вместо того, чтобы ждать запуска проверки

- `CAT-11` — Реализовать `validate --watch`
- `CAT-12` — Реализовать `gv2-content index`
- `CAT-13` — Собрать конфигурацию редактора
- `CAT-14` — Синхронизировать документацию

## Актуальные нормативные источники

- [BuildAndTooling](../../Architecture/BuildAndTooling.md)
- [DefinitionEnvelopeAndSchemaRules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md)
- [StableIDSpecification](../../Architecture/StableIDSpecification.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/ContentAuthoringTools) содержит исходные task-файлы, acceptance criteria и evidence.
