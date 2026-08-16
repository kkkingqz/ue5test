---
title: "ADR-0008: Minimal Repository Indexes"
status: accepted
date: 2026-08-10
---

# ADR-0008: Minimal Repository Indexes

> **Решение:** Только необходимые indexes v1.
> **Нормативный текст:** [GameDataRepository Contract](../Architecture/GameDataRepositoryContract.md).

## Context

Первоначальный GDR делал mandatory множество generic indexes до появления реальных query patterns.

## Decision

v1 repository обязательно хранит `ById`, `ByKind`, provenance и redirect/tombstone tables. Localization/resource/widget catalogs — отдельные derived outputs. Generic tag/group/scalar/back/forward indexes добавляются по конкретному measured use case.

## Consequences

- Меньше builder code, memory и invalidation surface.
- Deterministic lookup/enumeration остаётся.
- Некоторые ранние queries могут временно сортировать/scan небольшие collections.
- Новый index требует contract + benchmark, но не новую architecture.

## Rejected alternatives

- Универсальный index framework upfront: преждевременная сложность.
- Только raw map без kind/provenance: ухудшает typed API и diagnostics.

