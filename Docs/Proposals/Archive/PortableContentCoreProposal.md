---
title: Portable Content Core Proposal
status: archived
proposal_state: implemented
version: 1.0
updated: 2026-08-14
depends_on:
  - ../../Architecture/SystemContextAndComponents.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Architecture/HeadlessSimulationContract.md
  - ../../Architecture/Modding.md
decisions:
  - ../../ADR/0006-repository-reload-and-session-pinning.md
  - ../../ADR/0008-minimal-repository-indexes.md
  - ../../ADR/0010-portable-runtime-and-headless-simulation.md
---

# Предложение по portable Content Core

> **Предлагает:** общий portable pipeline `Packages → Definitions → Repository Snapshot → Runtime`.
> **Затрагивает:** [GameDataRepository](../../Architecture/GameDataRepositoryContract.md), [System Context](../../Architecture/SystemContextAndComponents.md).
> **Состояние:** реализовано; нормативный результат перенесён в contracts, документ сохраняется как rationale.

> **Реализовано.** Нормативное поведение перенесено в [GameDataRepository Contract](../../Architecture/GameDataRepositoryContract.md), [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md), [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md) и [Build and Tooling](../../Architecture/BuildAndTooling.md). Ход выполнения — в архивном плане [PortableContentCore](../../Plans/Archive/PortableContentCore/README.md). Документ сохраняется как rationale.

## Назначение и область

Предлагается реализовать один portable `GV2ContentCore`, который строит GameDataRepository для UE, `gv2-headless`, CI и authoring tools. Это материализует уже зафиксированное разделение `Package Sources → Definitions → Runtime`, не создавая альтернативной gameplay implementation.

## Ownership и dependency direction

```text
Package Sources
      ↓
GV2ContentCore: parse → validate → resolve → freeze
      ↓
Immutable Repository Snapshot
      ├──→ GV2RuntimeCore / Lua queries
      ├──→ GV2 UE host
      ├──→ gv2-headless
      └──→ gv2-content tooling
```

- `GV2ContentCore` владеет parser, source locations, declarative schema validation, provider resolution, references, provenance, normalization, minimal indexes и snapshot hash.
- `GV2ContentCore` не зависит от UObject, UMG, `FText`, Slate, filesystem UI или Lua C API.
- `GV2RuntimeCore` читает pinned immutable snapshot через typed read interface и не участвует в repository build.
- UE Application остаётся owner публикации current snapshot и controlled session restart.
- `FGV2SessionCoordinator` остаётся единственным coordinator active session. Отдельный gameplay lifecycle manager не добавляется.

Создание отдельного build target оправдано тремя consumers: UE, headless и CLI. Portable sources собираются Unreal Build Tool и CMake из одного набора файлов.

## Основной pipeline

1. Принять immutable resolved package descriptors.
2. Enumerate package-relative sources в deterministic order только для diagnostics.
3. Parse UTF-8 JSON5 с duplicate-key detection и source spans.
4. Проверить file envelope и exact schema binding.
5. Валидировать typed values без coercion и материализовать только explicit defaults.
6. Проверить namespace ownership и выбрать full-override winners.
7. Разрешить redirects/tombstones и typed references.
8. Запустить ordered side-effect-free semantic validators.
9. Построить только необходимые indexes, provenance и deterministic hash.
10. Freeze candidate; host атомарно публикует его после token validation.

Invalid override обязан блокировать candidate и не раскрывать предыдущего provider. Active session не переключает pinned snapshot.

## Разделение data и runtime

| Слой | Mutable | Lifetime | Содержимое |
|---|---|---|---|
| Parsed source model | Только во время build | Candidate build | Tokens/values, duplicate keys, source spans |
| Resolved Definition | Нет | Repository snapshot | Canonical ID, normalized data, schema identity, provenance |
| Runtime Instance | Да, через Command path | Lua session/save | `instance_id`, `definition_id`, `type_id`, instance state |

Runtime не хранит mutable pointer на Definition. Полный syntax tree не переносится в runtime snapshot: остаются normalized values и компактная provenance, необходимая diagnostics.

## Минимальный API

```text
BuildRepository(package_set, build_options) -> BuildResult
BuildResult = { snapshot | ordered_diagnostics }

RepositoryReadHandle:
  Find(definition_id)
  Require(definition_id)
  List(kind)
  GetProvenance(definition_id)
  GetContentHash()
```

Public API использует typed Stable ID wrappers и value types. Per-kind C++ managers (`ItemManager`, `QuestManager`, `LocationManager`) запрещены; domain behavior остаётся в Lua services и definitions.

## Этапы внедрения

1. **Foundation:** portable value model, Stable ID wrapper, diagnostics DTO и build-result API.
2. **Definitions:** JSON5/envelope/schema parsing с source spans и deterministic fixtures.
3. **Resolution:** packages, full overrides, references, provenance, minimal indexes и hash.
4. **Hosts:** одинаковые repository fixtures в UE, headless и CLI.
5. **Publication:** background candidate build, Game Thread publish и controlled restart integration.

Каждый этап обязан оставлять один reference execution path. Parallel build разрешается только после equivalence tests с single-thread build.

## Польза, риски и трудоёмкость

- **Польза:** один data contract для runtime, tests и tooling; раннее обнаружение mod/content ошибок; headless parity.
- **Трудоёмкость:** **L**.
- **Риск:** преждевременный generic framework. Мера — реализовать только keywords, indexes и validators из текущих contracts.
- **Риск:** расхождение UBT/CMake. Мера — один source set и одинаковые fixtures в обоих build systems.
- **Риск:** лишняя память из-за source model. Мера — освобождать parse representation после freeze и хранить компактные spans/provenance.

## Не входит в предложение

- ECS, fixed-point arithmetic, lockstep networking или multi-VM process.
- Per-definition-type native managers/classes.
- Generic trigger/effect DSL до concrete gameplay scenarios.
- Unreal Data Registry как storage owner.
- DataConfig или другой UE reflection serializer как обязательная dependency.

## Критерии приёмки

- UE, headless и CLI строят одинаковый normalized snapshot/hash или одинаковый ordered diagnostic set из одинаковых inputs.
- Portable target собирается без Unreal headers/libraries.
- Invalid candidate не меняет current snapshot; active session продолжает использовать pinned handle.
- Full override, broken override, duplicate key/ID, references, redirects, file permutation и parallel equivalence покрыты fixtures.
- Runtime query не выполняет parsing, I/O, asset loading или hidden migration.
- Добавление нового definition kind со schema не требует нового C++ manager.
