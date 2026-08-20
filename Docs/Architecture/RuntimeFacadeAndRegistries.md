---
title: Runtime Facade and Registries
status: normative
version: 1.0
updated: 2026-08-20
depends_on:
  - LuaRuntimeContract.md
  - StableIDSpecification.md
decisions:
  - ../ADR/0025-lua-module-replacement-and-export-freezing.md
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0031-entity-authoring-extensions.md
  - ../ADR/0033-command-validator-authoring.md
  - ../ADR/0034-gameplay-service-authoring.md
---

# Runtime Facade and Registries

> **Владеет:** картой фасада `game`, общим registry protocol и host-side последовательностью заморозки.
> **Не владеет:** значением registry entries, локальным ordering, refusal и override rules — ими владеют subsystem contracts.
> **Инварианты:** [INV-006](Invariants.md)
> **Реализация:** `Scripts/runtime/*_registry.lua`, `Scripts/bootstrap/`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`.
> **Проверки:** `Tests/Lua/lifecycle/`, registry specs соответствующих подсистем.

`game` — закрытая session-scoped карта runtime capabilities. Новая подсистема не добавляет top-level field без архитектурного решения: сначала используется namespace существующего owner-а. Gameplay code не подменяет и не расширяет runtime-owned fields; extension data публикуется только под `game.mods[mod_id]`. Конфликт даёт `GameApiFieldConflict`.

## Facade map

| Field | Роль | Owner semantics |
|---|---|---|
| `state` | canonical mutable state | [Canonical State and Save](CanonicalStateAndSave.md) |
| `repository` | read-only definitions query | [GameDataRepository](GameDataRepositoryContract.md) |
| `commands` | dispatch, handlers, validators, deferred queue | [Commands and Events](CommandsAndEvents.md) |
| `events` | post-commit EventBus и subscribers | [Commands and Events](CommandsAndEvents.md) |
| `instances` | registries и disposable runtime wrappers | [Canonical State and Save](CanonicalStateAndSave.md) |
| `services` | stateless multi-entity workflows | [Commands and Events](CommandsAndEvents.md) |
| `actions` | semantic action → command binding | [Semantic Input](../UI/SemanticInput.md) |
| `entity_extensions` | effective methods сущностей | [Canonical State and Save](CanonicalStateAndSave.md) |
| `presentation` | desired-presentation source | [Screen Templates](../UI/ScreenTemplates.md) |
| `ui` | value-only presentation outbound | [Presentation Snapshot and Effects](../UI/PresentationSnapshotAndEffects.md) |
| `save_slots` | opaque slot storage primitive | [Canonical State and Save](CanonicalStateAndSave.md) |
| `bridge` | typed host operations | [Lua Runtime Contract](LuaRuntimeContract.md) |
| `runtime` | session metadata, phase и fixed runtime entry points | [Bootstrap and Session Lifecycle](BootstrapAndSessionLifecycle.md), [Commands and Events](CommandsAndEvents.md) |
| `random`, `time` | deterministic PRNG и gameplay clock | [Canonical State and Save](CanonicalStateAndSave.md) |
| `log` | structured diagnostics | [Lua Runtime Contract](LuaRuntimeContract.md) |
| `mods` | namespaced package extensions | [Modding](Modding.md) |
| `null` | explicit-null sentinel | [Lua Runtime Contract](LuaRuntimeContract.md) |
| `debug` | development-only surface | [Build and Tooling](BuildAndTooling.md) |

`game.repository` — единственное имя repository API; `game.data` отсутствует. Оно предоставляет `get`, `require`, `list`, `exists`, защищено от записи и возвращает detached payload copies без provenance. Полная query/error semantics остаётся у repository contract.

Новая категория runtime instances размещается под `game.instances`, а не новым top-level field. Registry отвечает за identity, lookup, lifecycle и deterministic enumeration; gameplay rules принадлежат entity methods или services.

## Common registry protocol

Каждый production registry проходит один lifecycle:

```text
create → register declarations → resolve cross-references → freeze → read/execute
```

- Регистрация разрешена только в lifecycle phase `register`; поздняя запись отклоняется typed `*RegistryFrozen` или authoring-specific `*DeclarationAfterFreeze`.
- `freeze()` идемпотентно завершает resolution/validation, после чего `is_frozen()` возвращает `true`. Разморозка production registry запрещена.
- Direct table assignment запрещён; mutation выполняется только опубликованной registration operation.
- Query/introspection не выдаёт mutable internal entries. Enumeration детерминирована.
- Duplicate, replacement, priority и entry validation принадлежат owner subsystem и не выводятся из package order общим протоколом.
- Test isolation может получить private admin handle, но он не публикуется через `game`; isolated scope обязан восстановить entries и frozen state даже после ошибки.

Functions и implementation tables остаются внутри Lua. Host знает пути registry objects только для lifecycle freeze и не извлекает entries через C++/Lua boundary.

## Host-side freeze sequence

После вызова всех module hooks `register`, до создания или присвоения canonical state, host выполняет единый gate:

1. Authoring adapters регистрируют накопленные declarations и проверяют targets/mежпакетные ссылки.
2. Замораживаются `services`, `actions`, `entity_extensions`.
3. Замораживаются `commands.validators`, затем `commands.handlers`.
4. Замораживаются `events.subscribers`, затем EventBus registration surface.
5. Замораживаются `instances.actors`, затем общий `instances` registry.
6. Замораживается `presentation` source registry.
7. Замораживаются reference-field/schema registries canonical state.

Порядок фиксирован, потому что validation раннего registry может ссылаться на declarations, собранные всеми пакетами. Ошибка resolution или freeze блокирует session startup; частично замороженная candidate session не публикуется.

Module export tables замораживает loader сразу после исполнения provider-а, а не этот gate. Package load order и module replacement определяет [Lua Runtime Contract](LuaRuntimeContract.md); registry override rules не наследуют last-wins автоматически.

## Failure and evolution

Отсутствующий required registry, mutable production entry, late registration и незамороженный registry при переходе к state build являются startup fault. Registry API расширяется только при наличии owner subsystem и concrete consumer; generic «registry of everything» запрещён.

Проверки обязаны подтверждать late-registration rejection, immutable entries, deterministic enumeration, duplicate/override policy owner-а, target resolution после загрузки всех пакетов и одинаковый frozen state в UE/headless hosts.
