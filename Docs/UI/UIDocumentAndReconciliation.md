---
title: UI Document and Reconciliation
status: normative
version: 1.5
updated: 2026-08-20
depends_on:
  - ../Architecture/StableIDSpecification.md
  - ../Architecture/CommandsAndEvents.md
  - ScreenTemplates.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0035-ui-foundation-and-composition.md
---

# UI Document and Reconciliation

> **Владеет:** моделью желаемого UI-документа, маршрутами, слоями и правилами полной реконсиляции.
> **Не владеет:** физическим деревом виджетов и локальным визуальным состоянием.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** реализована многослойная реконсиляция через `FGV2LayeredUiReconciler` и `UGV2GameShellWidgetBase` (слои `background`, `location_content`, `character_presentation`, `core_interface`, `overlay_stack`, `modal_stack`); см. [Implementation Status](../Status/ImplementationStatus.md).
> **Проверки:** `GV2.UI.LayeredReconciliationContract`, `GV2.Runtime.Presentation.*`, `gv2-headless --self-test`.

UI-document — полная декларативная desired model Screen instances для одной revision. Lua строит его из canonical state и pinned repository; Presentation разрешает `screen_id` через Screen Registry и reconciles document с UMG instances.

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

Configured `WBP_GameShell` обязан наследовать native `UGV2GameShellWidgetBase` и содержать authored panel hosts `BackgroundHost`, `LocationContentHost`, `CharacterPresentationHost`, `CoreInterfaceHost`, `OverlayStackHost` и `ModalStackHost`. Каждый host обязан быть частью отображаемого Widget tree. Runtime запрещено создавать отсутствующий host как unattached fallback: отсутствие или несовместимость host отклоняет apply документа, чтобы session bootstrap не публиковал невидимый экран как успешный.

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
              key: "market",
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

Lua не передаёт children, Widget Blueprint class или физические Widget names. Допустимые поля и их schemas определяются Dynamic Screen Elements конкретного Screen Template. Repeated field items обязаны иметь deterministic `key` согласно разделу [Repeated Element Identity](#repeated-element-identity).

## Repeated Element Identity

Каждый элемент повторяемого поля (кнопки в `button_list`, интерактивные спаны в `rich_text`, опции `dropdown_select` и элементы будущих списков) обязан иметь проверяемую детерминированную идентичность `key`.

### Правила идентичности

1. **Обязательность**: отсутствие `key` у элемента повторяемого поля — фатальная ошибка документа (`UiElementKeyMissing`), отклоняемая до apply. Молчаливый откат к индексу массива запрещён.
2. **Область уникальности**: ключ обязан быть уникальным в пределах своего непосредственного контейнера (`field_id`). Полный путь элемента в дереве биндингов задаётся как `node_key_path`: `layer → instance_key → field_id → key`.
3. **Грамматика ключа**: ключ обязан состоять из строчных латинских букв, цифр, знаков подчёркивания, дефисов и точек (`[a-z0-9_.-]+`). Нарушение грамматики отклоняется ошибкой `UiElementKeyInvalid`.
4. **Источники значения** (в порядке приоритета):
   - явный `key`, заданный автором интерфейса;
   - Stable ID или instance ID доменной сущности (`rh:item.weapon.iron_sword`, `actor@42`);
   - канонический `command_id` с детерминированно сериализованными аргументами (`location_travel_rh_location_city_market`).
5. **Запрет вывода из текста**: ключ строго запрещено выводить из отображаемого или локализуемого текста (`text_id` или `TextSpec`). При попытке использовать текст как ключ документ отклоняется ошибкой `UiElementKeyTextDerived`.
6. **Запрет изменяемого состояния**: ключ не должен содержать мутирующие параметры состояния (`item#3`). Изменение ключа семантически означает появление новой сущности и приводит к уничтожению старого виджета.
7. **Уникальность**: два одинаковых ключа в одном контейнере отклоняют документ ошибкой `UiElementKeyDuplicate`.

### Реконсиляция контейнера

Внутри поля-контейнера сопоставление элементов между ревизиями документа происходит по паре `(тип элемента, key)`:

| Ситуация | Действие реконсилятора |
|---|---|
| Ключ совпал, тип совпал | Обновить данные виджета на месте; сохранить UI-local состояние |
| Ключ совпал, тип изменился | Пересоздать элемент новым классом виджета |
| Ключа нет в предыдущей ревизии | Создать новый дочерний виджет |
| Ключа нет в новой желаемой ревизии | Логически отключить и удалить виджет |
| Порядок изменился, набор ключей тот же | Переупорядочить виджеты в контейнере без пересоздания |

### Диагностика переиспользования (Dev Builds)

В dev-сборках (`!UE_BUILD_SHIPPING`) контейнеры ведут учёт переиспользованных и вновь созданных дочерних виджетов. Если контейнер за настраиваемое число ревизий (по умолчанию 3) ни разу не переиспользовал ни одного виджета при непустом списке, рантайм выводит предупреждение о нестабильных ключах. Эта диагностика не попадает в Shipping-сборки и не участвует в Presentation Digest.

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

## Implementation and architecture

Private `FGV2UiBindingRegistry` реализует prepared binding candidate и отдельный commit. `FGV2LayeredUiReconciler` сопоставляет экраны по паре `layer + instance_key`, переиспользует существующие виджеты при неизменном `screen_id` (сохраняя UI-local состояние) и заменяет класс виджета при его смене.

Слои Game Shell (`background`, `location_content`, `character_presentation`, `core_interface`, `overlay_stack`, `modal_stack`) валидируются реестром и оболочкой `UGV2GameShellWidgetBase`. Модальные окна в `modal_stack` блокируют ввод нижних слоёв, оставляя интерактивным только верхнее активное модальное окно.

`UGV2GameShellWidgetBase` разрешает слой только в соответствующий authored host. Динамическое создание host вне Widget tree запрещено: это скрывает ошибку Blueprint contract и приводит к логически применённому, но невидимому документу.

`FGV2ScreenFieldAdapterRegistry` поддерживает schemas базового набора; их typed values определены в [Blueprint Screen Template Contract](ScreenTemplates.md#dynamic-screen-element-contract).

## Verification status

Automation-тесты (`GV2.UI.LayeredReconciliationContract`, `GV2.Runtime.Presentation.*`, `gv2-headless --self-test`) проверяют:

- Валидацию слоёв Game Shell и отклонение неразрешённых слоёв;
- Наследование `WBP_GameShell` от `UGV2GameShellWidgetBase`, наличие шести authored hosts в отображаемом Widget tree и фактическое присоединение Screen к требуемому host;
- Валидацию Screen Registry и сопоставление классов экранов;
- Сопоставление Screen Instances по `layer + instance_key`;
- Переиспользование существующего виджета без пересоздания при неизменном `screen_id`;
- Замену класса виджета при смене `screen_id`;
- Блокировку интерактивности нижних слоёв при открытии модального окна и её восстановление при закрытии;
- Атомарность публикации: отказ кандидата не разрушает и не мутирует активный набор экранов и биндингов;
- Сохранение UI-local состояния между ревизиями при переиспользовании экземпляра.
- Editor startup profile создаёт `WBP_Testscreen` через полный repository → Lua presentation → UI document → reconciliation pipeline и присоединяет его к `LocationContentHost` активной Game Shell.
