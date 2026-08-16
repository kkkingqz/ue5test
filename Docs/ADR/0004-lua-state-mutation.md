---
title: "ADR-0004: Lua State Mutation Policy"
status: accepted
date: 2026-08-10
---

# ADR-0004: Lua State Mutation Policy

> **Решение:** Mutable table без proxies; mutation только command path.
> **Нормативный текст:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).

## Context

Lua table удобна для data-driven gameplay, но unrestricted mutation из modules/events разрушает command invariants. Runtime proxies/capabilities заметно усложняют код.

## Decision

`game.state` остаётся обычной mutable table без proxies. Нормативно mutation разрешена только active Command Handler через Gameplay Services. Event/technical/UI handlers enqueue command. Нарушение контролируется tests/review, не runtime capability system.

## Consequences

- Простая Lua implementation.
- Один понятный mutation path.
- Trusted code технически может нарушить rule; fault переводит session в Failed.
- Universal rollback отсутствует; validation обязана предшествовать mutation.

## Rejected alternatives

- Полностью свободная mutation: невозможно гарантировать command failure semantics.
- Read-only proxies/per-module ownership guards: сложнее debugging и mod authoring без security benefit v1.

