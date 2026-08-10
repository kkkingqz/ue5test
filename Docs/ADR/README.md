---
title: Architecture Decision Records
status: normative
version: 1.0
updated: 2026-08-10
---

# Architecture Decision Records

Accepted ADR фиксирует решение и причины. Контракты содержат актуальное полное правило; ADR объясняет выбор. Изменение accepted решения создаёт новый ADR и помечает старый `superseded`.

| ADR | Status | Decision |
|---|---|---|
| [0000](0000-markdown-documentation-authority.md) | accepted | Markdown в `Docs` — единственный нормативный набор |
| [0001](0001-authority-boundaries.md) | accepted | Lua state / C++ boundary / UE presentation ownership |
| [0002](0002-stable-id-format.md) | accepted | `<namespace>:<kind>.<path>` и no reuse |
| [0003](0003-command-and-event-model.md) | accepted | Commands + validators; EventBus post-commit only |
| [0004](0004-lua-state-mutation.md) | accepted | Mutable table без proxies; mutation только command path |
| [0005](0005-value-only-async-boundary.md) | accepted | DTO ingress; C++ не хранит Lua callbacks |
| [0006](0006-repository-reload-and-session-pinning.md) | accepted | Pinned snapshot; reload через restart |
| [0007](0007-lua-module-environment.md) | accepted | Одна VM/_G; no globals; `game.mods[mod_id]` |
| [0008](0008-minimal-repository-indexes.md) | accepted | Только необходимые indexes v1 |
| [0009](0009-explicit-schema-defaults.md) | accepted | Optional absent остаётся absent без explicit default |
| [0010](0010-portable-runtime-and-headless-simulation.md) | accepted | Portable gameplay runtime, UE/headless hosts и общий Command Dispatcher path |

## Template

```markdown
---
title: ADR-NNNN: Title
status: proposed | accepted | superseded | rejected
date: YYYY-MM-DD
---

# ADR-NNNN: Title
## Context
## Decision
## Consequences
## Rejected alternatives
```
