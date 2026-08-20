---
title: PortableContentCore Archive Summary
status: archived
version: 1.0
updated: 2026-08-14
---

# PortableContentCore: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Реализовать один portable `GV2ContentCore`, который принимает уже разрешённый набор package descriptors и детерминированно возвращает либо immutable repository candidate, либо ordered diagnostics. Один source set обязан использоваться CMake и Unreal Build Tool

**Результат:** Реализовать один portable `GV2ContentCore`, который принимает уже разрешённый набор package descriptors и детерминированно возвращает либо immutable repository candidate, либо ordered diagnostics. Один source set обязан использоваться CMake и Unreal Build Tool

## Этапы и задачи

### M1 — Foundation

Пустой core package проходит через `BuildRepository()` и создаёт пустой immutable candidate. Shared implementation target собирается CMake и UBT без Unreal headers, Lua API и filesystem ownership внутри core. Тонкая UBT module-bootstrap translation unit может иметь private `Core` dependency согласно System Context and Components

- `PCC-01` — Зафиксировать MVP fixtures
- `PCC-02` — Создать target `GV2ContentCore`
- `PCC-03` — Добавить portable value model
- `PCC-04` — Добавить diagnostic model
- `PCC-05` — Определить `BuildResult` API
- `PCC-06` — Определить resolved package descriptor

### M2 — JSON5 Parsing

Portable parser преобразует bounded JSON5 source в value tree с точными source spans либо возвращает deterministic ordered diagnostics. Parsed source model существует только во время candidate build

- `PCC-07` — Реализовать UTF-8 input и parser limits
- `PCC-08` — Реализовать JSON5 lexer
- `PCC-09` — Реализовать JSON5 parser
- `PCC-10` — Добавить duplicate-key detection
- `PCC-11` — Реализовать numeric normalization
- `PCC-12` — Добавить parser conformance suite

### M3 — Definition Envelope и Schema Validation

Один core package с `location`, двумя `screen` и `item` definitions проходит полную envelope/schema validation. Invalid package возвращает точные diagnostics и не создаёт candidate

- `PCC-13` — Реализовать schema resource parser и registry
- `PCC-14` — Реализовать scalar validation
- `PCC-15` — Реализовать container validation
- `PCC-16` — Реализовать presence, null и explicit defaults
- `PCC-17` — Реализовать special field kinds
- `PCC-18` — Реализовать Definition file envelope
- `PCC-19` — Реализовать extension block validation
- `PCC-20` — Добавить минимальные core schemas

### M4 — Repository Resolution

Упорядоченный provider set превращается в immutable repository snapshot. Test mod добавляет definition в собственном namespace и полностью переопределяет один core screen; invalid override блокирует весь candidate

- `PCC-21` — Реализовать package-local identity validation
- `PCC-22` — Реализовать full override selection
- `PCC-23` — Зафиксировать broken override semantics
- `PCC-24` — Реализовать typed reference resolution
- `PCC-25` — Определить semantic validator interface
- `PCC-26` — Реализовать redirects и tombstones
- `PCC-27` — Реализовать provenance
- `PCC-28` — Построить minimal indexes
- `PCC-29` — Реализовать deterministic content hash
- `PCC-30` — Реализовать immutable snapshot/read handle

### M5 — CLI и интеграция host-ов

CLI, Headless и UE используют один `GV2ContentCore` и общие fixtures. Из одинаковых inputs они получают одинаковый snapshot/hash либо одинаковый ordered diagnostic set. Публикация candidate остаётся обязанностью host Application

- `PCC-31` — Реализовать `gv2-content validate`
- `PCC-32` — Реализовать `gv2-content inspect`
- `PCC-33` — Реализовать `gv2-content hash`
- `PCC-34` — Зафиксировать stable CLI exit codes
- `PCC-35` — Интегрировать repository с Headless
- `PCC-36` — Интегрировать repository с Unreal host
- `PCC-37` — Реализовать atomic publication
- `PCC-38` — Добавить cross-host parity tests

### M6 — Lua Repository Access

Lua читает definitions из pinned immutable snapshot через `game.repository`. Один marshaller обслуживает оба portable value-типа. Headless и UE получают одинаковые значения, одинаковый порядок и одинаковые typed errors из одного corpus

- `PCC-39` — Выделить `FGV2LuaMarshaller`
- `PCC-40` — Согласовать value limits parser и marshaller
- `PCC-41` — Передать pinned read handle в `FRuntimeSession`
- `PCC-42` — Реализовать `game.repository` query API
- `PCC-43` — Зафиксировать error convention repository API
- `PCC-44` — Зафиксировать canonical order `list`
- `PCC-45` — Зафиксировать отсутствие provenance в Lua surface
- `PCC-46` — Добавить cross-host тесты Lua repository access
- `PCC-47` — Подключить первый потребитель API

## Актуальные нормативные источники

- [GameDataRepositoryContract](../../Architecture/GameDataRepositoryContract.md)
- [DefinitionEnvelopeAndSchemaRules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md)
- [BuildAndTooling](../../Architecture/BuildAndTooling.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/PortableContentCore) содержит исходные task-файлы, acceptance criteria и evidence.
