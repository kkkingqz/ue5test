---
title: Lua Runtime Contract
status: normative
version: 3.1
updated: 2026-09-12
depends_on:
  - StableIDSpecification.md
decisions:
  - ../ADR/0005-value-only-async-boundary.md
  - ../ADR/0007-lua-module-environment.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0025-lua-module-replacement-and-export-freezing.md
  - ../ADR/0044-session-replacement-and-registry-sealing.md
---

# Lua Runtime Contract

> **Владеет:** VM, загрузкой модулей, sandbox-ограничениями, protected execution, value boundary, детерминизмом и runtime diagnostics.
> **Не владеет:** составом `game` и registry lifecycle ([Runtime Facade](RuntimeFacadeAndRegistries.md)), authoring `_ENV` ([Authoring Surface](AuthoringSurfaceContract.md)) и семантикой gameplay-подсистем.
> **Инварианты:** [INV-006](Invariants.md), [INV-007](Invariants.md), [INV-008](Invariants.md)
> **Реализация:** `Source/GV2RuntimeCore/`, `Scripts/runtime/`, `Scripts/bootstrap/`.
> **Проверки:** `RunLuaMarshallerConformance`, `Tests/Lua/lifecycle/`, `Tests/Lua/modules/`.

Lua — authoritative gameplay runtime. Одна main Lua 5.4 VM принадлежит одной session и исполняется только на owner thread: Game Thread в UE или выделенном thread standalone host-а. Exact patch — Lua 5.4.8 из repository; build проверяет `LUA_VERSION_RELEASE_NUM == 50408`. Source, checksum, license, исключения и compiler adaptations фиксирует `BuildManifest.json`. System-installed Lua library запрещена.

## Invariants

- `lua_State*` не покидает private integration layer; одновременное исполнение entry points и synchronous re-entry запрещены.
- Каждый C++ → Lua call protected, проверяет result schema и восстанавливает stack top и execution context.
- Boundary принимает только schema-defined values, Stable IDs и opaque operation IDs. UObject, pointers, functions, threads, userdata, metatables и canonical state через неё не проходят.
- Lua source или module graph меняются только через full session restart.
- Portable runtime не содержит UObject, `FText`, UE containers, Presentation implementation или filesystem access.
- Session удерживает pinned immutable repository handle от создания VM до `Stop()`; неготовый handle даёт `RepositoryNotReady` до создания VM.

## Libraries and global environment

Открываются безопасные части `base`, `table`, `string`, `math`, `utf8`. Запрещены `io`, `os`, `debug`, `coroutine`, `load`, `loadfile`, `dofile`, `package.loadlib`, filesystem searchers, package bytecode, `math.random` и `math.randomseed`. Это portability boundary, не security sandbox.

Обычный module хранит private values в lexical locals и возвращает export table; создание globals запрещено. Runtime-owned globals: safe libraries, `require`, `require_base` и `game`. Состав `game` задаёт [Runtime Facade and Registries](RuntimeFacadeAndRegistries.md). Authoring-модули получают отдельный `_ENV` по [Authoring Surface Contract](AuthoringSurfaceContract.md).

## Module loader

`module_id` — Stable ID kind `module`; source path — provenance, не identity. Незамещаемый ID выводится из пути под `scripts/` (`scripts/runtime/actors.lua` → `<pkg>:module.runtime.actors`). Замещаемый module (`replaceable: true`) обязан явно назвать target ID.

Package manifest возвращает data-only `{ entry_module_id, modules[] }` и не является module. Descriptor содержит `module_id`, package-relative `source`, полный список direct `dependencies`, опциональные `replaceable` и `authoring`. Core manifest находится в `scripts/bootstrap/manifest.lua` или `scripts/manifest.lua`; package manifests может детерминированно генерировать `Tools/Content/generate_manifest.py`.

Host рекурсивно собирает UTF-8 `.lua` под `scripts/`, передаёт полный source set как `@<package_id>/<relative>` и не задаёт load semantics порядком файлов. До первого module runtime обязан отклонить missing/unlisted source, duplicate ID внутри пакета, hidden или dynamic import, dependency cycle, unreachable module, sealed override и новый ID в чужом namespace.

Зависимости извлекаются из литеральных `require("...")`; `require(variable)` даёт `DynamicRequireDisallowed`. `require(module_id)` разрешён при инициализации только для direct dependency текущего descriptor, исполняет provider один раз и возвращает export активного победителя. Runtime handler использует сохранённые lexical imports и не вызывает `require`.

