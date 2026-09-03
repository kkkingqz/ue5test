---
title: UI Document and Reconciliation
status: normative
version: 1.18
updated: 2026-09-03
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
  - ../ADR/0041-ui-commit-rollback-model.md
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
- **Идентичность array capability**: `array.keyed_by` обязан проецироваться в требуемый `KeyPropertyName` schema capability, а не только в boolean-признак keyed-identity. До `Ready` это имя сравнивается с `KeyPropertyName` widget capability тем же `SchemaContract ⊆ WidgetCapabilities` preflight; различие отклоняется `core:diagnostic.ui_capability.key_property_mismatch` (`KeyPropertyMismatch`). Совпадение одного default-имени без проекции схемы не является доказательством совместимости.
- **Замкнутость на всех уровнях (Closed Schemas)**: любой неизвестный ключ на любом уровне вложенности значения поля или схемы отклоняется типизированной ошибкой валидации (`core:diagnostic.ui_schema.value.unknown_field`).
- **Владение namespace**: пакет объявляет схемы исключительно своего namespace (`core:`, `textsystem:`, `rh:`, `<mod>:`). Попытка объявить схему чужого namespace отклоняется на стадии сборки репозитория.
- **Политика отказа для мода**: моды собирают схемы исключительно из стандартных kinds в данных без написания C++. Несовместимая или ошибочная схема мода отбраковывает мод, а не приводит к сбою сессии. **Текущая реализация ([`STATUS-008`](../Status/ImplementationStatus.md)):** это целевой контракт `ADR-0040` Decision 6/7, ещё не реализованный для `ui_field`/`ui_value` — `FGV2UiSchemaCache` резолвит их статичным сканированием фиксированных файловых корней, независимо от pinned `GameDataRepository`/package closure и accept/reject решения по модам. Разрыв не наблюдается, пока ни один текущий пакет (`core`/`textsystem`/`rh`) не является модом; открывается, когда мод впервые поставит `ui_field`/`ui_value` схему.

### Композиционные циклы вложенных экранов (DUC-11)

`schema_ref` запрещает циклические зависимости на уровне схем (см. выше). Вложенные экраны через `screen_id` в табах (DUC-09/10) — отдельная ось: `screen_id` — рантайм-строка, разрешаемая через Screen Registry, а не compile-time ссылка на класс Widget Blueprint.

- **Что уже гарантирует UMG**: `UWidgetBlueprint::IsWidgetFreeFromCircularReferences` (Editor-time) и компиляторская DFS-проверка (`HasCircularReferences`) полностью закрывают self-containment, размещённый в Designer, — Widget Blueprint не может напрямую или транзитивно содержать инстанс самого себя через WidgetTree.
- **Что UMG не видит**: путь `screen_id` из таба к экрану, разрешаемый Screen Registry в рантайме, не проходит через граф классов Widget Blueprint, который сканирует компилятор. Ничто на уровне UMG не мешает контенту объявить таб, чей `screen_id` — это уже подготавливаемый экран, напрямую или через промежуточный экран.
- **Guard DUC-11**: `ActiveCompositionChain` — упорядоченный путь `screen_id`, уже находящихся в процессе подготовки на этом стеке вызовов, засеиваемый в корне `FGV2LayeredUiReconciler::PrepareReconcile` значением `Instance.ScreenId`. Каждый таб перед разрешением в Screen Registry проверяется на присутствие своего `screen_id` в этой цепочке; совпадение — прямое (таб указывает на экран, который уже готовится) или косвенное (через один и более промежуточных экранов) — отклоняется с `core:diagnostic.ui_composition.cycle_detected` и отрендеренной цепочкой (`A -> B -> A`) до применения какой-либо мутации.
- **Глубина вложенности не ограничивается — это решение, а не умолчание.** Guard проверяет только повтор одного и того же `screen_id` на пути; сколь угодно длинная нециклическая цепочка вложенных экранов остаётся допустимой.

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
4. **Отказ ВНУТРИ фазы Commit ([ADR-0041](../ADR/0041-ui-commit-rollback-model.md), `GBH-09`/`GBF-04`/`GBF-05`)**: пункты 1–3 закрывают только отказ **до** начала Commit. Если Commit коллекции сам отказывает на элементе K после успешного коммита элементов `1..K-1` — а переиспользуемые элементы это тот же widget-объект, что уже стоял в контейнере, — их свойства к этому моменту уже физически замьютированы на новое значение, хотя `Panel`/`ActiveWidgetsByKey` ещё не продвинуты. Этот резидуальный разрыв `REM-02` закрыт: у каждого direct mutation есть inverse, подготовленный от committed tuple `LastCommittedProperties` + `LastCommittedSchema` + `LastCommittedSchemaId`; элементы `1..K-1` откатываются тем же Prepare/Commit pipeline (`FGV2KeyedCollectionPropertyConsumer::CommitWithFailureInjector`, `GV2PropertyConsumers.cpp`) прежде, чем Commit коллекции возвращает failure. Только после успешного physical inverse возвращается захваченный tuple; при ошибке inverse метаданные запрещено менять. То же правило рекурсивно действует для `RollbackFieldPlans` экранов документа и вложенных tab screens. Неполный tuple или недостроенный inverse отвергает Prepare typed diagnostic `core:diagnostic.ui_rollback.*`, а не допускает частичный Commit. Это доказано `GV2.UI.StandardPropertyConsumers` и schema-switch сценарием `GV2.UI.LayeredReconciliationContract`.

