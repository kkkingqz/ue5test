---
title: Commands and Events
status: normative
version: 1.2
updated: 2026-08-10
depends_on:
  - LuaRuntimeContract.md
  - StableIDSpecification.md
decisions:
  - ../ADR/0003-command-and-event-model.md
  - ../ADR/0004-lua-state-mutation.md
---

# Commands and Events

Command — единственная публичная форма намерения изменить gameplay-state. Event — неотменяемый факт после commit. Отдельные action bus, command bus и `before_*` events отсутствуют.

## Command envelope

```json5
{
  command_id: "core:command.location.travel",
  schema_version: 1,
  payload: {
    target_location_id: "core:location.city.market",
  },
  source: {
    kind: "ui",
    ui_instance_id: "ui@17:8",
    node_key_path: ["route", "main", "buttons", "travel_market"],
    element_id: "core:screen.main#widget.travel_market",
  },
  correlation_id: "runtime@17:51",
}
```

Command IDs используют kind `command`. Handler function name не является частью wire contract.

Для `source.kind = "ui"` поля `ui_instance_id` и `node_key_path` берутся из validated UI binding record. Path обозначает logical Screen Instance/Field/item, а не physical Widget tree. `element_id` optional и является authored provenance, а не обязательной runtime identity. Blueprint не формирует command source и не передаёт authoritative `command_id` напрямую.

## Result

```lua
{ ok = true, value = { location_id = "core:location.city.market" } }
{ ok = false, error = { code = "core:error.location.locked", params = {} } }
```

Expected refusal не является runtime fault. Failed result гарантирует: state не изменён, gameplay events не опубликованы, deferred effects отброшены.

## Lifecycle

1. Validate runtime phase и source identity.
2. Validate envelope/payload schema.
3. Run ordered command validators read-only.
4. Invoke one handler.
5. Handler mutates state через Gameplay Services.
6. Validate required postconditions.
7. Commit command result и enqueue gameplay facts.
8. Pump EventBus FIFO/breadth-first.
9. Rebuild affected UI/presentation desired model.

Command handler не вызывает другой handler synchronously. Новая самостоятельная command ставится в queue.

## Command validators

Validators заменяют cancellable `before_*` events.

- Read-only state/repository/payload.
- No mutation, event emission, presentation effect или external operation.
- Возвращают allow либо typed refusal.
- Stable order: priority, package load order, registration order.
- Core и mods регистрируются до registry freeze.
- Exception/error validator-а — runtime fault, не normal refusal.

## Mutation authority

Только active Command Handler вызывает mutating Gameplay Services. Event handlers, UI controllers и technical-input handlers могут enqueue command, но не меняют canonical state напрямую.

Gameplay Service обязан:

- проверять локальные invariants;
- не выполнять filesystem/UE calls;
- возвращать structured result;
- enqueue facts через текущий command context;
- не публиковать events до command commit.

## Runtime phases

| Phase | New command | Event enqueue | Save |
|---|---|---|---|
| Idle | Allowed | System-only | Allowed |
| ExecutingCommand | Queue only | Current context only | No |
| PumpingEvents | Queue only | Allowed | No |
| Saving | No | No | One operation |
| Failed | No | No | Recovery only |

## Gameplay EventBus

Event IDs используют kind `event`:

```text
core:event.location.leave
core:event.location.enter
core:event.item.add
```

Event envelope содержит `event_id`, schema version, immutable payload, correlation/causation IDs и source provenance.

- Event доставляется только после successful command commit.
- Queue FIFO; events, созданные handler-ами, обрабатываются breadth-first.
- Subscriber order: priority, package load order, registration order.
- Handler может читать state/repository, enqueue event или enqueue command.
- Pump имеет configured limit; limit breach переводит session в `Failed`.

## Error semantics

- Validation/refusal: state unchanged, session остаётся Ready.
- Handler fault до mutation: command fails; session policy может вернуть Failed без corrupted state.
- Handler fault после mutation began: `InvariantViolation`, session `Failed`, no rollback.
- Event handler fault: originating command остаётся committed, current pump прекращается, remaining events discard, session `Failed`.
- UI сохраняет successful command result и отдельно показывает system failure/recovery surface.

## Resource-dependent command

Command не ждёт async UE operation. Если обязательный resource не prepared, validator возвращает typed `resource_not_ready`. Presentation controller запускает prepare operation; completion приходит как technical input и повторно enqueue-ит command с актуальной identity. Commit выполняется только после повторной полной validation.

## Conformance

Tests покрывают schema rejection, validated UI source construction, validator order/refusal, no-mutation-on-failure, single handler, queued nested commands, commit-before-events, FIFO/breadth-first ordering, handler order, event pump limit, post-mutation fault, event fault и resource prepare retry.
