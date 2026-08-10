---
title: "ADR-0005: Value-Only Async Boundary"
status: accepted
date: 2026-08-10
---

# ADR-0005: Value-Only Async Boundary

## Context

Один контракт требовал technical event DTO, другой передавал Lua callback function в C++ и хранил registry reference.

## Decision

Bridge принимает DTO и возвращает opaque `operation_id`. Completion проходит token/generation validation и bounded technical ingress queue как DTO. C++ не принимает/хранит Lua function. Lua может сопоставить operation ID с transient internal handler.

## Consequences

- C++/Lua boundary действительно value-only.
- Нет lifetime leak/stale Lua function reference в C++.
- Completion всегда non-reentrant и observable в одном ingress mechanism.
- Domain EventBus не смешивается с platform completions.

## Rejected alternatives

- Strong Lua registry callback в C++: конфликтует с DTO-only boundary и усложняет teardown.
- Превращать completion в gameplay event: событие не является committed gameplay fact.

