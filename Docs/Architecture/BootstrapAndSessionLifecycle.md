---
title: Bootstrap and Session Lifecycle
status: normative
version: 2.4
updated: 2026-08-13
depends_on:
  - SystemContextAndComponents.md
  - GameDataRepositoryContract.md
  - LuaRuntimeContract.md
decisions:
  - ../ADR/0006-repository-reload-and-session-pinning.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0011-blueprint-screen-templates.md
---

# Bootstrap and Session Lifecycle

Ни один partially built repository, state, runtime или presentation не становится public. Commit выполняется на Game Thread после token/generation validation.

## Core invariants

- Не более одной active session и одной Lua VM.
- VM создаётся и уничтожается вместе с session.
- Cold start создаёт full menu session с обычным lifecycle core и enabled mod modules и empty gameplay roots.
- Initial repository строится до первой VM. Application может позднее опубликовать новый current snapshot, но active session остаётся pinned до restart.
- Registries freeze после registration.
- До `Ready` semantic input, commands, events, effects и save закрыты, кроме lifecycle-owned initial projection path.
- Async result проверяет owner, session generation и operation token.
- Failure candidate session заканчивается обязательным cleanup до `Destroyed`.

## Application states

```text
Uninitialized → Bootstrapping → MenuActive
MenuActive/GameActive → Transitioning → MenuActive/GameActive
Any active state → ShuttingDown → Terminated
Bootstrapping/Transitioning → Failed → Transitioning or ShuttingDown
```

`MenuActive`/`GameActive` допустимы только когда active session `Ready`. `Failed` не удерживает повреждённую session.

## Session states

```text
Creating → Registering → BuildingState → RestoringInstances
→ Starting → PreparingPresentation → Ready

Any build phase → Failed → Stopping → Destroyed
Ready → Stopping → Destroyed
```

Public readiness — один bool `is_ready`. Он становится true только после successful initial presentation apply и сбрасывается до выхода из `Ready`.

## C++ lifecycle façade

`UGV2RuntimeSubsystem` живёт в `UGameInstance` scope и предоставляет Blueprint только состояние lifecycle и typed requests. Он не является владельцем gameplay-state и не вызывает произвольные Lua functions.

Минимальный Blueprint-facing API:

- `GetSessionState()` возвращает application/session state и `is_ready` без mutable internal references.
- `SubmitUiInteraction(binding_handle, input_values)` принимает opaque UI binding handle и schema-defined values; команда определяется только current binding registry.
- lifecycle requests используют отдельные typed methods/descriptors, а не generic `CallLuaFunction(name, args)`.

Текущий vertical slice реализует façade без test-only runtime methods:

- `StartSession()`/`EndSession()` открывают и закрывают generation с одной Lua 5.4.8 VM;
- `GetActiveScreen()` возвращает только текущую reconstructable presentation instance;
- `SubmitUiInteraction(...)` является единственным публичным путём пользовательского input;
- создание Screen из C++ параметров, вызов Lua builder из automation и методы с семантикой `ForTest` запрещены.

До открытия session `UGV2RuntimeSubsystem` обязан успешно построить configured `UGV2ImageResourceCatalog`, загрузить `UGV2ScreenRegistry`, валидировать все `screen_id`, layers, duplicates и concrete non-abstract classes и построить private lookup. Ошибка любого required presentation catalog/registry запрещает создание Lua VM и переход session в `Ready`; наличие ранее опубликованного catalog instance не маскирует failure текущего bootstrap build. Перед module bootstrap coordinator рекурсивно загружает UTF-8 `.lua` tree из `Scripts/`; portable runtime проверяет `bootstrap/manifest.lua`, graph и source coverage до module initialization. Любая ошибка после создания candidate переводит candidate session в `Failed`. Binding records session-scoped и инвалидируются при новой generation.

Debug start sequence: `GameInstance` start → Screen Registry ready → session `Ready` → start binding publication → `UGV2DebugStartScreenWidget` → реальный button event → Semantic Input → Lua `core:command.debug.start` handler → copied generic Screen request → registry resolution → prepared field/binding candidate → registered `WBP_ScreenBase` child → atomic field apply → binding revision commit. Screen replacement выполняется после выхода из Lua. Automatic debug fixture запрещён в shipping build и не добавляет отдельный test API.

`FGV2SessionCoordinator` является private UE owner active/candidate session. Он создаёт для каждой generation отдельную portable runtime session, Bridge context, ingress queue, UI binding registry и operation registry. Ни один из этих объектов не переживает уничтожение owning session. `GV2RuntimeCore` не зависит от UObject/UMG и назначает вызывающий Game Thread owner thread-ом VM; standalone host использует тот же lifecycle на своём worker thread.

