---
title: Canonical State and Save
status: draft
version: 0.1
updated: 2026-08-10
depends_on:
  - LuaRuntimeContract.md
  - StableIDSpecification.md
  - BootstrapAndSessionLifecycle.md
---

# Canonical State and Save

Этот начальный контракт фиксирует границу state/save. Конкретная per-system schema будет добавляться без изменения ownership.

## Canonical root

```lua
game.state = {
  meta = {},
  player = {},
  actors = {},
  item_instances = {},
  world = {},
  quests = {},
  mods = {},
}
```

| Section | Purpose |
|---|---|
| `meta` | Save/schema versions, save identity, instance counters, PRNG, gameplay time |
| `player` | Single player state |
| `actors` | Persistent NPC/actor instances |
| `item_instances` | Unique item instances; stack counts live in owning containers |
| `world` | Global flags и location/screen state |
| `quests` | Activated quest instances only |
| `mods` | Только нестандартное namespaced mod state |

Стандартные mod entities используют общие registries. `mods[mod_id]` не дублирует standard state.

## Allowed values

State содержит strings, bool, int64/finite double, dense arrays, string-key maps/objects и `game.null`; cycles/shared identity/functions/metatables/userdata/handles/definition tables запрещены.

Значимый map iteration имеет explicit sort. Runtime wrappers и caches не входят в save.

## Instance invariants

- Player хранится отдельно; NPC — в `actors`.
- Unique instance имеет один persistent `instance_id` и один `definition_id`.
- Каждый unique item принадлежит ровно одному logical container.
- Persistent allocator counters не уменьшаются и сохраняются.
- Definition, instance и UE projection identities не взаимозаменяемы.

## Save container

Container включает:

- format/save schema versions и save ID;
- game/build metadata и checksum;
- repository content hash/provider fingerprints;
- explicit versioned core sections;
- PRNG streams и gameplay time;
- enabled mods/order/versions/fingerprints;
- namespaced mod sections;
- opaque orphaned sections временно отсутствующих mods.

Physical encoding не является gameplay contract. Development может использовать readable JSON; production — compact format.

## Export boundary

Lua формирует explicit pure data tree. C++:

1. Validates allowed DTO types, depth/size policy и envelope.
2. Serializes payload и metadata.
3. Writes temporary file.
4. Verifies and atomically replaces slot.
5. Maintains previous backup.

Lua не выполняет filesystem I/O. Save write failure не меняет предыдущий valid slot.

## Safe point

Save разрешён только когда:

- runtime phase `Idle`;
- command/event queues empty;
- session `Ready`, not `Failed`;
- gameplay-significant technical inputs processed;
- no lifecycle transition active.

## Load

Load всегда создаёт replacement session:

1. C++ preflight header/checksum/types/mod metadata без teardown current session.
2. Resolve required packages и choose current repository.
3. Destroy old session only after successful preflight.
4. Decode to temporary pure tree.
5. Run explicit core/mod migrations in deterministic order.
6. Resolve Stable ID redirects.
7. Restore runtime instances и validate invariants.
8. Assign canonical state only after full success.
9. Build initial presentation and commit new session Ready.

Migration failure не изменяет source save. После failure replacement candidate уничтожается и создаётся recovery menu.

## Missing mods

State неизвестного/disabled mod сохраняется opaque в container и не передаётся чужому module. При возвращении mod section доступна только после version/fingerprint compatibility check. Broken required references на missing definitions следуют explicit per-field recovery policy; silent substitution запрещён.

## Migrations

Migration работает с temporary tree и имеет `(section_id, from_version, to_version)`. Она deterministic, side-effect-free, не вызывает Bridge/events/effects и не меняет исходный container. Downgrade не поддерживается.

## Required follow-up

До production content нужны typed schemas для каждой root section, save compatibility matrix, orphan policy по reference kinds, corruption/backup recovery fixtures и size limits platform policy.

