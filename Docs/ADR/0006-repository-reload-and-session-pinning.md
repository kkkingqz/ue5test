---
title: "ADR-0006: Repository Reload and Session Pinning"
status: accepted
date: 2026-08-10
---

# ADR-0006: Repository Reload and Session Pinning

> **Решение:** Pinned snapshot; reload через restart.
> **Нормативный текст:** [GameDataRepository Contract](../Architecture/GameDataRepositoryContract.md).

## Context

Live publication и pinned session snapshot одновременно означали, что reload формально успешен, но active gameplay продолжает видеть old data. Safe live swap требует сложной compatibility/invalidation model.

## Decision

Application atomically publishes full current snapshot. Session pins one snapshot for entire lifetime. Любое content/schema/localization/resource reload применяется через controlled session restart. Hidden live handle swap и live compatibility gate отсутствуют.

## Consequences

- Deterministic definitions на протяжении session.
- Проще caches, runtime instances и UI reconstruction.
- Development reload медленнее, но predictable.
- Same-hash candidate может skip publication/restart.

## Rejected alternatives

- Per-session live swap: требует dependency/reference/cache invalidation protocol.
- Partial definition patch: нарушает whole-snapshot invariant.

