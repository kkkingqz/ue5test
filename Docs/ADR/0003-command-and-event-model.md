---
title: "ADR-0003: Command and Event Model"
status: accepted
date: 2026-08-10
---

# ADR-0003: Command and Event Model

> **Решение:** Commands + validators; EventBus post-commit only.
> **Нормативный текст:** [Commands and Events](../Architecture/CommandsAndEvents.md).

## Context

Ранние документы использовали gameplay action, отдельный UI Action Registry и cancellable `before_*` events в EventBus. Это создавало две semantics одного bus и лишнее mapping layer.

## Decision

Gameplay mutation запускается `command_id` через один Command Dispatcher. Ordered command validators выполняют preconditions/veto до mutation. EventBus публикует только non-cancellable post-commit facts. UI передаёт bound `command_id` напрямую; Action Registry отсутствует.

## Consequences

- Один mutation entry point и одна event semantics.
- Модовый veto остаётся доступен через validator.
- Event handler не может отменить committed fact.
- Local UE-only actions не пересекают gameplay boundary.

## Rejected alternatives

- Cancellable EventBus phase: сложнее ordering/error/re-entry model.
- Action Registry 1:1 mapping: дополнительный registry без demonstrated transformation need.

