---
title: Portable Content Core Repository Resolution Tasks
status: archived
version: 1.0
updated: 2026-08-13
depends_on:
  - SchemaValidation.md
  - ../../../Architecture/GameDataRepositoryContract.md
  - ../../../Architecture/Modding.md
decisions:
  - ../../../ADR/0006-repository-reload-and-session-pinning.md
  - ../../../ADR/0008-minimal-repository-indexes.md
---

# M4 — Repository Resolution

## Результат этапа

Упорядоченный provider set превращается в immutable repository snapshot. Test mod добавляет definition в собственном namespace и полностью переопределяет один core screen; invalid override блокирует весь candidate.

## Задачи

- [x] **PCC-21 — Реализовать package-local identity validation**
  - Зависимости: PCC-06, PCC-18.
  - Проверить duplicate ID, namespace ownership, foreign new ID и package-local source uniqueness.
  - Done: core/mod ownership cases покрыты positive и negative fixtures.
  - Evidence: `BuildRepository()` ownership pass; shared fixtures `valid/test_mod`, `invalid/foreign_new_id`; UE automation `GV2.Runtime.ContentCore.RepositoryResolutionM4`; CTest `gv2_headless_self_test`.

- [x] **PCC-22 — Реализовать full override selection**
  - Зависимости: PCC-21.
  - Последний provider по explicit `load_index` полностью заменяет definition, metadata и extensions.
  - Done: file/entry order не влияет на winner; deep merge отсутствует.
  - Evidence: `valid/core` + `valid/test_mod`; winner artifact assertions проверяют complete metadata replacement, canonical ID order и package/file/entry permutations в UE/headless.

- [x] **PCC-23 — Зафиксировать broken override semantics**
  - Зависимости: PCC-22.
  - Invalid winning provider обязан отклонить candidate без fallback к shadowed definition.
  - Done: test подтверждает отсутствие частичной публикации и скрытого восстановления core entry.
  - Evidence: shared fixture `invalid/broken_override`; оба host suites ожидают schema failure без candidate/fallback.

- [x] **PCC-24 — Реализовать typed reference resolution**
  - Зависимости: PCC-17, PCC-22.
  - Разрешить `ref`, `text_id` и `resource_ref` после выбора winners.
  - Done: missing target и wrong kind/class дают typed diagnostics; optional absent reference не создаётся; `data` и materialized `definition_entry` extensions используют один resolution path.
  - Evidence: recursive `ResolveReferences()` после winner selection; shared fixtures `invalid/missing_reference`, `invalid/resource_class_mismatch`; representative core проверяет valid `ref`, `text_id`, `resource_ref`; UE/headless extension tests проверяют missing target и redirect normalization.

- [x] **PCC-25 — Определить semantic validator interface**
  - Зависимости: PCC-24.
  - Validator получает read-only candidate view и исполняется в schema-declared stable order.
  - Done: interface не допускает Lua, I/O, mutation, locale/time dependency; один concrete vertical-slice validator подтверждает путь; конфликт IDs core/host registries является fatal.
  - Evidence: `ISemanticValidator`, `FSemanticCandidateView`, `FSemanticValidatorRegistry`; built-in `core:validator.item.positive_price`; shared fixture `invalid/semantic_failure`; UE/headless tests duplicate registry composition.

- [x] **PCC-26 — Реализовать redirects и tombstones**
  - Зависимости: PCC-21, PCC-24.
  - Проверить ownership, same-kind redirects, conflicts, chains и cycles.
  - Done: active definition не сосуществует с redirect source; resolution сохраняет final canonical target.
  - Evidence: package descriptor retirement tables; shared fixtures `invalid/redirect_cycle`, `invalid/active_redirect_source`; positive two-hop chain и tombstone в `valid/test_mod`; UE/headless M4 conformance.

- [x] **PCC-27 — Реализовать provenance**
  - Зависимости: PCC-22, PCC-26.
  - Хранить winner, source/span, schema identity, original/canonical ID, redirect chain и ordered shadowed providers.
  - Done: shadowed data не входит в active DTO; absolute paths отсутствуют.
  - Evidence: `FDefinitionProvenance`/`FProviderProvenance`; UE/headless tests проверяют winner `test_mod`, ordered shadowed `core`, package-relative source, non-default span, schema ID/version и redirect chain.

- [x] **PCC-28 — Построить minimal indexes**
  - Зависимости: PCC-24, PCC-27.
  - Реализовать только `ById`, `ByKind`, `ProvenanceById`, redirects/tombstones.
  - Done: новый generic index не добавлен; enumeration semantics явно проверены.
  - Evidence: private snapshot tables `ById`, `ByKind`, `ProvenanceById`, redirects/tombstones; `List("screen")` содержит только active IDs; дополнительных generic indexes нет.

- [x] **PCC-29 — Реализовать deterministic content hash**
  - Зависимости: PCC-27, PCC-28.
  - Hash включает normalized active content, schemas, provider identities/order и compatibility-relevant provenance.
  - Done: comments, whitespace, raw numeric spelling и file order не влияют на hash; active content, provider identity и retirement tables влияют на hash.
  - Evidence: canonical value encoding + SHA-256; UE/headless tests фиксируют expected representative hash, сравнивают repeated build, package/file/entry permutation, comments/object order и decimal/hex spelling, а также проверяют sensitivity к active/provider/retirement changes.

- [x] **PCC-30 — Реализовать immutable snapshot/read handle**
  - Зависимости: PCC-28, PCC-29.
  - API: typed `Find`, `Require`, `List`, `GetProvenance`, `GetContentHash`.
  - Done: query не выполняет parsing/I/O; views не переживают handle; snapshot нельзя изменить после freeze или сконструировать извне в обход builder.
  - Evidence: `FRepositorySnapshot`, `FRepositoryReadHandle`, typed `FDefinitionId`, structured `Require`; snapshot non-assignable и хранится как shared `const`; constructors закрыты compile-time assertions; handle lifetime test переживает candidate; invalid handle проверяется отдельно.

## Проверка milestone

- [x] Core + test mod дают ожидаемые winners, provenance и hash.
- [x] Broken override, broken reference и redirect cycle не создают snapshot.
- [x] File permutation и повторный single-thread build эквивалентны.
- [x] Snapshot предоставляет только contract-required indexes и read operations.
