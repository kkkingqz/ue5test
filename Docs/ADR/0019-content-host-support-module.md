---
title: "ADR-0019: Content Host Support Module"
status: accepted
date: 2026-08-14
---

# ADR-0019: Content Host Support Module

## Context

`gv2-content`, `gv2-headless` and the UE host adapter each need to turn a
single filesystem package root directory into a `GV2ContentCore::FPackageDescriptor`
(scan `definitions/*.json5` + self-describing `schemas/*.json5`) before
calling `GV2ContentCore::BuildRepository()`. This logic was briefly
duplicated three times, then unified as `GV2ContentCore::DiscoverPackageFromDirectory` —
but that placed `std::filesystem`/`std::ifstream` calls directly in
`GV2ContentCore`'s public API, violating ADR-0018's decision that shared
sources and public API have no filesystem ownership, and contradicting
`GameDataRepositoryContract.md`'s "не выполняет discovery внутри portable
core". Reverting to per-host duplication would restore the drift risk the
unification was meant to fix, threatening PCC-38 cross-host parity.

## Decision

- `GV2ContentHostSupport` is a new portable library, sibling to
  `GV2ContentCore`, built by both CMake (`gv2_content_host_support`) and UBT.
- It owns `DiscoverPackageFromDirectory()` and any other filesystem-based
  package discovery shared across hosts. It freely uses `std::filesystem`.
- `GV2ContentHostSupport` depends on `GV2ContentCore` (for `FPackageDescriptor`,
  `FDiagnostic`); the dependency is one-directional. `GV2ContentCore` has no
  knowledge of `GV2ContentHostSupport` and remains unchanged by this ADR.
- `GV2ContentCore`'s public API and shared implementation sources continue to
  have zero filesystem ownership, exactly as ADR-0018 requires.
- `gv2-content`, `gv2-headless` and the `GV2` Unreal module (via
  `FGV2FilesystemContentSourceProvider`) link `GV2ContentHostSupport` and call
  its `DiscoverPackageFromDirectory()` instead of each host maintaining its
  own copy.

## Consequences

- ADR-0018 and `GameDataRepositoryContract.md` remain accurate without any
  edit: the portable core still never touches the filesystem.
- Discovery-convention changes (e.g. supporting a new schema-resource field)
  happen in one place and apply identically to all three hosts, preserving
  PCC-38 parity.
- One more physical module to build (CMake target + UBT module), following
  the same dual-build pattern already established for `GV2ContentCore` and
  `GV2RuntimeCore`.

## Rejected alternatives

- Keep `DiscoverPackageFromDirectory` inside `GV2ContentCore`: directly
  contradicts ADR-0018's filesystem-ownership prohibition; a
  code/documentation disagreement caught in review.
- Amend ADR-0018 to carve out a bounded discovery exception inside
  `GV2ContentCore`: weakens the exact invariant ADR-0018 protects and sets a
  precedent for further ad hoc exceptions.
- Revert to three independent per-host copies: restores duplication and
  cross-host drift risk that the original unification was meant to remove.