`GBF-07` добавляет structural gate `validate_ui_rollback_boundaries.py`: actual side выводит из всех `Source/GV2/Private/UI/*.cpp` definitions, соответствующих grammar `Commit*`/`AttachScreenToLayer` (включая `static`, UE macro и standard attributes перед `bool`), каждую точку применения. Каждый `Commit*`/attach root обязан иметь уникальный `rollback_boundary`; каждый bare `Commit` обязан явно быть `rollback_leaf` direct property mutation либо `rollback_delegate` к owning boundary. ID boundary и leaf/delegate обязан существовать в `EGV2UiRollbackBoundary`; каждый boundary ID ровно один раз сопоставлен с executable recovery rule: `ReplayInverse` для property/screen/document/collection/nested-tab и `RestoreStructure` для Shell attach; `GV2.UI.PrepareCommitAndFailureInjection` исполняет этот mapping по всему enum до `Count`. Поэтому новая definition внутри этой grammar, bare leaf, enum value без recovery rule или неверная классификация роняет CTest; self-test синтетически добавляет root в трёх declaration forms и bare leaf без marker, удаляет marker и удаляет recovery case. Gate по построению не анализирует произвольные функции вне этой grammar и не доказывает корректность тела recovery. Добавление fallible live mutation вне этой grammar запрещено без одновременного расширения scanner/enum и отдельной runtime fault-injection проверки.

Отчёт GBF-07: scanner выводит шесть sequence roots, а не получает их из таблицы ниже; таблица фиксирует их recovery owner и существующую runtime evidence.

