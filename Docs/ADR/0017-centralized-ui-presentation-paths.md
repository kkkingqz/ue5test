---
title: "ADR-0017: Centralized UI Presentation Paths"
status: accepted
date: 2026-08-12
---

# ADR-0017: Centralized UI Presentation Paths

## Context

Reusable UI components shared theme and parts of the Text Pipeline, but composite Widgets could still resolve images, construct repeated children, submit Semantic Input or apply text through local mechanisms. A local improvement therefore did not necessarily reach every consumer and parallel presentation paths could drift.

## Decision

- Runtime-authored text, content images, dynamic collections, Screen Field values and Semantic Input each use exactly one registered presentation path.
- A leaf adapter owns the only allowed mutation of its underlying UMG primitive. Composite Widgets compose leaf adapters and may not implement a parallel resolver, renderer, collection factory or input ingress.
- Common policy is centralized by concern through services, interfaces and registries. One universal Widget base is not introduced because UMG primitives require different native parents.
- `TextSpec` is prepared by the Text Pipeline before renderer mutation. A renderer may declare capabilities such as interactive runs or scrolling, but may not change localization, markup, token or font rules.
- Runtime content images use `resource_id` through the Image Resource Catalog and the image leaf adapter. Direct brushes are allowed only for explicitly documented theme chrome that is not runtime content.
- Screen publication uses generic `field_id + schema_id + value` envelopes. Schema adapters prepare typed UE values; screen runtime code contains no concrete screen IDs or field names.
- Repeated items have deterministic keys and use a shared keyed collection lifecycle. Item-specific adapters own only typed item application.
- Every interactive component submits an opaque binding handle through one component-side Semantic Input emitter.
- Candidate fields, resources and bindings are validated and applied before the binding revision becomes current. Failure publishes neither partial Widgets nor partial bindings.

Explicit exceptions are limited to system recovery surfaces, development bootstrap fixtures and UE-local theme chrome. An exception must be named in the owning contract and still reuse common preparation services whenever they are available.

## Consequences

- Improvements to a presentation concern reach all conforming components without per-composite changes.
- Adding a Screen Template that uses existing field schemas requires no C++ change.
- Adding a new field schema requires one schema adapter and contract tests, but no per-screen branch.
- Central registries and prepared values may be cached by immutable document revision; discovery and reflection are not performed per frame.
- UI asset audits can reject direct primitives and calls outside approved leaf adapters.

## Rejected alternatives

- **One base Widget for every component.** Rejected because CommonUI/UMG primitives have incompatible native inheritance requirements.
- **Convention without enforcement.** Rejected because existing text, image and input paths already drifted while individually appearing valid.
- **Lua-authored physical Widget tree.** Rejected by ADR-0011; it would transfer layout ownership from UE to gameplay code.
