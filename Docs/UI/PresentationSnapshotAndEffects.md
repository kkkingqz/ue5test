---
title: Presentation Snapshot and Effects
status: draft
version: 0.3
updated: 2026-08-10
depends_on:
  - UIDocumentAndReconciliation.md
  - ../Architecture/StableIDSpecification.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
---

# Presentation Snapshot and Effects

Lua → UE сообщения разделены на durable desired snapshot и one-shot effects. Snapshot достаточен для полного восстановления presentation; effect никогда не является единственным носителем важного состояния.

## Presentation snapshot

```json5
{
  snapshot_revision: 73,
  ui_document: { /* complete document */ },
  background_resource_id: "core:resource.location.market.background",
  characters: [
    {
      slot_id: "core:slot.character.left",
      actor_instance_id: "actor@7",
      appearance_resource_id: "core:resource.character.aria.casual",
      pose: "idle",
    },
  ],
  music_resource_id: "core:resource.music.market.day",
  ambience_resource_ids: [],
  prefetch_resource_ids: [],
}
```

Snapshot содержит только semantic IDs/values. UE paths, Actors и streaming handles отсутствуют.

Physical media payload принадлежит host resource adapter. UE catalog сопоставляет `resource_id` с cooked asset locator/Pak entry; headless catalog хранит только kind/availability metadata и не декодирует media. Raw locator не пересекает Lua boundary и не входит в canonical state.

Lua публикует snapshot только через fixed binding `game.ui.publish_snapshot(snapshot)`. Binding schema-validates и deep-copies DTO в outbound queue; reconciler не запускается внутри Lua call. После выхода из protected entry point coordinator принимает только snapshot current session generation и передаёт его Presentation. Same/older revision или payload от destroyed session отбрасываются до apply.

## Effect

```json5
{
  effect_id: "core:effect.transition.fade",
  sequence: 104,
  target: { ui_instance_id: "ui@17:8" },
  args: { duration_ms: 250 },
}
```

Examples: play sound, semantic animation, short transition, transient toast. Effect может быть discarded после stale target/session. Save/load его не replays.

## TextSpec and locale resolution

Lua публикует только `TextSpec { text_id, args }`. Concatenation локализованных fragments в gameplay Lua запрещена. Host localization adapter выбирает locale, применяет plural/gender/number rules и создаёт UE `FText` либо portable report string.

Headless balance run по умолчанию сохраняет unresolved `TextSpec`; locale отсутствует и не влияет на gameplay. Localization tests могут подключить portable catalog из того же logical localization source, из которого cook создаёт UE localization resources.

## Rebuild guarantee

После load, UI reconstruction, map/presenter reset или recoverable Presentation failure UE:

1. Releases invalid projections/handles.
2. Resolves resource IDs from pinned repository/catalog.
3. Applies latest complete snapshot.
4. Enables input only after UI/document readiness.

Gameplay state не меняется в результате rebuild.

## Resource prepare and prefetch

- Prefetch IDs — hints; они не доказывают command availability и не гарантируют retention.
- Mandatory resource uses explicit prepare operation before gameplay commit.
- Active critical resources pin according to Presentation scope.
- Optional missing resource uses typed placeholder.
- Required missing resource fails the operation and returns TechnicalInput.

## Snapshot/effect ordering

- Snapshot revision monotonically increases.
- Same/older revision ignored.
- Effects have sequence and optional target identity.
- Snapshot application is atomic at logical presentation level; partial apply cannot become interactive.
- Failed effect does not invalidate snapshot or gameplay state.

## Tests

Tests cover rebuild from snapshot, stale revision/effect, missing optional/required resources, prefetch non-authority, input gate during apply, effect non-persistence и actor/widget reconstruction.
