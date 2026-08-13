---
title: Content Diagnostics and Tooling Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-13
depends_on:
  - PortableContentCoreProposal.md
  - ../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../Architecture/GameDataRepositoryContract.md
  - ../Architecture/StableIDSpecification.md
---

# Предложение по diagnostics и tooling контента

## Назначение и область

Предлагается сделать diagnostics частью `GV2ContentCore`, а первым consumer — небольшой CLI `gv2-content`. Editor integration и будущий LSP обязаны отображать те же результаты, а не реализовывать вторую validation grammar.

Подходы к location tracking, bounded parsing, corpus tests, fuzzing и benchmarks берутся как reference из `rakaly/jomini`. Schema-aware completion и reference navigation — как будущий UX reference из CWTools и Paradox Language Support.

## Diagnostic model

Каждый diagnostic обязан содержать machine-readable поля:

```json5
{
  diagnostic_id: "core:diagnostic.content.duplicate_key",
  severity: "error",
  package_id: "weather_mod",
  source: {
    relative_path: "definitions/items.json5",
    start: { line: 18, column: 7, byte_offset: 412 },
    end: { line: 18, column: 12, byte_offset: 417 },
  },
  definition_id: "weather_mod:item.storm_ring",
  json_pointer: "/definitions/0/data/price",
  schema_id: "core:schema.definition.item.v1",
  message: "Duplicate object key.",
}
```

- `diagnostic_id` является canonical Stable ID; automation не ветвится по `message`.
- Paths package-relative и не раскрывают absolute host path в portable report.
- Span использует byte offsets и one-based line/column для editor UX.
- Отсутствующие context fields остаются absent, а не получают пустые placeholders.
- Diagnostics сортируются по severity, package load index, relative path, start byte и `diagnostic_id`.
- Parallel и single-thread build обязаны возвращать одинаковый порядок.

## Parser resilience

Parser обязан иметь configurable limits на input bytes, nesting depth, string length, container entries, definitions per file и total diagnostics. Limit breach возвращает typed diagnostic и прекращает только текущий candidate build.

Fuzz targets:

- JSON5 lexer/parser и UTF-8 handling;
- duplicate-key/source-span tracking;
- schema validator и union discriminator;
- normalization/hash stability;
- Stable ID parser и reference resolver.

Fuzzer не проверяет Unreal objects и не запускает Lua gameplay. Seed corpus включает все documentation examples и минимальные invalid fixtures.

## CLI `gv2-content`

Первый интерфейс:

```text
gv2-content validate <package-or-project-root> [--format=text|json]
gv2-content inspect <definition_id> [--provenance]
gv2-content hash <package-or-project-root>
```

CLI обязан:

- использовать тот же `GV2ContentCore`, schemas и package resolver, что runtime;
- возвращать stable process exit codes для success, content invalid и tool/configuration failure;
- печатать human-readable text локально и deterministic JSON для CI;
- не публиковать repository и не запускать Lua VM;
- не требовать Unreal Engine installation.

SARIF export добавляется после стабилизации diagnostic DTO и реального CI consumer. Он является adapter-ом, а не отдельной моделью ошибок.

## Editor и LSP roadmap

1. CLI validation и JSON output.
2. Editor command/commandlet, который вызывает shared core и открывает source location.
3. Completion для Stable IDs и schema fields из immutable repository metadata.
4. Find references/rename preflight для definitions.
5. Тонкий LSP process только после стабилизации CLI API.

Lua tooling на первом этапе ограничивается generated annotations для фиксированного `game` façade и registry IDs. Полный type system или отдельный Lua analyzer не добавляется.

## Польза, риски и трудоёмкость

- **Польза:** content/mod ошибки видны до запуска UE; CI и editor говорят одинаковыми codes/spans.
- **Трудоёмкость:** **M** после `GV2ContentCore`; LSP отдельно **L** и отложен.
- **Риск:** слишком ранний IDE framework. Мера — сначала CLI и stable diagnostic DTO.
- **Риск:** fuzz nondeterminism или долгий CI. Мера — bounded corpus tests на каждый change, длительные fuzz runs отдельно.
- **Риск:** две schema implementations. Мера — adapters используют только shared core API.

## Не входит в первый этап

- Собственный VS Code extension.
- Полное сохранение comments/formatting или round-trip JSON5 editor.
- Автоматическое исправление ambiguous content errors.
- Paradox DSL parser или dependency на Jomini/CWTools runtime.

## Критерии приёмки

- Один invalid fixture выдаёт одинаковые `diagnostic_id`, spans и order в UE commandlet, headless tests и CLI.
- Все examples Definition Envelope и Stable ID входят в corpus tests.
- Invalid UTF-8, excessive nesting/size и fuzz inputs не приводят к crash, hang или partial publication.
- CLI работает без Unreal Engine и имеет documented stable exit codes.
- Editor/LSP adapters не содержат собственной schema или reference validation logic.
