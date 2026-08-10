---
title: System Context and Components
status: normative
version: 1.4
updated: 2026-08-10
depends_on:
  - Overview.md
  - GlossaryAndNaming.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
---

# System Context and Components

Документ фиксирует logical modules, dependency direction, ownership и lifetime. Конкретное разбиение на Unreal Build Modules и имена private classes можно менять без ADR, если границы остаются теми же.

## Context

| Actor/system | Связь |
|---|---|
| Player | Semantic input → system; visual/audio/world presentation ← system |
| Content author | Core definitions, schemas, Lua modules, localization и resources |
| Mod author | Package с собственным namespace, definitions, schemas/modules и optional Pak |
| OS/platform | Process, window, devices, locale, storage и platform services |
| Persistent storage | Settings, enabled-mod order, saves и diagnostics |
| Unreal toolchain | C++ build, asset cook и package assembly |
| Headless orchestrator | Scenario/Observation/CommandRequest и deterministic balance reports |

## Logical runtime modules

| Module | Responsibility | May depend on |
|---|---|---|
| Application | Process mode, session lifecycle, operation coordination, top-level recovery | Content, LuaRuntime, Bridge, Presentation |
| Content | Package resolution input, repository build, schemas, immutable data | Engine/Core only |
| LuaRuntime | VM, state, commands, gameplay services, EventBus, UI projection | Content |
| Presentation | UI/world/audio/resource projection и semantic input capture | Content |
| Bridge | Typed value-only façade между LuaRuntime и UE-facing services | Content, LuaRuntime, Presentation |

`Application` — composition root. `Content` — нижний module. `LuaRuntime` и `Presentation` не зависят друг от друга напрямую. Tooling никогда не становится runtime dependency.

`GV2RuntimeCore` является portable library под `LuaRuntime`/portable DTO и не зависит от UE Presentation. `GV2` Unreal module и `gv2-headless` являются sibling host adapters.

## Unreal module mapping

Проектный Unreal Build Module `GV2` является runtime composition module для UE boundary и presentation integration. Он может зависеть от `Core`, `CoreUObject`, `Engine`, `UMG`, `Slate` и `SlateCore`.

`GV2` не владеет canonical gameplay-state и не добавляет gameplay rules. Новые C++ классы должны сохранять logical ownership из таблицы выше: UE-facing adapters, UMG и DTO относятся к `Presentation` или `Bridge`, а gameplay mutation проходит через Lua `Command Dispatcher`.

На первом этапе logical modules реализуются внутри одного Build Module `GV2`. Разделение на несколько Build Modules допускается позднее без ADR, если public behavior, ownership и dependency direction не меняются. При добавлении native CommonUI bases `CommonUI` становится явной dependency `GV2`; Lua runtime library и optional serialization libraries остаются private dependencies.

### C++ implementation profile

Рекомендуемая физическая структура сохраняет logical boundaries:

```text
Source/GV2/
  Public/
    Runtime/        UGV2RuntimeSubsystem и Blueprint-safe DTO
    UI/             native Widget base classes и presentation DTO
  Private/
    Application/    FGV2SessionCoordinator
    Bridge/         ingress queue, operation и UI binding registries
    UI/             document reconciler, widget registry, input adapter
Source/GV2RuntimeCore/
  Public/            portable DTO, runtime session и host-service interfaces
  Private/           Lua VM owner, marshaller, fixed native bindings, vendored Lua
Headless/
  Source/            standalone simulation host и metadata-only adapters
```

Имена concrete private classes являются implementation baseline, но не compatibility API. Нормативны следующие границы:

- `UGV2RuntimeSubsystem : UGameInstanceSubsystem` — единственный Blueprint-facing façade runtime/session уровня.
- `FGV2SessionCoordinator` владеет UE active session composition: generation, pinned repository handle, portable runtime session, ingress queue, UI binding registry, operations и latest accepted Presentation Snapshot.
- `GV2RuntimeCore::FRuntimeSession` является STL-only public façade; Lua headers и `lua_State*` остаются в его private implementation.
- `FGV2UiDocumentReconciler`, Widget Registry и Semantic Input Adapter принадлежат Presentation/Bridge, но не LuaRuntime.
- Blueprint Widget classes отвечают за composition, style и local visual state; они не выбирают Lua entry point и не хранят gameplay authority.

