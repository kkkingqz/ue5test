---
title: System Context and Components
status: normative
version: 2.6
updated: 2026-08-15
depends_on:
  - Overview.md
  - GlossaryAndNaming.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0012-centralized-ui-theme.md
  - ../ADR/0016-png-suffix-image-metadata.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0018-portable-content-core-module.md
  - ../ADR/0019-content-host-support-module.md
---

# System Context and Components

> **Владеет:** составом logical modules, направлением зависимостей, ownership и lifetime scopes.
> **Не владеет:** внутренним устройством модулей и физической раскладкой сборки ([Build and Tooling](BuildAndTooling.md)).
> **Инварианты:** [INV-013](Invariants.md)
> **Реализация:** `Source/GV2ContentCore/`, `Source/GV2ContentHostSupport/`, `Source/GV2RuntimeCore/`, `Source/GV2ContentAuthoring/`, `Source/GV2ContentEditor/`, `Source/GV2/`.
> **Проверки:** запреты зависимостей — [Dependency Map](DependencyMap.md); границы модулей — `host_conformance_parity_contract`.

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

`GV2ContentCore` является нижней portable library для Content value model, Stable ID, package descriptors, repository build result и validators; её public API и shared sources никогда не выполняют filesystem I/O (ADR-0018). `GV2RuntimeCore` является portable library под `LuaRuntime`/portable DTO и зависит от `GV2ContentCore`, но не от UE Presentation. `GV2` Unreal module и `gv2-headless` являются sibling gameplay host adapters; `gv2-content` — дополнительный portable CLI (`validate`/`inspect`/`hash`), использующий тот же `BuildRepository()` reference path без Lua/UE dependency и без запуска gameplay session.

`GV2ContentHostSupport` (ADR-0019) — отдельная portable library, единственный владелец filesystem-based package discovery (`DiscoverPackageFromDirectory`: сканирует `definitions/*.json5` + self-describing `schemas/*.json5`). Она зависит от `GV2ContentCore` (типы `FPackageDescriptor`/`FDiagnostic`); обратная зависимость запрещена. `gv2-content`, `gv2-headless` и `GV2` (через `FGV2FilesystemContentSourceProvider`) — единственные consumers; ни один из них не дублирует discovery-логику самостоятельно.

`GV2ContentAuthoring` (ADR-0037) — portable write library поверх Core/HostSupport. Она готовит candidate в памяти, вызывает authoritative `BuildRepository()`, проверяет optimistic file stamp и только затем заменяет файл. `GV2ContentEditor` состоит из portable Editor Adapter/form/reference части и editor-only Slate frontend. Gameplay runtime, Headless и Shipping target не зависят ни от authoring, ни от editor/test modules.

UE host строит repository из canonical container/lock package set; интерактивный Editor может использовать config-owned development profile. Layout package root, staging и разделение production-контента и test corpus описаны в [Build and Tooling](BuildAndTooling.md).

Canonical Stable ID parser `GV2ContentCore::FStableId` принадлежит нижней portable library и используется Content, LuaRuntime и обоими host-ами. UE может иметь только encoding adapter `FStringView → UTF-8`; отдельная UE grammar запрещена.

## Unreal module mapping

Проектный Unreal Build Module `GV2` является runtime composition module для UE boundary и presentation integration. Он может зависеть от `Core`, `CoreUObject`, `Engine`, `UMG`, `Slate` и `SlateCore`.

`GV2` не владеет canonical gameplay-state и не добавляет gameplay rules. Новые C++ классы должны сохранять logical ownership из таблицы выше: UE-facing adapters, UMG и DTO относятся к `Presentation` или `Bridge`, а gameplay mutation проходит через Lua `Command Dispatcher`.

Physical mapping использует runtime modules `GV2`, `GV2RuntimeCore`, `GV2ContentCore`, `GV2ContentHostSupport` и editor-only modules `GV2ContentAuthoring`, `GV2ContentEditor`, `GV2TestSupport`. Shared `GV2ContentCore` implementation/public sources запрещено включать Unreal headers или вызывать Lua/filesystem API; filesystem-based package discovery принадлежит исключительно `GV2ContentHostSupport` (ADR-0019), который зависит от `GV2ContentCore`, но не наоборот. Тонкая generated Unreal module-bootstrap translation unit может иметь private dependency на UE `Core`; эта dependency не пересекает portable API и отсутствует у CMake static library. При добавлении native CommonUI bases `CommonUI` становится явной dependency `GV2`; Lua и optional serialization libraries остаются private implementation dependencies соответствующих host/runtime modules.

### C++ implementation profile

Рекомендуемая физическая структура сохраняет logical boundaries:

