---
title: GameDataRepository Contract
status: normative
version: 2.8
updated: 2026-08-15
depends_on:
  - StableIDSpecification.md
  - DefinitionEnvelopeAndSchemaRules.md
decisions:
  - ../ADR/0006-repository-reload-and-session-pinning.md
  - ../ADR/0008-minimal-repository-indexes.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0018-portable-content-core-module.md
  - ../ADR/0022-external-translation-catalog.md
---

# GameDataRepository Contract

> **Владеет:** сборкой репозитория, разрешением провайдеров и override, redirects и tombstones, идентичностью снимка и API чтения.
> **Не владеет:** содержимым конкретных definitions и тем, как их использует геймплей.
> **Инварианты:** [INV-002](Invariants.md), [INV-010](Invariants.md), [INV-011](Invariants.md)
> **Реализация:** `Source/GV2ContentCore/Private/RepositoryBuilder.cpp`, `RepositorySnapshot.cpp`.
> **Проверки:** `RunPackageDescriptorConformance`, `RunLuaRepositoryConformance`, `GV2.Runtime.ContentCore.*`.

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

`package_id` и namespace обязаны соответствовать canonical `segment`; namespace package обязан равняться его immutable `package_id`. Definition/schema/extension-schema paths обязаны быть unique canonical package-relative paths без `.`/`..`, absolute prefix и platform separators. Exact schema binding содержит `definition_type`, positive int64 `schema_version`, `schema_id` и schema resource path. Exact extension binding дополнительно содержит supported `extension_site` и package-owned `extension_namespace`. Resolved descriptor также содержит manifest-authored redirects и tombstones. Resolved descriptor предоставляет только const access к этим значениям.

File enumeration и entry order не влияют на provider selection, normalized snapshot и hash. Для diagnostics источники сортируются по package index, canonical package-relative path и entry index.

### Portable build entry point

`GV2ContentCore::BuildRepository(package_set, build_options)` является единственным reference build path. Он возвращает `BuildResult`, содержащий либо complete immutable stage artifact, либо непустой ordered diagnostic list. `FCandidate::GetStage()` различает `SchemaValidated`, `ReferencesValidated` и `RepositoryResolved`; publication разрешена только когда `IsPublishable()` возвращает `true`. `build_options.source_provider` предоставляет immutable bytes по `(package_id, package-relative source)` и не выполняет discovery внутри portable core.

Builder обязан прочитать в deterministic order и разобрать все объявленные definition sources, schema и extension-schema resources, построить exact schema registries, проверить closed envelopes и package-local duplicate IDs, materialize typed values/defaults/extensions, проверить namespace ownership и выбрать full-override winners. Package может вводить новый definition ID только в собственном namespace; foreign ID допустим только как override ID, уже предоставленного более ранним provider. Каждый provider entry обязан быть самодостаточно valid до winner selection: ошибка более позднего override блокирует весь candidate и не открывает shadowed entry.

После выбора winners builder проверяет redirects/tombstones, разворачивает redirect chains до final active target и одним recursive path разрешает `ref`, `text_id`, `resource_ref` в `data` и materialized `definition_entry` extension blocks только против final winner set. Redirect source и tombstone не могут сосуществовать с active definition. Missing target, wrong target kind и несовпадающий `resource_class` являются fatal typed diagnostics. Materialized reference хранит final canonical target; extension diagnostic сохраняет identity соответствующей extension schema. Затем schema-declared semantic validators исполняются в порядке schema над read-only candidate view. `build_options.semantic_validator_registry` является non-owning host registry на время вызова; core validators имеют process lifetime. Пересечение validator IDs core и host registries, а также отсутствующий validator блокируют candidate.

Успешный полный M4 path создаёт immutable `FRepositorySnapshot`, строит только contract-required indexes/provenance/hash и возвращает publishable `RepositoryResolved` candidate. `SchemaValidated` и `ReferencesValidated` остаются lifecycle names для неполных внутренних stages, но текущий reference `BuildRepository()` не возвращает их при success. Stage tag без frozen snapshot не является publishable. Empty core descriptor создаёт допустимый пустой snapshot.

## Snapshot identity

Published snapshot имеет:

- process-local monotonic `repository_version`;
- deterministic `content_hash` normalized snapshot;
- `resolution_generation` provider set.

Same-hash candidate не публикуется и не увеличивает version. Failed/stale candidate не меняет current identity.

## Build pipeline

1. Ingest immutable resolved source manifest.
2. Parse UTF-8 JSON5 definitions/schemas.
3. Parse closed schema resources и register exact `(definition_type, schema_version)` bindings.
4. Validate envelopes и typed structures.
5. Materialize explicit defaults и normalize values.
6. Validate namespace ownership и select full-override winners.
7. Validate и flatten redirects/tombstones.
8. Resolve typed references against final winners.
9. Run deterministic semantic validators.
10. Build provenance, minimal indexes и content hash.
11. Freeze candidate.
12. On Game Thread validate token and atomically publish current.

