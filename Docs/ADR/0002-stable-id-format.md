---
title: "ADR-0002: Stable ID Format"
status: accepted
date: 2026-08-10
---

# ADR-0002: Stable ID Format

> **Решение:** `<namespace>:<kind>.<path>` и no reuse.
> **Нормативный текст:** [Stable ID Specification](../Architecture/StableIDSpecification.md).

## Context

Документы использовали две несовместимые формы: `namespace:kind.path` и `type.namespace.path`, а также разные case policies.

## Decision

Global Stable ID — `<namespace>:<kind>.<path>`. Parser strict: lowercase ASCII input не нормализуется. Namespace равен package/mod ID. Published ID не переиспользуется; rename использует owner-declared redirect.

## Consequences

- Все JSON5, Lua, saves, logs и UI examples используют одну форму.
- Kind визуально и машинно отделён от namespace.
- Нет dual parser/migration до первого public release.
- Authoring tooling может предложить fix, но runtime не угадывает intent.

## Rejected alternatives

- `<type>.<namespace>.<path>`: уже конфликтует с основной архитектурой и большим числом public examples.
- Поддержка обеих grammars: ambiguous parsing и постоянная compatibility burden.
- Silent lowercase/trim: скрывает authoring errors и создаёт collisions.

