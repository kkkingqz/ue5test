---
title: "ADR-0007: Lua Module Environment"
status: accepted
date: 2026-08-10
---

# ADR-0007: Lua Module Environment

> **Решение:** Одна VM/_G; no globals; `game.mods[mod_id]`.
> **Нормативный текст:** [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md).

## Context

Архитектура одновременно обещала per-mod environments и общий `_G`. Security sandbox не является целью v1.

## Decision

Одна VM и shared runtime `_G`. Modules не создают globals, используют lexical locals и export tables. Dependencies объявлены manifest-ом. Public mod extension хранится в `game.mods[mod_id]`. Per-module environments отсутствуют.

## Consequences

- Простая loader/debugger model.
- Конфликты уменьшаются convention/lint и explicit namespace.
- Это не hostile-code isolation.
- Runtime-owned global replacement — contract error.

## Rejected alternatives

- Per-module `_ENV`: дополнительная import/capability semantics без security guarantee.
- Unrestricted globals: load-order collisions и скрытые dependencies.