Any error blocks publication. Invalid override does not reveal previous provider.

## Full override and provenance

Последний provider одного ID заменяет всю entry. Provenance хранит:

```text
winning package, source file/span, schema ID/version,
original/canonical ID, redirect chain, ordered shadowed providers
```

Shadowed data не входит в active DTO.

Package-local duplicate ID использует `core:diagnostic.definition.entry.duplicate_id`; новый foreign ID — `core:diagnostic.repository.identity.foreign_new_id`. Full override не выполняет deep merge `data`, metadata или extensions.

## Redirects and tombstones

- Redirect/tombstone объявляется resolved package descriptor-ом из manifest layer; отдельный definition kind не вводится. Файловая форма manifest layer — необязательный `package.json5` в корне пакета; её конвенция описана в [Build and Tooling](BuildAndTooling.md).
- Redirect source и tombstone ID принадлежат namespace declaring package. Target redirect может принадлежать другому namespace, но обязан иметь тот же kind.
- Один retired ID объявляется ровно один раз: redirect/redirect и redirect/tombstone conflicts fatal.
- Chain хранит original source, intermediate IDs и final active ID. Cycle, missing/tombstoned final target и active source conflict блокируют snapshot.
- Snapshot хранит flattened `source -> final canonical target`, но provenance сохраняет полную chain.
- Tombstone lookup не возвращает definition; `Require()` отличает tombstoned ID от обычного missing ID.

## Typed references and semantic validation

- `ref` разрешается по exact Stable ID и проверяет schema-declared `target_kind`; правило одинаково для основной schema и materialized `definition_entry` extension schema.
- `text_id` разрешается как reference kind `text` (по [ADR-0022](../ADR/0022-external-translation-catalog.md) репозиторий владеет идентичностью текста, а не переводами; definition kind `text` хранит обязательную непустую исходную строку `source_message`).
- `resource_ref` разрешается как kind `resource` и дополнительно проверяет `resource_class` winner definition.
- Optional absent field не создаёт reference и не диагностируется.
- Ошибки используют `core:diagnostic.reference.target_missing`, `core:diagnostic.reference.target_kind_mismatch` и `core:diagnostic.reference.resource_class_mismatch`.
- `ISemanticValidator` получает только immutable definition, `FSemanticCandidateView`, provenance context и diagnostic sink. Validator обязан быть deterministic и side-effect-free; Lua, I/O, mutation, locale и wall-clock time запрещены.
- Validator Stable ID обязан иметь kind `validator`; duplicate registration внутри registry, конфликт IDs между core/host registries и unavailable schema declaration являются fatal. `ListIds()` возвращает detached canonical-order list только для проверки composition; он не передаёт ownership validator instances.

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

`FRepositoryReadHandle::Find(FDefinitionId)` возвращает pointer-view либо empty; redirect source разрешается в final active definition. `Require(FDefinitionId)` возвращает `FRepositoryQueryResult` со structured `not_found`, `tombstoned` или `invalid_handle` error. `List(kind)` возвращает detached typed ID list только active definitions; redirects не перечисляются. `GetProvenance()` принимает active либо redirect ID. `GetContentHash()` возвращает 64 lowercase hex SHA-256. Public definition lookup принимает typed `FDefinitionId`, не interchangeable raw string.

`FRepositorySnapshot` после construction не имеет mutation API, не копируется и хранится handle-ами как shared `const`. Snapshot constructor доступен только `BuildRepository()`, а pinned-handle constructor — только `FCandidate`; внешний consumer не может собрать несогласованные indexes/hash в обход reference build path. Returned definition/provenance pointer-view действителен не дольше read handle. Query не выполняет parsing, I/O или hash computation.

Enumeration order C++ не является gameplay semantics. Caller задаёт explicit comparator. Lua API возвращает IDs в canonical byte order, если schema не объявляет другой explicit order.

## Lua API

```lua
local item, err = game.repository.get("rh:item.weapon.iron_sword")
local item = game.repository.require("rh:item.weapon.iron_sword")
local ids = game.repository.list("item")
local exists = game.repository.exists("rh:item.weapon.iron_sword")
```

- `get(id)` никогда не выбрасывает Lua error; при успехе возвращает `(definition_table, nil)`, при отсутствии/ошибке — `(nil, err_table)` со стабильным полем `code` (`not_found`, `tombstoned`, `invalid_id`, `invalid_handle`, `marshal_error`), `requested_id` и `canonical_id` (при наличии).
- `require(id)` возвращает `definition_table` либо выбрасывает Lua error, где стабильный `code` является первым токеном сообщения (`not_found: ...`, `tombstoned: ...`, `invalid_id: ...`, `invalid_handle: ...`).
- `list(kind)` возвращает массив идентификаторов в каноническом лексикографическом порядке (strict lowercase ASCII order); redirects не перечисляются; при неизвестном или невалидном `kind` возвращается `{}`.
- `exists(id)` возвращает boolean `true`/`false`.

