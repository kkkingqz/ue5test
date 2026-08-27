---
title: UI Document and Reconciliation
status: normative
version: 1.7
updated: 2026-08-27
depends_on:
  - ../Architecture/StableIDSpecification.md
  - ../Architecture/CommandsAndEvents.md
  - ScreenTemplates.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0035-ui-foundation-and-composition.md
  - ../ADR/0040-universal-ui-property-pipeline.md
---

# UI Document and Reconciliation

> **Владеет:** моделью желаемого UI-документа, маршрутами, слоями и правилами полной реконсиляции.
> **Не владеет:** физическим деревом виджетов и локальным визуальным состоянием.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** двухфазная многослойная реконсиляция через `FGV2LayeredUiReconciler` (`PrepareReconcile`/`CommitReconcile`) и `UGV2GameShellWidgetBase` (слои `background`, `location_content`, `character_presentation`, `core_interface`, `overlay_stack`, `modal_stack`); см. [Implementation Status](../Status/ImplementationStatus.md).
> **Проверки:** `GV2.UI.LayeredReconciliationContract`, `GV2.Runtime.Session.PreparedCommitAndFailureInjection`, `GV2.Runtime.Presentation.*`, `gv2-headless --self-test`.

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

Lua не передаёт children, Widget Blueprint class или физические Widget names. Допустимые поля и их schemas определяются элементами Screen Template, реализующими `IGV2ScreenFieldHost` и `IGV2UiPropertyHost`. Repeated field items обязаны иметь deterministic `key` согласно разделу [Repeated Element Identity](#repeated-element-identity).

### Декларативные UI-схемы как данные

Схемы полей интерфейса и переиспользуемых структур объявляются декларативными JSON5-файлами в репозитории контента (`GameData/<package>/schemas/`):

- **Домены схем**: `schema_domain: "ui_field"` (схема поля экрана) и `schema_domain: "ui_value"` (переиспользуемая вложенная структура).
- **Стандартные kinds**:
  - *Скалярные*: `bool`, `integer`, `number`, `string`;
  - *Семантические*: `key` (валидируемый ключ коллекции), `text` (`TextSpec`), `ref` (типизированная ссылка Stable ID на ресурс/сущность), `binding` (семантическая привязка команды);
  - *Структурные*: `object`, `array` (с поддержкой `keyed_by`, `min_items`, `max_items`), `screen_fields`, `schema_ref` (inline-включение именованной схемы без циклических зависимостей).
- **Замкнутость на всех уровнях (Closed Schemas)**: любой неизвестный ключ на любом уровне вложенности значения поля или схемы отклоняется типизированной ошибкой валидации (`core:diagnostic.ui_schema.value.unknown_field`).
- **Владение namespace**: пакет объявляет схемы исключительно своего namespace (`core:`, `textsystem:`, `rh:`, `<mod>:`). Попытка объявить схему чужого namespace отклоняется на стадии сборки репозитория.
- **Политика отказа для мода**: моды собирают схемы исключительно из стандартных kinds в данных без написания C++. Несовместимая или ошибочная схема мода отбраковывает мод, а не приводит к сбою сессии.

## Repeated Element Identity

Каждый элемент повторяемого поля (кнопки в `button_list`, интерактивные спаны в `rich_text`, опции `dropdown_select` и элементы будущих списков) обязан иметь проверяемую детерминированную идентичность `key`.

### Правила идентичности

1. **Обязательность**: отсутствие `key` у элемента повторяемого поля — фатальная ошибка документа (`UiElementKeyMissing`), отклоняемая до apply. Молчаливый откат к индексу массива запрещён.
2. **Область уникальности**: ключ обязан быть уникальным в пределах своего непосредственного контейнера (`field_id`). Полный путь элемента в дереве биндингов задаётся как `node_key_path`: `layer → instance_key → field_id → key`.
3. **Грамматика ключа**: ключ обязан состоять из строчных латинских букв, цифр, знаков подчёркивания, дефисов, точек, символов `@` и `:` (`[a-z0-9_.@:-]+`, длина от 1 до 192 символов). Нарушение грамматики отклоняется ошибкой `UiElementKeyInvalid`.
4. **Источники значения** (в порядке приоритета):
   - явный `key`, заданный автором интерфейса (`button_1`, `filter-all`);
   - Stable ID или instance ID доменной сущности (`rh:item.weapon.iron_sword`, `actor@42`);
   - канонический `command_id` с детерминированно сериализованными аргументами (`location_travel_rh_location_city_market`).
5. **Запрет вывода из текста**: ключ строго запрещено выводить из отображаемого или локализуемого текста (`text_id` или `TextSpec`, включая `text:...` и `<namespace>:text.<path>`). При попытке использовать текст как ключ документ отклоняется ошибкой `UiElementKeyTextDerived` / `TextDisallowedAsKey`.
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

#### Границы транзакционности и отката

1. **Контейнерная атомарность структуры**: `FGV2KeyedCollection` и `FGV2KeyedCollectionPropertyConsumer` гарантируют полную атомарность как структуры иерархии детей (`UPanelWidget`), так и состояния каждого дочернего виджета. Добавление, удаление и переупорядочивание детей фиксируются в контейнере только после успешного завершения фазы Prepare для всех элементов.
2. **Фаза preflight-валидации**: проверка обязательности ключей, их уникальности и вызов `CanApplyItem` происходят до создания новых виджетов и до подготовки состояния переиспользуемых виджетов. Ошибка на этой фазе не мутирует ни контейнер, ни живые виджеты.
3. **Двухфазная транзакционность элементов (Prepare/Commit)**: consumer коллекции (`FGV2KeyedCollectionPropertyConsumer` и двухфазная реконсиляция `FGV2KeyedCollection::ReconcilePrepared`) подготавливает мутации **всех** элементов off-tree до вызова Commit. При отказе подготовки любого элемента (включая валидацию дочерних свойств, отсутствие ресурсов или ошибки стилизации) фаза Commit не выполняется вовсе. Ни один переиспользованный виджет не остаётся частично или полностью мутированным, а их визуальные и физические значения строго сохраняют предыдущее состояние.

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

Реконсиляция документа выполняется атомарно через две фазы (`FGV2LayeredUiReconciler`):

1. **Фаза Prepare (`PrepareReconcile`)**:
   - Валидировать envelope, revision, Screen Registry entries и command bindings.
   - Сформировать candidate binding definitions через `GV2ScreenFieldMaterializer::PrepareBindingDefinitions`.
   - Подготовить кандидатный набор биндингов в `FGV2UiBindingRegistry::PrepareBindings`.
   - Материализовать поля через `GV2ScreenFieldMaterializer::BuildFields` (`FGV2ScreenFieldValue` с `PreparedValue` и `CompiledSchema`).
   - Сопоставить Screen Instances по `layer + instance_key`.
   - Для каждого экрана подготовить полный мутационный план (`UGV2ScreenWidgetBase::PrepareScreenFields`) off-tree.
   - Если подготовка хотя бы одного экрана в любом слое не удалась (включая несовпадение схемы, дублирующийся ключ глубокого ребёнка или незамкнутое поле), вся фаза Prepare отвергается: ни один старый экран не отсоединяется, ни один новый не присоединяется, и активный набор экранов остаётся неизменным.
2. **Фаза Commit (`CommitReconcile`)**:
   - Отсоединить удалённые и заменяемые экраны.
   - Присоединить новые экраны к соответствующим hosts `UGV2GameShellWidgetBase`.
   - Применить подготовленные мутационные планы экранов (`UGV2ScreenWidgetBase::CommitScreenFields`).
   - Применить маскирование интерактивности модальных слоёв (`ApplyInputMasking`).
   - Атомарно закоммитить подготовленные биндинги ревизии в `FGV2UiBindingRegistry`.
   - Вызвать `OnScreenFieldsApplied` для применённых экранов.
   - Запустить optional enter/exit animations.

Exit animation не продлевает logical input lifetime removed Screen Instance или field item. В случае отказа на стадии Prepare физическое дерево виджетов и активные биндинги вообще не затрагиваются; компенсирующий откат устранён физически. Failed candidate не оставляет частично обновлённый interactive screen.

## Full update policy

Lua всегда отправляет complete document/revision, не operations patch. Internally Presentation может вычислять diff. Boundary-level partial patch, JSON Patch и mutation operations отсутствуют.

Presentation source, который возвращает уже опубликованный `UiDocument` из `show_screen`, не публикует его повторно. Legacy route-shaped screen request нормализуется в document только когда он не содержит native envelope (`ui_instance_id`, `revision`, `route`); метatable convenience-поля документа не меняют это правило. Поэтому initial document сохраняет свою первую revision и проходит binding validation один раз.

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

Валидация и материализация полей документа выполняются универсальным материализатором `GV2ScreenFieldMaterializer` на базе скомпилированных UI-схем репозитория контента (`GV2ContentCore`); виджеты реализуют `IGV2ScreenFieldHost` и `IGV2UiPropertyHost` для связывания валидированных данных с UMG через раздельные фазы Prepare/Commit.

### Устойчивая идентичность LocationScreen

LocationScreen — один route instance. Его `screen_id` всегда `textsystem:screen.location`, а `instance_key` всегда `location`; `location.screen_ids` выбирает values definition и не участвует в route identity. Поэтому переход между локациями обязан обновлять Screen Fields в существующем widget, а не создавать новый. Изменение template или instance key допустимо только как явная route replacement.

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
