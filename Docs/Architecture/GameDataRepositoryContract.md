---
title: GameDataRepository Contract
status: normative
version: 1.1
updated: 2026-08-10
depends_on:
  - StableIDSpecification.md
  - DefinitionEnvelopeAndSchemaRules.md
decisions:
  - ../ADR/0006-repository-reload-and-session-pinning.md
  - ../ADR/0008-minimal-repository-indexes.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
---

# GameDataRepository Contract

GameDataRepository — единственный runtime-источник immutable static definitions. Candidate либо публикуется целиком одной atomic operation, либо не становится observable.

## Ownership

- Repository service принадлежит Application lifetime.
- Package Loader предоставляет уже resolved immutable provider set.
- Menu/Game session получает pinned read handle при bootstrap.
- Query не выполняет I/O, parsing, asset loading, migration, gameplay command или EventBus publish.
- Mutable gameplay state остаётся в Lua.

## Input provider set

Каждый provider содержит:

```text
package_id, namespace, load_index, resolved source,
definition files, schema files, localization/resources
```

Core имеет `package_id=core`, `load_index=0`. Enabled mods следуют в resolved order. Duplicate `package_id`/load index и missing core дают `PackageSetInvalid`.

File enumeration и entry order не влияют на provider selection, normalized snapshot и hash. Для diagnostics источники сортируются по package index, canonical package-relative path и entry index.

## Snapshot identity

Published snapshot имеет:

- process-local monotonic `repository_version`;
- deterministic `content_hash` normalized snapshot;
- `resolution_generation` provider set.

Same-hash candidate не публикуется и не увеличивает version. Failed/stale candidate не меняет current identity.

## Build pipeline

1. Ingest immutable resolved source manifest.
2. Parse UTF-8 JSON5 definitions/schemas.
3. Register exact `(kind, schema_version)` bindings.
4. Validate envelopes и typed structures.
5. Materialize explicit defaults и normalize values.
6. Validate namespace ownership и select full-override winners.
7. Apply redirects/tombstones.
8. Run semantic/reference/text/resource validation.
9. Build minimal indexes и content hash.
10. Freeze candidate.
11. On Game Thread validate token and atomically publish current.

Any error blocks publication. Invalid override does not reveal previous provider.

## Full override and provenance

Последний provider одного ID заменяет всю entry. Provenance хранит:

```text
winning package, source file/span, schema ID/version,
original/canonical ID, redirect chain, ordered shadowed providers
```

Shadowed data не входит в active DTO.

## Minimal storage/indexes

Required structures v1:

| Structure | Purpose |
|---|---|
| `ById` | Canonical typed lookup |
| `ByKind` | Enumerate definitions одного kind |
| `ProvenanceById` | Diagnostics и authoring inspection |
| `Redirects`/`Tombstones` | Compatibility resolution |

Localization catalog, resource mapping и UI/widget catalogs являются отдельными derived immutable outputs. Их logical entries одинаковы для UE и headless hosts, но physical output host-specific: UE resource mapping может ссылаться на cooked assets, headless catalog хранит только metadata и availability policy. Ни один portable DTO не содержит raw UE asset path. Generic grouping/tag/scalar/forward-reference indexes не обязательны. Новый index добавляется только под измеренный query и не меняет canonical storage.

## C++ read semantics

`Find<T>` возвращает empty при valid missing ID. `Require<T>` возвращает structured error. Public API принимает typed Stable ID wrappers и read handle, не interchangeable raw string.

Enumeration order C++ не является gameplay semantics. Caller задаёт explicit comparator. Lua API возвращает IDs в canonical byte order, если schema не объявляет другой explicit order.

## Lua API

```lua
local item, err = game.repository.get("core:item.weapon.iron_sword")
local item = game.repository.require("core:item.weapon.iron_sword")
local ids = game.repository.list("item")
local exists = game.repository.exists("core:item.weapon.iron_sword")
```

Query result — detached deep Lua copy. Изменение copy разрешено, но не влияет на snapshot. Repeated `get` не гарантирует table identity. Generic `query(index_id, key)` появляется только вместе с конкретным зарегистрированным index contract.

## Threading and publication

I/O, parse, validation, indexes и freeze могут выполняться background workers при эквивалентности single-thread reference result. Current swap выполняется только на Game Thread.

Old snapshot живёт, пока существует read handle. Views не переживают свой handle. Candidate token включает resolution/operation generation; stale candidate discard не имеет side effects.

## Reload

Development reload всегда строит full candidate. Successful publish меняет Application current snapshot только для новых sessions.

Active session никогда не переключает pinned handle. Для применения любого content/schema/localization/resource change coordinator выполняет controlled session restart. Live compatibility gate и hidden per-session swap отсутствуют.

## Determinism and hash

Одинаковые source bytes, provider order, schemas и settings дают одинаковый normalized snapshot/hash либо одинаковый ordered diagnostic set.

Hash включает active normalized definitions, canonical IDs, schema identities, provider identities/order и compatibility-relevant provenance. Comments, whitespace, timestamps, raw numeric spelling, pointer values и worker completion order не включаются.

## Diagnostics

Diagnostic содержит severity, stable code, package, package-relative source, definition ID, JSON pointer/span, schema ID/version и human message. Machine logic ветвится по code/typed fields.

Required categories: input, parsing, envelope/schema, identity, provider resolution, references, semantic/build, publication и read API.

## Conformance

Tests покрывают same-input determinism, parallel equivalence, file permutation, duplicate-in-package, full override, broken override, redirects/cycles/tombstones, typed references, minimal indexes, C++ explicit order, Lua canonical order, detached copies, pinned handles, same-hash skip, stale token, old snapshot lifetime, restart reload и no partial publication.