| Source-derived root | `EGV2UiRollbackBoundary` | Recovery owner | Runtime evidence |
| --- | --- | --- | --- |
| `CommitUiHostProperties` | `PropertyMutation` | `RollbackCommittedMutations` replay previous plan | `GV2.UI.StandardPropertyConsumers` |
| `UGV2ScreenWidgetBase::CommitScreenFields` | `ScreenFields` | `RollbackFieldPlans` | `GV2.UI.LayeredReconciliationContract` |
| `FGV2LayeredUiReconciler::CommitReconcile` | `Document` | `RollbackFieldPlans` for committed screens | `GV2.UI.LayeredReconciliationContract` |
| `FGV2KeyedCollectionPropertyConsumer::CommitWithFailureInjector` | `KeyedCollection` | inverse item plan through `CommitUiHostProperties` | `GV2.UI.StandardPropertyConsumers` |
| `FGV2TabContainerTabsPropertyConsumer::CommitWithFailureInjector` | `NestedScreenTabs` | `RollbackFieldPlans` for committed child screens | `GV2.UI.LayeredReconciliationContract` |
| `UGV2GameShellWidgetBase::AttachScreenToLayer` | `ShellAttach` | `CommitReconcile` detaches new screens, reattaches replaced screens and rolls back fields | `GV2.UI.LayeredReconciliationContract` |

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
2. **Фаза Commit (`CommitReconcile`)** — порядок шагов подряд после PCC-07, потому что именно эта последовательность делает коммит документа атомарным на уровне ВСЕХ экранов, а не только каждого по отдельности:
   1. Закоммитить мутационные планы **всех** экранов, ничего ещё не отсоединяя и не присоединяя (`UGV2ScreenWidgetBase::CommitScreenFields`). `Commit` мутирует только собственные bound sub-widgets экрана по имени — это не требует, чтобы экран уже был присоединён к родительской панели, поэтому commit до attach/detach безопасен. `OnScreenFieldsApplied` и tab callbacks (`OnTabModelApplied`, `OnTabSelectionUpdated`, `OnTabChanged`) удалены: полный Asset Registry audit всех `.uasset` не нашёл Blueprint implementation, а callback внутри этой отменяемой фазы нарушал бы атомарность публикации. Test-only failure injector рекурсивно доходит до child Screen в tab container и получает путь `tabs.<tab_key>.<child_property>`; production в этот hook не передаёт callback. Отказ commit любого экрана здесь — нарушение инварианта: он не публикуется, а поскольку ничего ниже (detach/attach/`ActiveScreens`/layer interactivity) ещё не выполнялось, все остальные слои, их виджеты, биндинги и предыдущая ревизия `ActiveScreens` остаются буквально нетронутыми в смысле "их Commit не вызывался". Экраны, уже успешно закоммиченные РАНЬШЕ в этом же цикле (до отказавшего), этим предложением не покрыты сами по себе — `FGV2LayeredUiReconciler::CommitReconcile` откатывает их отдельно (см. п. 3 ниже, [ADR-0041](../ADR/0041-ui-commit-rollback-model.md), `GBH-10`), восстанавливая их через `RollbackFieldPlans` (`GV2ScreenWidgetBase.cpp`) прежде чем шаг 1 возвращает failure.
   2. Отсоединить старые экраны, заменённые новым widget instance (best-effort cleanup уже вытесняемого виджета, а не publish-шаг: неудача здесь логируется, а не останавливает commit).
   3. Присоединить новые (уже полностью закоммиченные) экраны к соответствующим hosts `UGV2GameShellWidgetBase`. `GBH-01` убрал каждую *предсказуемую* причину отказа `AttachScreenToLayer` в `PrepareReconcile`, поэтому отказ здесь — по определению непредсказуемый engine-level случай. `AttachScreenToLayer` обязан распространить результат `UPanelWidget::AddChild`: `nullptr` означает failure, а не успешное присоединение без physical child. `GBH-10` ([ADR-0041](../ADR/0041-ui-commit-rollback-model.md)) закрыл прежний остаточный разрыв (неудача присоединения экрана B после успешного присоединения A больше не оставляет A физически в дереве Shell): при отказе отсоединяются все новые экраны, уже присоединённые в этом же шаге, best-effort переприсоединяются экраны, отсоединённые шагом 2 ради замены, и откатываются Commit-мутации шага 1 для **всех** экранов плана (`RollbackFieldPlans`) — прежде чем `CommitReconcile` возвращает failure с деревом Shell и `ActiveScreens` точно на предыдущей ревизии.
   4. Отсоединить удалённые экраны, которых больше нет в документе (best-effort, та же логика, что и шаг 2).
   5. Закоммитить `ActiveScreens` — достигается только если каждый экран выше успешно закоммичен и присоединён.
   6. Layer Rules & Modal Interactivity (UIF-20): применить `SetLayerInteractive` по approved layers, либо (если есть модали) заблокировать все нижние слои и оставить интерактивным только верхний модальный.

`FGV2LayeredUiReconciler` не коммитит биндинги ревизии сам: это делает вызывающий `FGV2SessionCoordinator` отдельным вызовом `FGV2UiBindingRegistry::CommitPreparedBindings` после успешного `Reconcile`. Enter/exit animation экрана в текущем коде не реализованы (`STATUS-003`) — ни `FGV2LayeredUiReconciler`, ни вызывающий runtime не содержат animation-гейтинга; detach/attach выполняются синхронно.

В случае отказа на стадии Prepare физическое дерево виджетов и активные биндинги вообще не затрагиваются; компенсирующий откат устранён физически. Failed candidate не оставляет частично обновлённый interactive screen.

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

Валидация и материализация полей документа выполняются универсальным материализатором `GV2ScreenFieldMaterializer` на базе скомпилированных UI-схем репозитория контента (`GV2ContentCore`); top-level приёмник поля реализует `IGV2ScreenFieldHost` и `IGV2UiPropertyHost` для связывания валидированных данных с UMG через раздельные фазы Prepare/Commit — в текущем коде это только четыре Location-композита, см. [Widget Registry](WidgetRegistry.md#native-adapters-and-blueprint-bases).

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
- Реальный отказ `UPanelWidget::AddChild()` в production `CommitReconcile`: Shell tree, `ActiveScreens` и metadata остаются на предыдущей ревизии; CTest source gate перечисляет все `->AddChild(...)` в `AttachScreenToLayer` и требует propagation `nullptr` как failure;
- Сохранение UI-local состояния между ревизиями при переиспользовании экземпляра.
- Editor startup profile создаёт `WBP_Testscreen` через полный repository → Lua presentation → UI document → reconciliation pipeline и присоединяет его к `LocationContentHost` активной Game Shell.