Runtime-core vertical slice включает portable runtime session/Lua VM, UE-private `FGV2SessionCoordinator`, `FGV2UiBindingRegistry` и `FGV2RuntimeIngressQueue`. Coordinator владеет UE session generation, одной portable Lua session, atomic binding publication и bounded non-reentrant FIFO. Native test observer вызывается только после successful Lua dispatcher path.

Public C++ headers не выставляют Lua types, JSON strings, Slate implementation types, UObject references в Lua DTO или third-party runtime details. Boundary payload передаётся типизированными DTO/value tree; JSON может использоваться только как private storage/diagnostic representation, не как per-call Lua/Blueprint protocol.

## Lifetime scopes

| Scope | Owner | Содержимое |
|---|---|---|
| Application | Application subsystem | Current repository, platform services, session coordinator |
| Session | Session coordinator | One Lua VM, pinned repository handle, canonical state, Bridge binding |
| World | Presentation | Persistent UWorld и Actor projections |
| Presentation | Presentation coordinator | Game Shell, routes, overlays, presenters, resource pins |
| Route/UI instance | Route manager | Один route/overlay/modal и stale-input identity |
| Operation | Owning service | Async request, token, cancellation/invalidation |

Не более одной active session на process. Menu session и Game session не сосуществуют.

## Минимальные logical components

### Application

- Application mode state machine.
- Session coordinator.
- Operation/token coordinator.
- Error router и UE-native recovery surface.

### Content

- Package loader/resolver.
- Definition/schema parser и validator.
- Repository builder/service.
- Localization/resource catalog builders как derived outputs.

### LuaRuntime

- VM host и module loader.
- Command Dispatcher и command queue.
- Gameplay EventBus и event queue.
- Gameplay Service Registry.
- Runtime Instance Registry.
- UI Projection Controller.

Отдельный Action Registry отсутствует. UI отправляет `command_id` напрямую. Локальные hover/focus/navigation детали, не влияющие на gameplay, остаются в Presentation.

### Bridge

- DTO marshalling.
- UI/world/resource/audio adapters.
- Save adapter.
- Platform capability adapter.
- Operation registry с opaque handles; без Lua function references.

### Presentation

- Presentation coordinator и snapshot reconciler.
- UI document renderer и Game Shell.
- Route/layer manager.
- Widget registry.
- Semantic input adapter.
- Resource resolver/streaming service.
- Localization, portrait/character, world и audio presenters.

## Dependency rules

- Только Lua владеет canonical gameplay-state.
- Repository definitions immutable; runtime хранит IDs, не mutable pointers.
- Blueprint не вызывает Lua functions и не меняет state.
- Lua не получает UObject, raw pointer, UE asset path или filesystem API.
- Worker completion становится typed technical input и проходит ingress queue.
- Presentation можно уничтожить и построить заново без gameplay mutation.
- Gameplay extension points — commands, validators, events, services, lifecycle hooks и definitions.

## Principal flows

### Repository build

`resolved packages → schemas → parse/normalize → full override → semantic/reference validation → indexes/hash → immutable publish`

### Command

`semantic input → identity validation → Command Dispatcher → validators → handler/services → state commit → EventBus → UI projection → Presentation`

### Async UE operation

`Lua DTO request → Bridge operation_id → UE async work → token validation → TechnicalInput DTO → Lua ingress`

### Save

`Lua explicit save tree → C++ DTO validation → codec/checksum → atomic platform write → TechnicalInput result`

## Failure containment

| Failure | Required state |
|---|---|
| Repository build | Candidate not published; previous current remains |
| Lua bootstrap | Candidate session destroyed; recovery menu or UE-native surface |
| Command validation | State unchanged; typed failure |
| Command runtime error after mutation began | Session `Failed`; no automatic rollback |
| Event handler error | Command remains committed; event pump stops; session `Failed` |
| Presentation apply | Gameplay state remains valid; rebuild/fallback/error surface |
| Save write | Previous slot remains valid |