Query result — detached deep Lua copy. Изменение copy разрешено, но не влияет на snapshot. Repeated `get` не гарантирует table identity. Таблица `game.repository` закрыта от модификации и расширения (`__newindex`, `__metatable = false`). Generic `query(index_id, key)` появляется только вместе с конкретным зарегистрированным index contract.

### Отсутствие provenance в Lua surface

Lua runtime изолирован от authoring metadata: `game.repository` предоставляет ровно 4 функции (`get`, `require`, `list`, `exists`). Возвращаемые значения не содержат сведений о пакетах, путях к исходным файлам, строках или истории затенения (`shadowed_providers`). Инспекция и аудит provenance осуществляются исключительно через CLI-утилиту `gv2-content inspect <definition_id>` и C++ API `FRepositoryReadHandle::GetProvenance()`.

## Threading and publication

I/O, parse, validation, indexes и freeze могут выполняться background workers при эквивалентности single-thread reference result. Current swap выполняется только на Game Thread.

Old snapshot живёт, пока существует read handle. Views не переживают свой handle. Candidate token включает resolution/operation generation; stale candidate discard не имеет side effects.

## Reload

Development reload всегда строит full candidate. Successful publish меняет Application current snapshot только для новых sessions.

Active session никогда не переключает pinned handle. Для применения любого content/schema/localization/resource change coordinator выполняет controlled session restart. Live compatibility gate и hidden per-session swap отсутствуют.

## Determinism and hash

Одинаковые source bytes, provider order, schemas и settings дают одинаковый normalized snapshot/hash либо одинаковый ordered diagnostic set.

Hash использует versioned canonical binary-like encoding portable values и SHA-256. Он включает active normalized definitions, canonical schema resource values, provider identities/order, package-relative winner/shadowed provenance без source spans, flattened redirects, full redirect provenance и tombstones. Object key/source entry order, comments, whitespace, timestamps, raw numeric spelling, absolute paths, pointer values и worker completion order не включаются. Array order остаётся значимым.

## Diagnostics

Diagnostic содержит severity, canonical Stable ID code вида `<namespace>:diagnostic.<path>`, package ID/load index, package-relative source, definition ID, JSON pointer/span, optional related span/message, schema ID/version и human message. Duplicate-key diagnostic использует основной span для повторного key и related span для первого объявления. Machine logic ветвится по code/typed fields. Comparator сортирует сначала по package load index, затем по canonical relative source, span/entry position и stable code; package ID и остальные typed fields служат deterministic tie-breakers.

Required categories: input, parsing, envelope/schema, identity, provider resolution, references, semantic/build, publication и read API.

## Conformance

M4 shared UE/headless suite покрывает package/entry/file order invariance, repeated build, duplicate-in-package, full/broken override, redirect ownership/kind/conflict/chain/cycle, active tombstone/source conflicts, typed references в `data` и extension blocks, core/host validator conflicts, provenance fields, minimal indexes, pinned canonical hash, hash sensitivity и pinned handle lifetime.

M5 CLI/headless/UE integration suite покрывает cross-host normalized snapshot/hash parity для одного corpus (`gv2-content validate|hash|inspect`, `gv2-headless`, `GV2.Runtime.ContentCore.CrossHostParity`), application publication same-hash skip и failed/non-publishable candidate rejection без изменения current (`GV2.Runtime.ContentCore.RepositoryPublisherAtomicPublication`) и pinned handle persistence активной session через unrelated republish и controlled restart (`GV2.Runtime.ContentCore.SessionRepositoryPinningAcrossRestart`). Repository build и publish выполняются synchronously на Game Thread; parallel worker equivalence и async candidate build с cancellable operation token остаются future work.

M6 Lua repository access suite покрывает `game.repository` (`get`/`require`/`list`/`exists`) поверх pinned snapshot: detached deep copy и отсутствие table identity между повторными query, canonical `list` order, error codes `not_found`/`tombstoned`/`invalid_id`/`invalid_handle`, отсутствие provenance/package/source полей в возвращаемых значениях и один marshaller для обоих portable value-типов (`GV2.Runtime.Lua.MarshallerConformance`, `GV2.Runtime.Lua.RepositoryAccess`, `GV2.Runtime.Lua.RepositoryConformanceCrossHost`, `GV2.Runtime.Session.PinnedHandleLifetime`, self-test в `gv2-headless`).
