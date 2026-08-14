---
title: GV2 Implementation Plans Index
status: normative
version: 0.1
updated: 2026-08-13
depends_on:
  - ../README.md
---

# Индекс планов реализации

`Plans` содержит исполняемые декомпозиции уже принятых направлений. План не меняет архитектурный контракт и не заменяет Proposal или ADR: он связывает ограниченные задачи, зависимости и evidence завершения.

## Правила ведения

- Checkbox задачи является единственным источником её статуса завершения.
- `[ ]` означает, что Definition of Done ещё не подтверждён; `[x]` — подтверждён полностью.
- Для текущей работы после названия можно временно добавить `— in progress`; для блокировки — `— blocked: <причина>`.
- Нельзя отмечать задачу выполненной только по наличию кода: должны пройти перечисленные tests и быть добавлены ссылки в поле `Evidence` либо в итоговый отчёт change set.
- При завершении всех задач этапа синхронно отмечается milestone в его локальном `README.md`.
- Изменение архитектурного инварианта по ходу задачи требует ADR и обновления contracts до отметки `[x]`.

## Активные планы

| План | Основание | Результат |
|---|---|---|
| [PortableContentCore](PortableContentCore/README.md) | [Portable Content Core Proposal](../Proposals/PortableContentCoreProposal.md) | Общий pipeline `Packages → Definitions → Immutable Repository Snapshot` для CLI, Headless и UE |
