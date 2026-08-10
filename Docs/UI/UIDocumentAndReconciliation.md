---
title: UI Document and Reconciliation
status: draft
version: 0.3
updated: 2026-08-10
depends_on:
  - ../Architecture/StableIDSpecification.md
  - ../Architecture/CommandsAndEvents.md
---

# UI Document and Reconciliation

UI-document — полное декларативное desired UI для одной revision. Lua строит его из canonical state и pinned repository; Presentation reconciles его с UMG instances.

## Game Shell

Постоянная оболочка имеет named layers:

```text
background
location_content
character_presentation
core_interface
overlay_stack
modal_stack
```

Location/screen definitions заполняют разрешённые slots, но не копируют Game Shell. Core UI и mod extensions используют explicit slots/extension points.

## Document envelope

```json5
{
  ui_instance_id: "ui@17:8",
  revision: 42,
  route: { /* one root node */ },
  overlays: [],
  modals: [],
}
```

- `ui_instance_id` уникален для document lifecycle.
- `revision` monotonically increases внутри instance.
- Одновременно существует не более одного main route.
- Overlays существуют, пока присутствуют в desired document.
- Modals — ordered stack; interactive только top eligible modal.

## Node model

```json5
{
  key: "travel_market",
  element_id: "core:screen.city_map#widget.travel_market",
  widget_id: "core:widget.button",
  props: {
    label: {
      text_id: "core:text.location.market.travel",
      args: {},
    },
    is_enabled: true,
  },
  command: {
    command_id: "core:command.location.travel",
    args: {
      target_location_id: "core:location.city.market",
    },
  },
  children: [],
}
```

| Field | Rule |
|---|---|
| `key` | Stable local sibling identity; не display text |
| `element_id` | Optional authored local child ID для diagnostics/input |
| `widget_id` | Registered component type |
| `props` | DTO validated widget property schema |
| `command` | Optional command binding; no Lua function/callback name |
| `children` | Ordered child nodes if widget schema permits |

Keys unique among siblings. `element_id`, если задан, unique в document. Runtime-generated repeated nodes используют deterministic key из durable identity, не array index, если reorder возможен.

## Runtime binding registry

Widget не получает `command_id`, bound args или Lua callback как authoritative input payload. Во время apply `FGV2UiDocumentReconciler` создаёт session-scoped binding record:

```json5
{
  binding_handle: "runtime@17:91",
  session_generation: 17,
  ui_instance_id: "ui@17:8",
  revision: 42,
  node_key_path: ["route", "travel_market"],
  element_id: "core:screen.city_map#widget.travel_market",
  command_id: "core:command.location.travel",
  bound_args: {
    target_location_id: "core:location.city.market",
  },
  input_schema_id: "core:schema.ui.button_input.v1",
}
```

`binding_handle` — opaque transient ID, не Stable ID и не save data. Он уникален внутри session generation и резолвится только через current UI binding registry. `node_key_path` является runtime identity node в tree; optional `element_id` сохраняет authored provenance для diagnostics и result placement.

Presentation передаёт physical Widget только `binding_handle` и визуальные props. Registry инвалидируется до удаления/replacement node, смены interactive revision и session teardown. Blueprint не может создать binding record или заменить его `command_id`/`bound_args`.

Publication является atomic: registry сначала валидирует весь candidate set, включая generation, monotonic revision, unique `node_key_path`, command/bound args и input fields. Ошибка любого record оставляет current revision и все её handles без изменений. Успешная публикация целиком заменяет current set; handles предыдущей revision становятся invalid, а handles предыдущей session generation — stale.

Текущий vertical slice реализует эту семантику в private `FGV2UiBindingRegistry`. `WBP_Testscreen` публикует synthetic document revision через тот же registry; полноценный `FGV2UiDocumentReconciler` остаётся следующим presentation layer и не меняет registry contract.

## Reconciliation

1. Validate envelope, revision, widget registrations, command bindings и property/input schemas.
2. Mark old document non-interactive and invalidate bindings for replaced/removed nodes.
3. Match nodes by parent identity + `key`.
4. Same key + same `widget_id` updates existing Widget.
5. Changed `widget_id` replaces Widget.
6. Reconcile ordered children/layers and prepare new binding records privately.
7. Atomically publish document, binding registry и input-ready revision only after successful apply.
8. Start optional enter/exit animations after logical state commit.

Exit animation не продлевает logical input lifetime removed node.

## Full update policy

Lua всегда отправляет complete document/revision, не operations patch. Internally Presentation may calculate diff. Boundary-level partial patch, JSON Patch и mutation operations отсутствуют.

## Route/layer rules

- Route replacement создаёт новый route identity или новый UI instance согласно navigation policy.
- Overlay по умолчанию не блокирует route; его props могут явно ограничивать input scope.
- Modal блокирует нижние layers; owner removal каскадно удаляет его modals.
- Unknown/invalid widget открывает typed system error surface; document не применяется частично.

## UI-local state

UE хранит focus, hover, pressed, scroll offset, animation progress и local tooltip state. Lua хранит только значение, влияющее на gameplay или обязанное пережить document reconstruction. Такое значение возвращается semantic input/command и становится canonical state.

Open window/focus/animation progress не входят в save. После load UI строится из canonical state заново.

## Text and rich text

TextSpec:

```json5
{
  text_id: "core:text.dialogue.aria.greeting",
  args: { player_name: "..." },
}
```

Пунктуация и word order принадлежат localized message. Rich text использует закрытые semantic roles (`strong`, `emphasis`, `accent`, `warning`, `muted`), не inline colors/sizes.

## Initial acceptance

Tests покрывают stable-key reuse, widget replacement, overlay/modal order, removal-before-exit, invalid widget atomic failure, binding publication atomicity с сохранением предыдущего set, superseded/stale handle invalidation, revision monotonicity, full rebuild after load и local-state loss without gameplay mutation.