```text
Source/GV2/
  Public/
    Runtime/        UGV2RuntimeSubsystem и Blueprint-safe DTO
    UI/             native Widget base classes и presentation DTO
  Private/
    Application/    FGV2SessionCoordinator, stateless Screen Field Adapter Registry, FGV2RepositoryPublisher (Application-scope current repository/version) и FGV2FilesystemContentSourceProvider (UE-filesystem package source acquisition)
    Bridge/         ingress queue, operation и UI binding registries
    UI/             document reconciler, screen/widget registries, input adapter
Source/GV2RuntimeCore/
  Public/            portable DTO, runtime session и host-service interfaces
  Private/           Lua VM owner, marshaller, fixed native bindings, vendored Lua
Source/GV2ContentCore/
  Public/            Stable ID, value/diagnostic/package/schema-registry/scalar-validation/build-result API
  Private/           portable validators и reference repository build path; no filesystem I/O (ADR-0018)
Source/GV2ContentHostSupport/
  Public/            DiscoverPackageFromDirectory() и другие filesystem-based discovery helpers (ADR-0019)
  Private/           std::filesystem-based implementation; depends on GV2ContentCore, not vice versa
Source/GV2ContentAuthoring/
  Public/            authoring operations, typed outcomes и file-state stamp
  Private/           comment-preserving rewrite, candidate validation и atomic file replacement
Source/GV2ContentEditor/
  Public/            Editor Adapter, schema form/reference DTO и Slate widgets
  Private/           portable conformance плюс editor-only frontend; отсутствует в Shipping
Headless/
  Source/            standalone simulation host и metadata-only adapters
Tools/Content/
  Source/            standalone `gv2-content` CLI (validate/inspect/hash) host adapter; kept out of the
                     UE asset root `Content/` on purpose
Scripts/
  bootstrap/         module manifest и composition root
  boundary/          fixed Lua/host entry points
  runtime/           portable Lua kernel
  gameplay/          canonical gameplay features
  presentation/      desired presentation projection
  resources/         logical resource IDs/intents
  debug/             development content без test-only API
```

Имена concrete private classes являются implementation baseline, но не compatibility API. Нормативны следующие границы:

- `UGV2RuntimeSubsystem : UGameInstanceSubsystem` — единственный Blueprint-facing façade runtime/session уровня.
- `FGV2SessionCoordinator` владеет UE active session composition: generation, pinned repository handle, portable runtime session, ingress queue, UI binding registry, operations и latest accepted Presentation Snapshot.
- `FGV2ScreenFieldAdapterRegistry` является stateless Presentation/Bridge mapping `schema_id → trusted adapter`; adapters валидируют portable field values, готовят binding definitions и строят typed presentation values, но не публикуют Screen или bindings.
- `GV2RuntimeCore::FRuntimeSession` является STL-only public façade; Lua headers и `lua_State*` остаются в его private implementation.
- `FGV2UiDocumentReconciler`, Screen Registry, Widget Registry и Semantic Input Adapter принадлежат Presentation/Bridge, но не LuaRuntime.
- `UGV2ScreenWidgetBase` и Dynamic Screen Element adapters являются generic presentation layer и не знают concrete `screen_id`.
- Blueprint Screen Templates отвечают за composition и local visual state; reusable components получают default style из configured `UGV2UiTheme`. `UGV2ImageResourceCatalog` разрешает image `resource_id` в trusted texture и один из трёх canonical render modes. Ни templates, ни theme/catalog не выбирают Lua entry point и не хранят gameplay authority.

Runtime-core vertical slice включает portable runtime session/Lua VM и manifest loader, UE-private `FGV2SessionCoordinator`, `FGV2UiBindingRegistry` и `FGV2RuntimeIngressQueue`. Coordinator владеет UE session generation, одной portable Lua session, atomic binding publication и bounded non-reentrant FIFO. Host interaction sink вызывается только после successful Lua dispatcher path.

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
- Fixed Screen Field Adapter Registry; duplicate/unknown schema не допускает partial Screen candidate.
- Slot-scoped byte storage adapter; содержимое сейва не интерпретируется.
- Platform capability adapter.
- Operation registry с opaque handles; без Lua function references.

### Presentation

- Presentation coordinator и snapshot reconciler.
- UI document renderer и Game Shell.
- Route/layer manager.
- Screen Registry и Widget Registry.
- Central UI theme settings/Data Asset и style consumers.
- Semantic input adapter.
- Resource resolver/streaming service.
- Startup filesystem scanner и Image Resource Catalog с `fixed_aspect`/`nine_slice`/`tile` metadata.
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

`Lua serializes state to bytes → slot storage primitive → atomic platform write → TechnicalInput result`

Host не разбирает bytes: формат, integrity check и migrations принадлежат Lua (ADR-0021).

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