Providers упорядочены resolved package order: core, затем mods; последний разрешённый provider побеждает. `require_base()` доступен только во время инициализации replacement и возвращает предыдущий export, иначе `LuaModuleBaseNotAvailable`. Core `runtime/`, `boundary/`, `bootstrap/`, `presentation/` и `resources/` запечатаны по умолчанию; `gameplay/` и `debug/` заменяемы. Sealed target даёт `LuaModuleSealed`, новый foreign ID — `LuaModuleForeignNewId`. Lifecycle hooks вызываются только у победителя.

Каждая export table немедленно оборачивается immutable proxy (`__index`, `__pairs`, `__newindex` → `LuaModuleExportFrozen`, `__metatable = false`). В `GV2.LoadedModules` находится только активный provider. Этим модульные экспорты отличаются от runtime registries, которые замораживаются после общей фазы `register`.

## Source layers

```text
Scripts/
  bootstrap/       composition root и manifest
  boundary/        fixed host entry points и DTO adapters
  runtime/         portable kernel
  gameplay/        state, feature commands/services/queries
  presentation/    desired presentation; no gameplay mutation
  resources/       resource_id intents; no media loading
  debug/           development content через production contracts
```

`bootstrap/main.lua` — единственная composition root. `boundary` может вызывать runtime/application services; gameplay, presentation и resources не импортируют `boundary`, а получают host ports через composition root. Gameplay группируется по feature. `resources` хранит IDs и value-only request DTO; locator, streaming, decoded media и locale принадлежат host. Debug не создаёт test-only host API.

## Value boundary

| Lua | Boundary |
|---|---|
| boolean | bool |
| UTF-8 string | string |
| opaque byte buffer | Lua string с явной byte-buffer schema; UTF-8 validation не применяется |
| integer | signed int64 |
| finite number | double |
| dense `1..N` table | schema array |
| string-key table | schema object |
| `game.null` | explicit null |
| `nil` | absent field |

Schema различает array и object, включая пустой container. Conversion всегда создаёт detached deep copy; identity и shared references не сохраняются. Полученная Lua copy mutable и не обязана совпадать по identity с результатом следующего query.

`FGV2LuaMarshaller` — единственный C++ marshalling path для `GV2RuntimeCore::FValue` и `GV2ContentCore::FValue`. Он сохраняет canonical key order и различие absent/null; отклоняет sparse/mixed tables, cycles, depth больше 64, больше 10 000 nodes (`PortableValueLimitExceeded`), пустые field names (`PortableValueFieldInvalid`) и non-finite numbers (`PortableValueNonFinite`). DTO не хранит stack indices, registry references или userdata; JSON string не является call protocol. `FText` не пересекает boundary: Lua публикует `TextSpec`, Presentation выполняет localization.

Canonical state остаётся внутри VM; полная форма, mutation rules и opaque save boundary принадлежат [Canonical State and Save](CanonicalStateAndSave.md). Host может получить только документированные bytes или скаляры, например state hash.

## Protected execution and host API

Каждый entry point:

1. сохраняет stack top и execution context;
2. устанавливает execution guard;
3. вызывает `lua_pcall` с sanitized traceback;
4. валидирует return DTO;
5. восстанавливает context и stack;
6. только затем разрешает следующий queued ingress/outbound item.

Expected gameplay refusal — typed Result. Неверная schema, forbidden phase и uncaught error — structured runtime fault.

`GV2RuntimeCore::FRuntimeSession` — STL-only session façade; только private implementation видит Lua C API. Host получает фиксированные typed entry points для bootstrap/lifecycle, Semantic Input, direct `CommandRequest`, TechnicalInput и shutdown. Generic `Call(function_name, json)` запрещён. Lua source передаётся bytes-ами; portable core не открывает filesystem и не знает UE project path.

Native bindings также фиксированы и schema-defined. Outbound DTO валидируется и копируется в coordinator-owned queue до запуска Presentation или operation work. Blueprint delegate и другой host callback не вызываются синхронно во время Lua execution.

## Fixed lifecycle entry points

Portable host использует закрытые typed entry points, а не имена functions из gameplay content:

