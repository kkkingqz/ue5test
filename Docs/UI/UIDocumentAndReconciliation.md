---
title: UI Document and Reconciliation
status: draft
version: 1.3
updated: 2026-08-13
depends_on:
  - ../Architecture/StableIDSpecification.md
  - ../Architecture/CommandsAndEvents.md
  - ScreenTemplates.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
---

# UI Document and Reconciliation

UI-document — полная декларативная desired model Screen instances для одной revision. Lua строит его из canonical state и pinned repository; Presentation разрешает `screen_id` через Screen Registry и reconciles document с UMG instances.

Этот draft задаёт target contract полного layered reconciler. Реализованный scope явно перечислен в разделе [Current vertical slice](#current-vertical-slice); остальные lifecycle rules и соответствующие tests являются planned acceptance до появления layered document owner.

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

Screen Template помещается только в разрешённый registry layer и не копирует Game Shell. Core UI и mod extensions используют explicit slots/extension points.

## Document envelope

```json5
{
  ui_instance_id: "ui@17:8",
  revision: 42,
  route: {
    instance_key: "main",
    screen_id: "core:screen.main",
    fields: {
      description: {
        schema_id: "core:schema.ui_field.rich_text.v3",
        value: {
          text: {
            text_id: "core:text.screen.main.description",
            args: {},
            style: "inventory",
          },
          spans: [
            {
              span_id: "market",
              hover: {
                title: { text_id: "core:text.location.market.title", args: {} },
                description: { text_id: "core:text.location.market.hint", args: {} },
                image_resource_id: "core:resource.location.market.icon",
              },
              binding: {
                command_id: "core:command.location.inspect",
                args: { location_id: "core:location.city.market" },
              },
            },
          ],
        },
      },
      buttons: {
        schema_id: "core:schema.ui_field.button_list.v2",
        value: {
          items: [
            {
              key: "inventory",
              text: { text_id: "core:text.screen.main.inventory", args: {} },
              binding: {
                command_id: "core:command.screen.open_inventory",
                args: {},
              },
            },
          ],
        },
      },
    },
  },
  overlays: [],
  modals: [],
}
```

Каждый display text в Screen Fields, repeated items и hover content использует один `TextSpec`. `style` optional и выбирает semantic typography token active UE theme; physical font, size, RGB и asset locator boundary не пересекают. Localized value по `text_id` может содержать validated Text Pipeline markup. String arguments всегда экранируются до markup parse.

- `ui_instance_id` уникален для document lifecycle.
- `revision` monotonically increases внутри instance.
- Одновременно существует не более одного main route.
- `route`, каждый overlay и modal являются Screen Instance.
- Overlays существуют, пока присутствуют в desired document.
- Modals — ordered stack; interactive только top eligible modal.

## Screen Instance model

| Field | Rule |
|---|---|
| `instance_key` | Stable local identity в layer; не display text и не array index при возможном reorder |
| `screen_id` | Stable ID Screen Registry entry |
| `fields` | Полная map `field_id → { schema_id, value }` для этой revision |
| `element_id` | Optional authored provenance внутри field item, если нужен diagnostics/result placement |

Lua не передаёт children, Widget Blueprint class или физические Widget names. Допустимые поля и их schemas определяются Dynamic Screen Elements конкретного Screen Template. Repeated field items обязаны иметь deterministic `key` из durable identity.

## Runtime binding registry

Widget не получает `command_id`, bound args или Lua callback как authoritative input payload. Во время apply reconciler создаёт session-scoped binding record:

```json5
{
  binding_handle: "runtime@17:91",
  session_generation: 17,
  ui_instance_id: "ui@17:8",
  revision: 42,
  node_key_path: ["route", "main", "buttons", "inventory"],
  element_id: "core:screen.main#widget.inventory",
  command_id: "core:command.screen.open_inventory",
  bound_args: {},
  input_schema_id: "core:schema.ui.button_input.v1",
}
```

`binding_handle` — opaque transient ID, не Stable ID и не save data. Он уникален внутри session generation и резолвится только через current UI binding registry. Историческое имя `node_key_path` означает stable presentation path `layer → screen instance → field → item`; оно не раскрывает физический UMG tree. Optional `element_id` сохраняет authored provenance.

Presentation передаёт physical Widget только `binding_handle` и визуальные values. Registry инвалидируется до удаления/replacement Screen Instance или field item, смены interactive revision и session teardown. Blueprint не может создать binding record или заменить его `command_id`/`bound_args`.

Publication является atomic: registry сначала валидирует весь candidate set, включая generation, monotonic revision, unique paths, commands, bound args и input fields. Ошибка любого record оставляет current revision и все её handles без изменений. Успешная публикация целиком заменяет current set; handles предыдущей revision становятся invalid, а handles предыдущей session generation — stale.

## Reconciliation

1. Validate envelope, revision, Screen Registry entries, Screen Field schemas и command bindings.
2. Построить candidate binding records, но не публиковать их.
3. Match Screen Instances по layer + `instance_key`.
4. Same identity + same `screen_id` может переиспользовать existing Screen Widget и применить полный field set.
5. Changed `screen_id` заменяет Screen Widget class через registry.
6. Removed/replaced instances и field items логически отключаются до exit animation.
7. Commit prepared binding candidate и atomically publish document/input-ready revision только после successful apply всех Screen Fields.
8. Запустить optional enter/exit animations после logical commit.

Exit animation не продлевает logical input lifetime removed Screen Instance или field item. Failed candidate не оставляет частично обновлённый interactive screen; Presentation восстанавливает previous values либо полностью rebuilds previous document.

## Full update policy

Lua всегда отправляет complete document/revision, не operations patch. Internally Presentation может вычислять diff. Boundary-level partial patch, JSON Patch и mutation operations отсутствуют.

## Route/layer rules

- Route replacement создаёт новый route identity или новый UI instance согласно navigation policy.
- Overlay по умолчанию не блокирует route; его fields могут явно ограничивать input scope согласно registered layer policy.
- Modal блокирует нижние layers; owner removal каскадно удаляет его modals.
- Unknown `screen_id`, invalid registry class или incompatible field contract делает candidate document invalid до interactive apply.

## UI-local state

UE хранит focus, hover, pressed, scroll offset, animation progress и local tooltip state. Lua хранит только значение, влияющее на gameplay или обязанное пережить reconstruction. Такое значение возвращается Semantic Input/Command и становится canonical state.

Open screen/focus/animation progress не входят в save. После load UI строится из canonical state заново.

## Text and rich text

TextSpec:

```json5
{
  text_id: "core:text.dialogue.aria.greeting",
  args: { player_name: "..." },
}
```

Пунктуация и word order принадлежат localized message. RichText может использовать только semantic Text Pipeline tags и зарегистрированные theme tokens. Например, `<color=warning>`, `<size=huge>` и `<style=inventory>` выбирают logical token, но не содержат raw RGB, numeric font size, font/class или asset path. Новый token value добавляется в theme data без изменения C++.

Interactive fragment использует `<interactive id="market">…</interactive>` внутри localized message. `span_id` является local deterministic identity и разрешает отдельный span descriptor; tag никогда не содержит `command_id`, callback name, `resource_id` или raw asset path. Translator может перемещать complete tag и менять его visible content. Position offsets и post-localization word search запрещены.

Span descriptor содержит optional UE-local hover payload и optional Command binding. Presentation разрешает `TextSpec` и `resource_id`, создаёт opaque handle для clickable span и передаёт Widget только values/handle. Hover state/popover не входит в document identity или save. Добавление/удаление span участвует в full reconciliation и инвалидирует удалённый handle до exit animation.

## Current vertical slice

Private `FGV2UiBindingRegistry` реализует prepared binding candidate и отдельный commit. Lua command handler публикует portable Screen request с `screen_id = "core:screen.test"` и generic fields; UE schema adapters готовят typed values и handles, runtime создаёт generic `UGV2ScreenWidgetBase`, применяет полный field set и только после success делает candidate revision current. `WBP_Testscreen` наследует `WBP_ScreenBase` и не содержит Lua callback.

Vertical slice публикует generic `screen_id + fields[]` envelope. `FGV2ScreenFieldAdapterRegistry` поддерживает пять schemas: `core:schema.ui_field.rich_text.v3`, `core:schema.ui_field.button_list.v2`, `core:schema.ui_field.checkbox.v1`, `core:schema.ui_field.input_field.v1` и `core:schema.ui_field.dropdown_select.v1`; их typed values определены в [Blueprint Screen Template Contract](ScreenTemplates.md#dynamic-screen-element-contract). Runtime façade, coordinator и Screen Registry не содержат concrete field names. Текущий `UGV2RuntimeSubsystem` публикует один active Screen и при следующем accepted Screen request полностью строит replacement из Lua desired state. Layered reconciliation по `instance_key` для route/overlay/modal ещё не реализован; он расширит instance/layer lifecycle без замены field boundary.

## Verification status

Текущая automation обязана покрывать и покрывает:

- Screen Registry validation и inheritance `WBP_ScreenBase → WBP_Testscreen`;
- наличие всех пяти schema adapters и отклонение unknown schema;
- полный five-field contract concrete Screen, missing/unknown/duplicate field и schema mismatch;
- Lua → generic fields → typed adapters → UMG round-trip для RichText, ButtonList, Checkbox, InputField и DropdownSelect;
- rebuild единственного active Screen из обновлённого Lua desired state после Checkbox/InputField/DropdownSelect Semantic Input;
- binding candidate publication atomicity, сохранение предыдущего set при rejected candidate, superseded handle invalidation, stale generation rejection и monotonic revision.

Следующие проверки являются planned acceptance criteria полноценного layered reconciler и не считаются существующими тестами до реализации соответствующего lifecycle:

- same-instance Screen reuse и replacement при changed `screen_id`;
- overlay/modal ordering и input eligibility верхнего modal;
- logical removal до exit animation;
- atomic rollback полного document при field apply failure;
- full document rebuild after load;
- потеря UI-local state при reconstruction без gameplay mutation.
