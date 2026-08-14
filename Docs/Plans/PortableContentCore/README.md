---
title: Portable Content Core Implementation Plan
status: draft
version: 0.1
updated: 2026-08-13
depends_on:
  - ../../Proposals/PortableContentCoreProposal.md
  - ../../Architecture/GameDataRepositoryContract.md
---

# План реализации PortableContentCore

## Цель

Реализовать один portable `GV2ContentCore`, который принимает уже разрешённый набор package descriptors и детерминированно возвращает либо immutable repository candidate, либо ordered diagnostics. Один source set обязан использоваться CMake и Unreal Build Tool.

План материализует [Portable Content Core Proposal](../../Proposals/PortableContentCoreProposal.md), но не расширяет его scope.

## Границы первого релиза

Входят:

- core-only package descriptor и позднее упорядоченный набор providers;
- bounded UTF-8 JSON5 parsing с source spans;
- Definition Envelope и declarative schema validation;
- минимальные schemas `location`, `item`, `screen`, а также необходимые identities `text` и `resource`;
- full override, typed references, provenance, minimal indexes и deterministic hash;
- immutable snapshot и CLI;
- одинаковое поведение CLI, Headless и UE.

Не входят:

- автоматическое обнаружение модов, dependency resolver, пользовательский load order и `mods.lock`;
- Lua gameplay, save/load и UI reconciliation;
- editor/LSP, formatting-preserving AST и auto-fix;
- parallel build до готовности single-thread reference path;
- trigger/effect DSL и native manager на каждый definition kind.

## Milestones

Milestone отмечается выполненным только после завершения всех задач соответствующего файла.

- [x] M1 — [Foundation](Foundation.md): portable target и публичная модель build result.
- [x] M2 — [JSON5 Parsing](Json5Parsing.md): deterministic bounded parser и source locations.
- [ ] M3 — [Schema Validation](SchemaValidation.md): core package проходит envelope/schema validation.
- [x] M4 — [Repository Resolution](RepositoryResolution.md): providers превращаются в immutable snapshot.
- [ ] M5 — [CLI and Host Integration](CliAndHostIntegration.md): CLI, Headless и UE дают одинаковый результат.

## Критический путь

```text
Foundation
→ JSON5 Parsing
→ Schema Validation
→ Repository Resolution
→ CLI and Host Integration
```

Первый полезный инкремент — завершённые M1–M3: библиотека способна полностью проверить один core package через fixtures. Первый пользовательский инструмент появляется с `PCC-31` — `gv2-content validate`.

## Общие правила выполнения

1. Задача реализуется через один reference path; временная вторая grammar или альтернативный validator запрещены.
2. Portable код не включает Unreal headers и не вызывает Lua API.
3. Новое observable behavior синхронно отражается в соответствующем contract.
4. Каждая задача добавляет positive и negative tests, если меняет validation или failure semantics.
5. Diagnostic assertions используют stable code и typed fields, а не human-readable `message`.
6. Все paths в portable diagnostics package-relative.
7. Один небольшой PR на задачу предпочтителен; соседние задачи размера S можно объединить, если результат остаётся атомарным.

## Итоговый Definition of Done

- [ ] `GV2ContentCore` собирается CMake и UBT из одного набора sources без Unreal dependency.
- [ ] Одинаковые inputs дают одинаковый normalized snapshot/hash либо одинаковые ordered diagnostics.
- [ ] Invalid candidate не публикуется и не изменяет current/pinned snapshot.
- [ ] Core package и test mod подтверждают full override, broken override и typed references.
- [ ] CLI, Headless и UE исполняют общие fixtures.
- [ ] Документация, examples и CI синхронизированы с реализацией.
