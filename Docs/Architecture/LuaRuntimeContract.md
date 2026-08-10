---
title: Lua Runtime Contract
status: normative
version: 1.3
updated: 2026-08-10
depends_on:
  - StableIDSpecification.md
decisions:
  - ../ADR/0004-lua-state-mutation.md
  - ../ADR/0005-value-only-async-boundary.md
  - ../ADR/0007-lua-module-environment.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
---

# Lua Runtime Contract

Lua — authoritative gameplay runtime. Одна main Lua 5.4 VM принадлежит одной session и исполняется только на owner thread. В UE owner thread обязан быть Game Thread; standalone worker использует свой thread. Exact Lua patch закрепляется build manifest-ом.

Runtime source закреплён в repository как Lua 5.4.8. Build обязан проверять `LUA_VERSION_RELEASE_NUM == 50408`; использование system-installed Lua library запрещено. Upstream source, checksum, license location, excluded sources и локальные compiler adaptations фиксируются в `BuildManifest.json` рядом с vendored source.

## Invariants

- `lua_State*` не покидает LuaRuntime integration layer.
- Одновременное исполнение Lua entry points запрещено.
- Каждый C++ → Lua call protected и восстанавливает stack top.
- Synchronous re-entry запрещён.
- C++/Lua boundary принимает только schema-defined DTO, Stable IDs и opaque operation IDs.
- UObject, pointers, functions, threads, userdata и metatables не пересекают boundary.
- Lua source/module graph change требует full session restart.
- Portable runtime sources не включают UObject, `FText`, UE containers или Presentation implementation.

## Standard libraries

Разрешены безопасные части `base`, `table`, `string`, `math`, `utf8`. Не открываются:

- `io`, `os`, `debug`, `coroutine`;
- `load`, `loadfile`, `dofile`, `package.loadlib`;
- public filesystem searchers;
- Lua bytecode из packages;
- `math.random`/`math.randomseed`.

Это portability boundary, а не security sandbox.

## Module loader

- `module_id` — canonical Stable ID kind `module`.
- Manifest объявляет module dependencies; hidden dependencies запрещены.
- Dependency graph проверяется на cycles до VM creation.
- Core modules загружаются первыми, затем mods в resolved order.
- `require(module_id)` выполняет source один раз и возвращает export table.
- Module source path является provenance, не identity.
- Late registration после registry freeze запрещена.

## Global environment

Все modules используют одну VM и общий runtime `_G`, но module source не создаёт globals. Module хранит private values в lexical locals и возвращает export table.

Runtime-owned globals: `game`, safe standard libraries и loader functions. Extension data/functions публикуются только под:

```lua
game.mods[mod_id]
```

Например `game.mods.weather_mod`. Замена runtime-owned field — `GameApiFieldConflict`. Per-module environments и capability sandbox в v1 отсутствуют.

## Stable `game` façade

```lua
game = {
  state = <canonical mutable table>,
  repository = <read-only query service>,
  commands = <dispatcher service>,
  events = <post-commit EventBus service>,
  instances = <instance resolver>,
  services = <gameplay services>,
  ui = <presentation intent service>,
  bridge = <typed UE operation service>,
  random = <deterministic PRNG service>,
  time = <gameplay clock>,
  log = <structured logger>,
  runtime = <read-only session metadata>,
  mods = <module extension table>,
  null = <explicit-null sentinel>,
}
```

`game.repository` — единственное имя repository API; alias `game.data` отсутствует.

## Value model

| Lua | Boundary |
|---|---|
| boolean | bool |
| UTF-8 string | string |
| integer | signed int64 |
| finite number | double |
| 1..N dense table | schema array |
| string-key table | schema map/object |
| `game.null` | explicit null |
| `nil` | absent field |

Schema различает array/map. Empty container без schema boundary не пересекает. Cycles/shared identity не сохраняются. Conversion — deep copy.

Repository query возвращает обычную mutable detached Lua copy. Её изменение не влияет на repository и следующий query; frozen proxy/identity guarantees отсутствуют.

### C++ marshalling

`FGV2LuaMarshaller` конвертирует schema-defined values в private recursive C++ value model: null, bool, signed int64, finite double, UTF-8 string, array и string-key object. Каждое пересечение boundary создаёт detached deep copy.

Marshaller обязан:

- отличать absent field от explicit null;
- отклонять sparse/mixed tables, cycles, excessive depth/size и non-finite numbers;
- не сохранять Lua stack indices, registry references или userdata внутри DTO;
- не использовать JSON string serialization как runtime call protocol;
- преобразовывать Blueprint-facing structs в boundary values только через declared schema adapters.

`FText` не пересекает Lua boundary. Lua публикует `TextSpec`; Presentation выполняет localization/formatting и только после этого передаёт `FText` Widget-у.

## Canonical state

`game.state` — обычная mutable Lua table с JSON-representable values и `game.null`. Runtime не строит per-path proxies или ownership guards.

