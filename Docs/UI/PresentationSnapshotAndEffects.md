---
title: Presentation Snapshot and Effects
status: normative
version: 1.0
updated: 2026-09-18
depends_on:
  - UIDocumentAndReconciliation.md
  - ImageResources.md
  - ../Architecture/StableIDSpecification.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0047-one-shot-effect-pipeline-and-origins.md
  - ../ADR/0048-widget-exit-lifecycle-and-input-gating.md
---

# Presentation Snapshot and Effects

> **Владеет:** различием между восстановимым desired presentation и одноразовыми эффектами.
> **Не владеет:** тем, что именно показывать, и порядком гейплейных фактов.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h` (`FPresentationEffect`, `PublishHostLocalEffect`/`TakePendingEffects`, `ResolveEffectTarget`); `FGV2SessionCoordinator::DrainPresentationEffects` (единственный pull после каждого protected runtime entry и по запросу host-local producer); `UGV2RuntimeSubsystem::HandlePresentationEffects` (общий resolve/apply path обоих источников); `Source/GV2PresentationApply/Public/GV2PresentationApply/PresentationEffectApply.h` (`FGV2PresentationEffectApply::Apply`, `EPresentationEffectKind`).
> **Проверки:** `GV2.Runtime.Presentation.PresentationEffectConformance` (DTO, очередь, монотонность `sequence` при чередовании источников, три причины отбрасывания, non-persistence через реальный save/load), `GV2.Runtime.Presentation.PresentationEffectApply` (Game-Thread guard, exhaustive dispatch), `GV2.Runtime.Presentation.EffectsDrainAfterRuntimeEntry` (production drain после runtime entry без hover), `GV2.Runtime.Presentation.HoverEffectQueueContract` (accepted effect вызывает физическое действие, rejected effect его не вызывает), `GV2.Runtime.Presentation.HoverEffectNeverCrossesLua`.

Сообщения презентации разделены на durable desired snapshot и one-shot effects. Snapshot достаточен для полного восстановления presentation; effect никогда не является единственным носителем важного состояния.

Разделение проходит по одноразовости и отбрасываемости, а не по источнику: источник эффекта не входит в его identity ([ADR-0047](../ADR/0047-one-shot-effect-pipeline-and-origins.md)). Snapshot публикует только Lua.

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

### Источники эффекта

Поля источника в DTO нет. Источники различаются только точкой входа и во всём остальном равноправны: одна очередь, один счётчик `sequence`, один apply path.

- **Lua-published.** Фиксированный binding рядом с `publish_snapshot`. Binding schema-validates и deep-copies DTO в outbound queue; reconciler не запускается внутри Lua call.
- **Host-local.** Презентационное событие без gameplay-смысла: наведение и уход курсора, завершение показа по таймеру. Границу Lua не пересекает.

**Критерий допустимости host-local эффекта.** Host-local источник разрешён эффекту тогда и только тогда, когда выполнены оба условия:

1. Canonical state до порождения эффекта и после его полного исполнения побайтово равны.
2. После rebuild presentation эффект не воспроизводится сам и не требуется для корректного состояния: presentation полностью строится из latest complete snapshot.

Оба условия проверяются прогоном. Эффект, не удовлетворяющий любому из них, идёт через Lua и command path.

Hover/unhover остаются UE-local и Semantic Input не создают — см. [Semantic Input](SemanticInput.md) и [Widget Registry](WidgetRegistry.md); host-local источник это правило не ослабляет.

## TextSpec and locale resolution

Lua публикует только `TextSpec { text_id, args, style? }`. Concatenation локализованных fragments в gameplay Lua запрещена. Host localization adapter выбирает locale, применяет plural/gender/number rules и создаёт UE `FGV2TextViewModel` либо portable report string. `style` и markup token values разрешаются только host theme-ом.

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
- `sequence` монотонно возрастает по всей очереди независимо от источника; второго счётчика не существует.
- После каждого protected Lua entry host обязан немедленно вызвать единственную точку дренажа до выполнения несвязанной host-local работы. Поэтому committed Lua batch получает `sequence` до следующего host-local события; оставлять Lua batch до будущего hover запрещено.
- Отбрасывание эффекта несёт типизированную причину: stale target, чужое поколение сессии, устаревшая ревизия. Общий булев отказ не допускается — он делает неотличимой доставку в чужую сессию от штатного отбрасывания.
- Snapshot application is atomic at logical presentation level; partial apply cannot become interactive.
- Failed effect does not invalidate snapshot or gameplay state.

## Tests

Tests cover rebuild from snapshot, stale revision/effect, missing optional/required resources, prefetch non-authority, input gate during apply, effect non-persistence и actor/widget reconstruction. Non-persistence проверяется поведением после реальной загрузки, а не отсутствием поля в файле сохранения. Монотонность `sequence` проверяется потоком, в котором эффекты обоих источников чередуются.