- bootstrap принимает pinned repository, ordered sources и `start_inputs`, где `mode` и repository identity обязательны; load дополнительно получает captured opaque bytes;
- registry sealing вызывается один раз после `register` hooks через private bootstrap owner и возвращает только success либо `FRuntimeFault`;
- state composition целиком владеет `core:module.runtime.state_composition` (`compose_default_state`); C++ не знает названий секций (`meta`, `mods`, `prng`, `time`) и правил слияния вкладов модулей, а вызывает единственную защищённую точку входа и проверяет типизированный результат;
- restore, validation и start являются последующими защищёнными фазами с уведомлением о переходе через фазовый callback (`FPhaseCallback`), позволяющий хосту инспектировать прогресс и проверять cancellation между фазами без прерывания защищенного выполнения Lua; canonical tree не возвращается в C++ и boundary не пересекает;
- в процессе гарантируется строго не более одной живой Lua VM (атомарный `GLiveVmCount`, `INV-010`), попытка превышения отвергается ошибкой `LuaVmExceededLimit`;
- `preflight_save_bytes(bytes)` read-only проверяет container в active VM и возвращает только typed outcome;
- `save_to_slot(slot_id)` вызывается host-ом только в safe point; storage binding получает opaque bytes;
- `stop`/`unregister` выполняются в reverse resolved module order, уже известном loader-у (CFC-07); ошибка в user hook прерывает последующие user hooks, но не обязательный C++ teardown.

`seed_hex` имеет форму ровно 16 lowercase ASCII hex characters и представляет полный uint64. Session generation не является seed. Legacy numeric manifest seed мигрируется codec-ом, а не lifecycle entry point.

У двух форм старта требования к seed противоположны, и это не послабление, а следствие того, кто им владеет:

| Форма старта | `start_inputs.seed_hex` | Источник seed |
|---|---|---|
| Cold start (`Menu`, `NewGame`) | **обязателен**, 16 lowercase hex | host; отсутствующий или неверный — `InvalidSeedHex`, не молчаливый ноль |
| Load (captured opaque bytes) | **обязан отсутствовать** | `meta.seed_hex` внутри восстановленного Lua состояния; host-переданный seed отвергается как `SeedHexNotAcceptedForLoad`, потому что он бы переинициализировал продолжаемое прохождение |

Из этого следует запрет, который легко нарушить «удобным» исправлением: C++ не извлекает seed из container bytes. Container для него — непрозрачные байты ([ADR-0021](../ADR/0021-opaque-save-container.md)), а поиск поля внутри них означал бы, что host знает gameplay-кодирование и что любое содержимое сейва, совпавшее с искомой формой, может подменить seed. `meta.seed_hex` обязателен в canonical state: `core:module.runtime.state_validator` отвергает состояние без него, поэтому сейв без seed отклоняется владельцем кодирования на загрузке, а не всплывает при первом обращении к `game.random`.

Preflight и subsequent load получают один и тот же request-owned byte buffer. Lua может декодировать его дважды на разных фазах, но C++ не читает slot повторно и не преобразует bytes в `FValue`. Preflight не меняет `game.state`, registries, queues, PRNG или presentation source.

## Determinism and technical ingress

- Root `seed_hex`, named PRNG streams и gameplay clock входят в canonical inputs; wall clock отсутствует. Algorithm, stream derivation и vectors определены в [Canonical State and Save](CanonicalStateAndSave.md#deterministic-random-streams).
- Значимый map iteration использует deterministic sorting; порядок `pairs()` не влияет на authoritative result.
- Async completion становится ordered TechnicalInput. Worker scheduling и headless timing не являются authoritative inputs.

Bridge operation возвращает opaque `operation_id`; C++ не принимает Lua callback. Completion содержит `operation_id`, `operation_kind`, `ok` и value/error DTO, проходит generation/owner/token checks и попадает в bounded ingress queue. Transient map `operation_id → function` может существовать только внутри Lua.

Одинаковые repository snapshot, state, commands, technical inputs, PRNG state и gameplay clock обязаны дать семантически одинаковые state и events.

## Failure, GC and diagnostics

Runtime fault переводит session в `Failed`; universal rollback после начавшейся mutation отсутствует. Incremental GC имеет diagnostic budget; full GC разрешён после load/restore, при возврате в menu и перед destruction. Hard CPU/memory quotas не входят в v1.

Diagnostics включают heap current/peak, GC и entry duration, allocations, registry refs и stack balance. Fault содержит code, module ID, entry point, phase, session generation, optional operation/correlation IDs и source trace без absolute paths.

## Compatibility and verification

Изменение library set, module grammar, value boundary или fixed host entry point является breaking change по [Compatibility Policy](CompatibilityPolicy.md). Проверки покрывают VM ownership и patch, module graph/providers, frozen exports, restricted libraries, отсутствие globals и hidden imports, value limits/UTF-8/null, protected stack restoration, no re-entry, deferred outbound, stale TechnicalInput, deterministic replay, GC cleanup и restart-only reload.
