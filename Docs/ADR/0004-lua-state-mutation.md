---
title: "ADR-0004: Lua State Mutation Policy"
status: superseded
date: 2026-08-10
superseded_by: 0027-designer-lua-authoring-layer.md
---

# ADR-0004: Lua State Mutation Policy

> **Решение:** Mutable table без proxies; mutation только command path.
> **Нормативный текст:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).
> **Заменено ([ADR-0027](0027-designer-lua-authoring-layer.md) DLA-02/DLA-03, 2026-08-18):** отказ от runtime-механизма пересмотрен. `game.state` отдаётся геймплею как guarded-прокси: запись вне активного обработчика команды отклоняется ошибкой `MutationWindowClosed`, а не ловится ревью, каждая запись увеличивает счётчик `write_revision` (на нём держится семантика `fail()` после мутации), и `unwrap_state` исключён из публичного экспорта, поэтому сырая таблица недостижима из модулей, доменных методов и сервисов. Отвергнутая здесь альтернатива «read-only proxies/per-module ownership guards» принята в части guard-а записи; per-module ownership по-прежнему отсутствует. Остальная часть решения в силе: единственный путь мутации — command path, universal rollback нет, валидация обязана предшествовать мутации.

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

