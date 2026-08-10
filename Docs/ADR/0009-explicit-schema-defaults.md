---
title: "ADR-0009: Explicit Schema Defaults"
status: accepted
date: 2026-08-10
---

# ADR-0009: Explicit Schema Defaults

## Context

Автоматические defaults (`0`, empty array, first enum/union variant) смешивают absent и empty, а schema evolution может молча менять gameplay semantics.

## Decision

Required absent — error. Optional absent без explicit `default` остаётся absent. Default materialизуется только если schema задаёт его явно. `nullable` управляет explicit null отдельно.

## Consequences

- Нет скрытого выбора enum/union behavior.
- Absent, empty и null сохраняют разные смыслы.
- Schema чуть более многословна там, где default действительно нужен.
- Evolution safer и diagnostics понятнее.

## Rejected alternatives

- Built-in defaults по kind: компактно, но создаёт implicit domain decisions.
- Всегда materialize null: теряет absent/null distinction.

