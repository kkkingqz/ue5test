---
title: "ADR-0018: Portable Content Core Module"
status: accepted
date: 2026-08-13
---

# ADR-0018: Portable Content Core Module

## Context

Repository parsing, validation и resolution должны давать одинаковый результат в Unreal, headless и будущих CLI tools. Размещение этих правил в `GV2` или `GV2RuntimeCore` связывает Content с UE Presentation либо Lua VM и мешает independent testing.

## Decision

- `GV2ContentCore` является нижним portable C++ module для Stable ID, value tree, diagnostics, resolved package descriptors, repository builder и immutable candidate.
- `GV2RuntimeCore` зависит от `GV2ContentCore`; обратная dependency запрещена.
- CMake и UBT компилируют один набор implementation/public sources.
- Shared sources и public API не используют Unreal headers/types, Lua API и filesystem ownership.
- UBT module-bootstrap может отдельно зависеть от UE `Core`, но эта зависимость не входит в portable source set и CMake target.
- `BuildRepository()` является единственным reference repository-build entry point.
- `FCandidate` хранит explicit lifecycle stage. `SchemaValidated` и `ReferencesValidated` являются промежуточными non-publishable artifacts; только `RepositoryResolved` может пересечь publication boundary.

## Consequences

- Stable ID grammar доступна Content без зависимости от Lua runtime.
- UE/headless conformance проверяется одинаковыми inputs и typed results.
- Host отвечает за discovery и чтение source bytes; Content Core получает resolved values.
- Новые parsing/resolution stages добавляются в существующий reference path, а не создают host-specific builders.

## Rejected alternatives

- Разместить repository builder в `GV2`: создаёт UE dependency.
- Разместить Content внутри `GV2RuntimeCore`: связывает repository с Lua ownership.
- Поддерживать отдельные UE/headless реализации: создаёт semantic drift.
