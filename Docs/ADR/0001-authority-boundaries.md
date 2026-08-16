---
title: "ADR-0001: Authority Boundaries"
status: accepted
date: 2026-08-10
---

# ADR-0001: Authority Boundaries

> **Решение:** Lua state / C++ boundary / UE presentation ownership.
> **Нормативный текст:** [Overview](../Architecture/Overview.md).

## Context

Data-driven gameplay требует избежать дублирования domain model между Lua, C++ и Blueprint и гарантировать save/load reconstruction.

## Decision

Lua владеет canonical gameplay-state и gameplay rules. C++ владеет lifecycle, repository/save infrastructure и typed boundary. UE/Blueprint владеет reconstructable presentation и local UX.

## Consequences

- Нет C++ Item/Quest/Location domain mirror.
- UE objects можно перестроить без gameplay mutation.
- Save содержит pure Lua-defined data, не engine/runtime objects.
- Ошибки trusted Lua могут повредить state и требуют controlled failure/restart.

## Rejected alternatives

- C++ authoritative domain + Lua scripting facade: дублирование и медленное расширение.
- Blueprint authoritative state: слабая deterministic/save boundary.