Нормативно state меняется только внутри active Command Handler через зарегистрированные Gameplay Services. Прямое изменение технически возможно trusted Lua-коду, но является contract violation и покрывается review/tests.

State не содержит functions, metatables, userdata, UObject, operation handles, queues, subscriptions или definition copies.

Uncaught error после начала mutation не запускает универсальный rollback. Session переходит в `Failed`; поэтому validation выполняется до первой mutation.

## Runtime instances

- Persistent state хранит `instance_id`, `definition_id`, `type_id` и explicit instance state.
- Runtime wrappers/methods/metatables перестраиваются после load.
- `game.instances.resolve(instance_id)` возвращает typed result и transient view для текущего entry point.
- View не сохраняется между lifecycle phases или technical completions; требуется повторный resolve.
- Unknown/stale ID возвращает typed failure.

## Protected execution

Каждый entry point:

1. Captures stack top и execution context.
2. Sets `bExecutingLua=true`.
3. Calls `lua_pcall` с traceback handler.
4. Validates return DTO по declared schema.
5. Clears context, restores stack, sets `bExecutingLua=false`.
6. Dispatches следующий queued ingress item отдельным entry point.

Expected gameplay refusal возвращает typed Result. Wrong schema, forbidden phase и uncaught error становятся structured runtime fault.

### VM host and entry points

`GV2RuntimeCore::FRuntimeSession` — portable STL-only session-scoped façade, а его private implementation владеет VM и единолично видит Lua C API. Façade предоставляет host-ам фиксированные typed entry points для bootstrap/lifecycle, Semantic Input, direct simulation `CommandRequest`, TechnicalInput и shutdown; generic `Call(function_name, json)` отсутствует.

Portable runtime реализует создание/уничтожение VM, selective opening `base`/`table`/`string`/`math`/`utf8`, удаление запрещённых base/math functions и protected entry points. UE semantic input и headless `CommandRequest` сходятся во fixed Lua command dispatcher до gameplay validation. Test runtime source проверяет restricted environment при старте; production module loader/services развиваются поверх того же portable host.

UE adapter выполняет `FGV2UiControlValue → portable Value` conversion до вызова runtime. Lua C API и portable implementation не зависят от Unreal Engine.

Native bindings также фиксированы и schema-defined. Минимальная surface:

- `game.ui.publish_snapshot(snapshot)` публикует complete Presentation Snapshot;
- `game.bridge.start_operation(operation_kind, request)` запускает registered typed UE operation;
- `game.log.write(level, record)` пишет structured diagnostic.

Binding не вызывает Blueprint delegate синхронно. Outbound DTO валидируется и копируется, затем ставится в coordinator-owned outbound queue. Presentation/operation work начинается только после восстановления Lua stack и выхода из текущего entry point.

## Determinism

- `game.random` использует named PRNG streams; stream states сохраняются.
- `game.time` предоставляет gameplay clock; wall clock отсутствует в gameplay API.
- Значимый map iteration использует `game.runtime.sorted_keys(map)`.
- `pairs()` разрешён, но его order не влияет на authoritative result.
- Async completion order становится explicit ordered technical input.
- Headless timing/worker scheduling не входит в authoritative inputs.

Одинаковые repository, state, commands, technical inputs, PRNG и gameplay clock дают семантически одинаковые state и events.

## Technical ingress

Bridge operation принимает DTO и возвращает `operation_id`:

```lua
local result = game.bridge.start_operation(
  "core:operation.resource.prepare",
  { resource_ids = ids }
)
```

C++ не принимает и не хранит Lua callback. Completion создаёт DTO:

```json5
{
  operation_id: "runtime@17:42",
  operation_kind: "core:operation.resource.prepare",
  ok: true,
  value: {},
}
```

DTO попадает в bounded ingress queue, проверяется по session generation/owner/token и передаётся фиксированному runtime dispatcher. Lua может иметь transient internal map `operation_id → handler`, но function остаётся внутри Lua.

## GC и diagnostics

- Incremental GC выполняется с diagnostic per-frame budget.
- Full GC допустим после module load, restore, return to menu и перед VM destruction.
- Runtime отслеживает heap current/peak, GC duration, entry duration, allocations, registry refs и stack balance.
- Hard CPU/memory quotas не являются частью v1.

Structured fault содержит code, module ID, entry point, phase, session generation, optional operation ID, correlation ID и sanitized source trace без absolute paths.

## Conformance

Tests покрывают VM ownership, exact patch, private Lua C API, restricted libraries, source-only loader, declared dependency graph, no module globals, game namespace conflict, DTO isolation, null/absent, numeric bounds, sparse/cyclic/depth rejection, absence of JSON call protocol, state mutation policy, no rollback, deterministic replay, protected calls/stack restoration, deferred outbound publication, no re-entry, operation completion/stale discard, GC cleanup и restart-only reload.