Все Blueprint/UE requests сначала попадают в coordinator-owned bounded FIFO ingress. Coordinator проверяет state/generation и запускает Lua entry point только когда `bExecutingLua=false`. Submit из работающего entry point может только добавить следующий item в очередь; nested execution запрещён. Переполнение возвращает typed technical rejection и не расходует accepted input sequence. Lua outbound publications принимаются как copied DTO и применяются после возврата текущего protected entry point; synchronous Blueprint ↔ Lua re-entry запрещён.

## Session start descriptor

```text
mode: Menu | NewGame | LoadSave
save_slot_id: required only for LoadSave
repository_version: exact pinned snapshot identity
reason: diagnostic string
```

LoadSave выполняет read-only preflight container до teardown active session. Commit запрещён, если requested repository version больше не current.

## Module lifecycle

Каждый module экспортирует table с canonical `module_id`, например `weather_mod:module.storm_rules`:

```lua
return {
  id = "weather_mod:module.storm_rules",
  register = function(ctx) end,
  create_default_state = function(ctx) end,
  migrate_state = function(ctx, tree, from_version, to_version) end,
  restore_instances = function(ctx, tree) end,
  validate_state = function(ctx, tree) end,
  start = function(ctx) end,
  build_initial_projection = function(ctx) end,
  stop = function(ctx, reason) end,
  unregister = function(ctx) end,
}
```

Order: core modules, затем mods по resolved load order. `stop`/`unregister` выполняются в reverse order. Первая user-hook error прекращает следующие user hooks, но не обязательный C++ cleanup.

### Phase restrictions

| Hook/phase | State mutation | Events/effects/I/O |
|---|---|---|
| `register` | No | Registration API only |
| `create_default_state`/`migrate_state` | Temporary tree only | No |
| `restore_instances`/`validate_state` | Temporary/session-local reconstruction | No external effects |
| `start` | Internal initialization only | Gates closed |
| `build_initial_projection` | Read-only state | Returns declarative snapshot |
| Ready runtime | Commands/services only | Normal rules |
| `stop`/`unregister` | No gameplay mutation | Local cleanup only |

## Cold start

1. Initialize platform/application services и построить required presentation catalogs/registries.
2. Discover and resolve core/enabled packages.
3. Build and atomically publish repository.
4. On repository error, do not create Lua VM; show UE-native recovery surface.
5. Create full Menu session pinned to repository.
6. Register modules and freeze registries.
7. Build empty menu state, restore runtime objects, validate, start.
8. Apply initial menu presentation.
9. Commit session `Ready` and Application `MenuActive`.

## New/load session build

1. Allocate new session ID/generation, VM, Bridge and service set privately.
2. Register modules and freeze registries.
3. Build temporary state: defaults for NewGame; decoded/migrated tree for LoadSave.
4. Restore instances and validate module/global invariants.
5. Assign canonical state only after full validation.
6. Run start hooks with external gates closed.
7. Build/apply initial presentation snapshot.
8. Commit `Ready` and enable input.

## Lifecycle requests

Coordinator performs one transition at a time. Equivalent request may join. One conflicting pending slot uses last-wins semantics; replaced request completes as `Superseded`. Shutdown has highest priority and clears pending work.

Cancellation is accepted only between phases before Ready commit. Synchronous Lua hook is not interrupted; cancellation applies after it returns.

## Replacement sequences

- **Menu → Game:** destroy menu completely, show UE-native loading surface, create game candidate.
- **Game → Menu:** destroy game completely, create new full menu session.
- **Restart:** copy committed start descriptor, destroy current session, create replacement.
- **Load another save:** preflight target slot, then full replacement session; never mutate active state in place.
- **Content reload:** build/publish new Application current snapshot, then controlled restart so replacement session pins it.
- **Shutdown:** clear pending, block new input/operations, reverse cleanup, release repository/platform services.

## Teardown order

1. `is_ready=false`; block input, commands, events, save and new operations.
2. Invalidate UI binding registry, чтобы queued или уже захваченные Widget events стали stale.
3. Stop effects and remove UI/Actor projections.
4. Cancel supported operations; invalidate all remaining tokens.
5. Call module `stop` in reverse order.
6. Call `unregister` in reverse registration order.
7. Force-clear command/event/service/instance registries, ingress и Bridge adapters.
8. Destroy Lua VM and session memory; state becomes `Destroyed`.

## Mandatory tests

Tests cover cold start, required Image Catalog/Screen Registry failure before VM, repository failure before VM, full menu lifecycle, NewGame/LoadSave order, one-VM invariant, last-wins pending slot, joined request, phase cancellation, stale completion/input discard, registry freeze, restore gates, readiness single commit, bounded ingress backpressure, FIFO/no synchronous re-entry, teardown hook failure, recovery menu, load-another-save preflight, shutdown priority и content-reload restart.
