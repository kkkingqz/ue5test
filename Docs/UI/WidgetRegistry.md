---
title: Widget Registry Contract
status: normative
version: 3.18
updated: 2026-09-10
depends_on:
  - ../Architecture/StableIDSpecification.md
  - ImageResources.md
decisions:
  - ../ADR/0001-authority-boundaries.md
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0012-centralized-ui-theme.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0016-png-suffix-image-metadata.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0040-universal-ui-property-pipeline.md
  - ../ADR/0043-presentation-apply-boundary.md
---

# Widget Registry Contract

> **Владеет:** базовым набором виджетов, универсальным конвейером свойств (`IGV2UiPropertyHost`), централизованной темой и правилами их использования.
> **Не владеет:** раскладкой конкретного экрана и игровыми данными в нём.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** prepared-effect bases и value-only roles — `Source/GV2PresentationApply/`; semantic Prepare, registries и composition bases — `Source/GV2/Public|Private/UI/`; материализация — `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp`.
> **Проверки:** `GV2.Runtime.Presentation.*`, `GV2.UI.StandardPropertyConsumers`, `GV2.UI.CapabilityObservabilityHarness`, `GV2.UI.CapabilityObservabilityCompositeSweep`, `GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure`, инвентарь `/Game/UI` в automation.

Widget Registry описывает reusable UI elements, их trusted C++/UMG adapters, capabilities и field schemas. Concrete root screens принадлежат отдельному [Screen Template contract](ScreenTemplates.md) и разрешаются Screen Registry, а не `widget_id` из Lua-authored tree.

## Ownership

- Registry владеет разрешённым adapter/factory mapping для reusable element type.
- `GV2ScreenFieldMaterializer` выполняет материализацию полей на основе скомпилированных схем репозитория (`GV2ContentCore::FCompiledUiFieldSpec`); C++ schema-specific адаптеры и их статический реестр физически удалены.
- Виджеты реализуют `IGV2UiPropertyHost` (`DescribeUiCapabilities`), связывая валидированные данные схемы со своим локальным состоянием через универсальные property consumers. Виджет, служащий top-level приёмником Screen Field, дополнительно реализует `IGV2ScreenFieldHost` (`GetScreenFieldId`): это четыре Location-композита, восемь адресуемых базовых элементов DUC-02 и `UGV2DeclaredCompositeWidgetBase`. Последний становится field host только когда Designer задаёт его `HostIdentity`; остальные reusable UI-kit виджеты остаются nested property hosts ([Screen Templates § Screen Field Host and Property Host contract](ScreenTemplates.md#screen-field-host-and-property-host-contract)).
- Concrete Screen Blueprint выбирает и размещает Screen Field Hosts в структуре дерева виджетов.
- Lua выбирает `screen_id`, значения fields и Command bindings, но не `widget_id` физического child Widget.

Registry не хранит gameplay-state и не выбирает command availability.

## Element registration

Логическая registration entry:

```json5
{
  widget_id: "core:widget.button_list",
  field_schema_id: "core:schema.ui_field.button_list.v2",
  factory_resource_id: "core:resource.widget.button_list.default",
  source_package_id: "core",
}
```

Entry регистрируется до registry freeze. Duplicate ID с несовместимым adapter/schema — fatal. `factory_resource_id` разрешается UE-side; raw asset locator не входит в Lua Screen Document.

`widget_id` используется authoring validation, diagnostics и mod extension policy. Он не заставляет Lua описывать physical Widget tree.

## Native adapters and Blueprint bases

```text
UGV2ScreenWidgetBase
UGV2TextWidgetBase            implements IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2RichTextWidgetBase        implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2RichTextPopoverWidgetBase  transient value sink for prepared RichText popovers; deliberately NOT IGV2UiStyleConsumer
UGV2ImageWidgetBase           implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2ButtonWidgetBase          implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2UiBindingTarget, IGV2ScreenFieldHost
UGV2CheckboxWidgetBase        implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2UiBindingTarget, IGV2ScreenFieldHost
UGV2InputFieldWidgetBase      implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2UiBindingTarget, IGV2ScreenFieldHost
UGV2DropdownSelectWidgetBase  implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2UiBindingTarget
UGV2ButtonListWidgetBase      implements IGV2UiStyleConsumer, IGV2UiPropertyHost
UGV2ProgressBarWidgetBase     implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2PortraitWidgetBase        implements IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2ModalWidgetBase           implements IGV2UiPropertyHost
UGV2TabContainerWidgetBase    implements IGV2UiPropertyHost
UGV2SeparatorWidgetBase       implements IGV2UiStyleConsumer
UGV2LoadingIndicatorWidgetBase implements IGV2UiStyleConsumer
```

Prepared-effect Widget bases, capability descriptors и value-only interfaces находятся в `GV2PresentationApply`; schema validation, semantic Prepare, composition bases и authority-aware registries остаются в `GV2`. Виджет сообщает поддерживаемое **физическое действие** отдельной ролью `IGV2Prepared*Target` (`PreparedApplyTargets.h`). Единственный `FGV2PresentationApply::Apply` маршрутизирует операцию по роли, а не по списку concrete classes: так derived/custom Widget получает тот же путь без новой ветки. Отсутствие роли — диагностируемое несоответствие, а не тихий no-op; множество объявленных ролей сверяется с множеством реализаций (`Tools/Testing/validate_central_style_runtime_boundary.py`). `IGV2UiPropertyHost` и `IGV2UiBindingTarget` наследуют `IGV2PreparedKeyTarget`/`IGV2PreparedBindingTarget`, поэтому новый host с `key` или `binding` не требует правки диспетчера (DUC-03).

DUC-02 made eight base elements addressable as a top-level Screen Field the same way the four Location composites already were: `UGV2TextWidgetBase`, `UGV2RichTextWidgetBase`, `UGV2ImageWidgetBase`, `UGV2ButtonWidgetBase`, `UGV2CheckboxWidgetBase`, `UGV2InputFieldWidgetBase`, `UGV2ProgressBarWidgetBase`, `UGV2PortraitWidgetBase` now implement `IGV2ScreenFieldHost` too, delegating `GetScreenFieldId()` to the same shared `HostIdentity` every `IGV2UiPropertyHost` carries (DUC-01) — no separate per-class field, no dedicated C++ subclass. The remaining reusable widgets (`UGV2DropdownSelectWidgetBase`, `UGV2ButtonListWidgetBase`, `UGV2ModalWidgetBase`, `UGV2TabContainerWidgetBase`, `UGV2RichTextPopoverWidgetBase`, `UGV2ListViewWidgetBase`) are collection/composite-ish or transient-projection widgets outside DUC-02's scope and remain addressable only as a nested property host (inside a `CollectionHost` composite or a parent's capability tree), not as an independent top-level Screen Field — см. [Screen Templates § Screen Field Host and Property Host contract](ScreenTemplates.md#screen-field-host-and-property-host-contract).

```text
WBP_ScreenBase (abstract)
└── WBP_Testscreen
    ├── DescriptionText: WBP_RichText
    ├── PlayerNameField: WBP_InputField
    ├── ClassSelectField: WBP_DropdownSelect
    ├── CheckboxField: WBP_Checkbox
    └── ButtonList: WBP_ButtonList

WBP_RichTextPopover
├── TitleText: UCommonTextBlock
├── DescriptionText: WBP_RichText
└── Icon: UImage (optional resource projection)
```

Blueprint отвечает за layout/composition/animation. Central theme задаёт default visual style. Native adapter применяет typed presentation value, управляет rebuild/local events и передаёт Semantic Input Adapter-у opaque binding handle.

## Universal Property Host architecture

Виджеты реализуют интерфейс `IGV2UiPropertyHost`, объявляя свои свойства через `DescribeUiCapabilities(FGV2UiCapabilityBuilder& Builder)`. Это заменяет ручные schema-specific DTO (`FGV2ButtonViewModel`, `FGV2ProgressBarViewModel` и т.д.) универсальным конвейером типизированных значений `FGV2PreparedUiValue` и специализированных потребителей свойств (Property Consumers).

### Варианты подготовленного значения (`FGV2PreparedUiValue`)

- **`Scalar`**: примитивные скалярные значения (`bool`, `int64`, `double`, `FString`).
- **`Key`**: нормализованный семантический идентификатор (`FName`).
- **`Text`**: разрешённая модель локализованного текста `FGV2TextViewModel`: `FText`, нормализованная разметка и готовые style/scale values; Apply не повторяет Theme lookup.
- **`Ref`**: Stable ID ресурса или сущности (`FString`).
- **`Binding`**: непрозрачный описатель привязки команды `FGV2UiBindingHandle`.
- **`Object`**: именованный набор свойств `FGV2PreparedUiObject` (`TMap<FName, FGV2PreparedUiValue>`). Используется как контейнер верхнего уровня экрана; прямое потребление свойствами виджетов запрещено (неприменимый вид в фабрике потребителей; композиты используют плоские маппинги свойств либо `CollectionHost`/`NestedScreen`).
- **`Array`**: упорядоченный массив значений `TArray<FGV2PreparedUiValue>`.

### Семейство стандартных потребителей (`FGV2PropertyConsumerFactory`)

Каждая объявленная в виджете capability привязывается к соответствующему потребителю:
- `FGV2ScalarPropertyConsumer` — обновляет скалярные свойства (например, `is_checked`, `value`, `percent`).
- `FGV2KeyPropertyConsumer` — передаёт имя capability вместе со значением, а маршрутизирует цель: `selected_key` → `DropdownSelect`, `default_tab_key` → `TabContainer`, любое другое имя, например `key`, → общий `IGV2UiPropertyHost::SetKey`/`GetKey`. Имя, принадлежащее другому хосту, хост **отклоняет** (`IsHostClaimedKeyCapability`), и применение сообщает `core:diagnostic.ui_consumer.unhandled_target`: успешный commit, записавший значение не в то поле, — ровно тот отказ, ради устранения которого конвейер существует. Множество таких имён сверяется с фактически объявленными capability рефлексией (`GV2.UI.PreparedKeyCapabilityRouting`). Новый хост, объявивший `key`-capability, работает без правок этого потребителя (DUC-03).
- `FGV2TextPropertyConsumer` — обновляет отображаемый текст через `UGV2TextPipeline` (`text`, `label`, `placeholder`).
- `FGV2RefPropertyConsumer` — разрешает и применяет визуальные ресурсы (`resource_id`, `frame_resource_id`) через `UGV2ImageResourceCatalog`.
- `FGV2BindingPropertyConsumer` — передаёт `FGV2UiBindingHandle` виджетам, реализующим `IGV2UiBindingTarget`.
- `FGV2KeyedCollectionPropertyConsumer` — управляет жизненным циклом (создание, переиспользование, удаление) дочерних элементов списков и коллекций (`items`, `buttons`, `options`, `meters`, `effects`, `characters`).
- `FGV2RichTextSpansPropertyConsumer` — готовит и связывает интерактивные текстовые фрагменты со своими дескрипторами и биндингами.

### Двухфазный конвейер мутаций

Мутация любого `IGV2UiPropertyHost` разделена на две строгие фазы:
1. `PrepareUiHostProperties(Value, PrepareContext, OutPlan, OutError)`: выполняет полную валидацию, разрешение ресурсов, текста, central style и дочерних коллекций **off-tree**, формируя иммутабельный план и prepared presentation transaction. Ни один физический виджет UMG на этой фазе не модифицируется.
2. `CommitUiHostProperties(Plan)`: атомарно применяет подготовленный план ко всем потребителям свойств.

## Implemented vertical slice API

| Native class | Role & Capabilities | Associated schema¹ | Required `BindWidget` |
|---|---|---|---|
| `UGV2ScreenWidgetBase` | Screen orchestrator (`PrepareScreenFields`, `CommitScreenFields`, `CanApplyScreenFields`, `ApplyScreenFields`) | Aggregate contract | Dynamic hosts находятся через Widget tree по `IGV2ScreenFieldHost` |
| `UGV2TextWidgetBase` | Property host: `text` (Text) | `core:schema.ui_field.text.v1` — addressable (DUC-02) | `TextBlock: UCommonTextBlock` |
| `UGV2RichTextWidgetBase` | Property host: `text` (Text), `spans` (RichTextSpans) | `core:schema.ui_field.rich_text.v3` — addressable (DUC-02) | `RichTextScrollBox: UScrollBox`, `RichTextBlock: UCommonRichTextBlock` |
| `UGV2RichTextPopoverWidgetBase` | Presentation popover: `InitializePopover(FGV2RichTextHoverViewModel, FPreparedRichTextStyle)` — единственная точка входа, стиль обязателен | Transient tooltip projection | `PopoverBorder: UBorder`, `PopoverWidth: USizeBox`, `TitleText: UCommonTextBlock`, `DescriptionText: WBP_RichText`; optional `Icon` |
| `UGV2ImageWidgetBase` | Property host: `resource_id` (Ref), `key` (Key); `ApplyResolvedImageResource` (принимает уже разрешённый ресурс — `PSC-10C`) | `core:schema.ui_field.image.v1` — addressable (DUC-02) | `Image: UImage` |
| `UGV2ButtonWidgetBase` | Property host: `text` (Text), `binding` (Binding), `key` (Key); implements `IGV2UiBindingTarget` | Leaf interaction element — addressable (DUC-02), no dedicated top-level schema yet | `LabelText: UCommonTextBlock` |
| `UGV2CheckboxWidgetBase` | Property host: `key` (Key), `text` (Text), `is_checked` (Scalar), `binding` (Binding); `SubmitCheckboxState(bool)` | `core:schema.ui_field.checkbox.v1` — addressable (DUC-02) | `Checkbox: UCheckBox`, `LabelText: UCommonTextBlock` |
| `UGV2InputFieldWidgetBase` | Property host: `key` (Key), `label` (Text), `placeholder` (Text), `value` (Scalar), `binding` (Binding); `SubmitTextValue(FString)` | `core:schema.ui_field.input_field.v1` — addressable (DUC-02) | `EditableTextBox: UEditableTextBox`; optional `LabelText: UCommonTextBlock` |
| `UGV2DropdownSelectWidgetBase` | Property host: `placeholder` (Text), `selected_key` (Key), `options` (CollectionHost), `binding` (Binding); `SubmitSelection(FName)` | `core:schema.ui_field.dropdown_select.v1`† | `HeaderButton: UGV2ButtonWidgetBase`, `PopupBorder: UBorder`, `PopupSizeBox: USizeBox`, `OptionsScrollBox: UScrollBox` |
| `UGV2ButtonListWidgetBase` | Property host: `items` (CollectionHost для кнопок) | `core:schema.ui_field.button_list.v2`† | `ButtonContainer: UVerticalBox` |
| `UGV2ProgressBarWidgetBase` | Property host: `percent` (Scalar), `label` (Text), `key` (Key) | `core:schema.ui_field.progress_bar.v1` — addressable (DUC-02) | `ProgressBar: UProgressBar` |
| `UGV2PortraitWidgetBase` | Property host: `resource_id` (Ref), `frame_resource_id` (Ref), `key` (Key) | `core:schema.ui_field.portrait.v1` — addressable (DUC-02) | `PortraitImage: UImage`, `FrameImage: UImage` |
| `UGV2ModalWidgetBase` | Property host: `title` (Text), `content` (Text), `buttons` (CollectionHost), `backdrop_close_action` (Binding) | `core:schema.ui_field.modal.v1`† | Content layout and slot hosts |
| `UGV2TabContainerWidgetBase` | Property host: `default_tab_key` (Key), `tabs` (CollectionHost) | `core:schema.ui_field.tab_container.v1`† | Tab header container and content slot |
| `UGV2SeparatorWidgetBase` | Только central style | Purely visual | `SeparatorSizeBox: USizeBox`, `SeparatorImage: UImage` |
| `UGV2LoadingIndicatorWidgetBase` | Только central style | UE-local operation state | `LoadingIndicator: UCircularThrobber` |

¹ Схема, разделяющая форму capability-дерева этого класса — не то же самое, что "виджет сконфигурирован как Screen Field". «Addressable (DUC-02)» — класс реализует `IGV2ScreenFieldHost`, делегируя `GetScreenFieldId()` в общий `HostIdentity` (DUC-01); может быть top-level приёмником Screen Field, если `HostIdentity` настроен на конкретном размещении, — проверено `GV2.Runtime.Presentation.LuaCreatesRegisteredScreen` (виджет `GreetingText` в `WBP_Testscreen`) сквозь реальный Lua/materializer pipeline. † Класс реализует только `IGV2UiPropertyHost`, не `IGV2ScreenFieldHost`: он не может быть top-level приёмником этой схемы через `GetScreenFieldId()` сейчас — это collection/composite-виджет вне границ DUC-02; схема остаётся валидируемым, протестированным на уровне `PrepareUiHostProperties`/`CommitUiHostProperties` контрактом и может применяться как вложенное свойство composite'а (`CollectionHost` entry). Классы, реализующие `IGV2ScreenFieldHost` в текущем коде — не остаётся ни одного Location-композита с собственным C++-классом; все четыре сведены к генерическому `UGV2DeclaredCompositeWidgetBase` (охватывает `top_bar` через `WBP_LocationTopBar`, DUC-08, `scene` через `WBP_SceneView`, DCA-05, `player_status` через `WBP_PlayerStatusPanel`, DCA-06, и `commands` через `WBP_CommandPanel`, DCA-07) плюс восемь адресуемых base-элементов, отмеченных «addressable (DUC-02)» выше (см. [Screen Templates](ScreenTemplates.md#screen-field-host-and-property-host-contract)).

`UGV2ScreenWidgetBase` централизует discovery, двухфазную подготовку и применение Screen Fields. Он не содержит concrete Screen fields или `screen_id` branches. Unset `ScreenFieldId` исключает nested Widget из aggregate contract.

### Охват capability sweep (DUC-04)

`GV2.UI.CapabilityObservabilityCompositeSweep` обязан получать source set не из списка имён в тесте, а reflection-ом: каждый прямой native implementation boundary `IGV2UiPropertyHost` из `/Script/GV2` требует реальный `WBP_*`-потомок в UI package roots. Отсутствующий Blueprint делает automation красной; тестовые подделки исключаются только явной UCLASS metadata `GV2TestOnly` и проверяются отдельными negative tests.

### Объявляемый generic composite (DUC-05)

`UGV2DeclaredCompositeWidgetBase` — единственный generic base для композита, чьи capability объявляет Designer. Он хранит редактируемый рядом с WidgetTree плоский `DeclaredCapabilities` из `PropertyName`, `ChildWidgetName`, `Kind` и (`GBH-06`) зависящих от `Kind` параметров — `NumberMin`/`NumberMax`, `IntMin`/`IntMax`, `TargetKind`, `ChildCapabilityName`; дерево capability строится только из этих полей, а не выводится из реализации ребёнка. Сверка с capability ребёнка остаётся самостоятельной задачей DUC-07/GBH-06.

`Kind` соответствует всем прямым consumer-backed маршрутам после PCC-05: `Boolean`, `Integer`, `Number`, `String`, `Key`, `Text`, `ResourceRef`, `Binding`, а для `Array` — `CollectionHost`, `RichTextSpans`, `NestedScreen`. `Null` и прямой `Object` намеренно не имеют значения Designer enum: для них consumer отсутствует или вид неприменим; object/array composition остаётся плоским mapping либо специальным collection/nested route.

**GBH-02A/B:** `RichTextSpans` помечен `UMETA(Hidden)` и не выбираем в Designer picker — единственное существующее доказательство идёт через **нативную** capability самого `UGV2RichTextWidgetBase`, а не как делегирование от composite к ребёнку. `CollectionHost` был Hidden по той же причине (`DescribeUiCapabilities`' ветка вызывала `AddCustom(...)`, у которого не было параметра `EntryWidgetClass` — первый элемент по-настоящему пустой коллекции создать было невозможно, REM-05), но `GBH-02B` вернул его в selectable: ветка теперь вызывает `AddKeyedCollection(...)` с `EntryWidgetClass`/`KeyPropertyName` из declaration, а item capability читается из `EntryWidgetClass`'s собственного CDO `DescribeUiCapabilities` — тот же приём, которым `UGV2ButtonListWidgetBase` уже пользуется для своей entry-коллекции. `NestedScreen` остаётся selectable — доказан DUC-09/10/11. `FGV2DesignerCapabilityKindGate::ValidateAllKindsClassified` (`GV2DeclaredCompositeWidgetBase.h`) — completeness-гейт по всем значениям enum, симметричный `FGV2PropertyConsumerFactory::ValidateAllKindsHandled` (PCC-05): новое значение enum без явной классификации (Hidden с причиной либо в списке доказанных) проваливает гейт, а не молча становится selectable.

`ChildWidgetName` — точная декларация target в instance WidgetTree. Если named child отсутствует, preflight `PrepareUiHostProperties` обязан остановить candidate до публикации с `core:diagnostic.ui_consumer.missing_target`; fallback на сам host допустим только для capability с `TargetName == NAME_None`. `WBP_DeclaredCompositeFixture` фиксирует production-конфигурацию `label → LabelText: WBP_Text` и одновременно является обязательной WBP-fixture capability sweep. DUC-05 не меняет существующие Location-композиты.

DUC-06 фиксирует, что Designer declaration определяет только **плоскую** поверхность schema/capability пары: `PropertyName` становится одним top-level именем в object schema с соответствующим standard kind. В частности, `WBP_DeclaredCompositeFlatFixture` объявляет `day → DayText` (Text) и `value → ValueBar` (Number), а принадлежащая `textsystem` schema содержит непосредственно `day: text` и `value: number`. Schema не раскрывает `WBP_Text.text` либо `WBP_ProgressBar.percent`; реальная compatibility-проверка выполняется между отдельно загруженной repository schema и capability, построенными из Designer declaration. DUC-07 отдельно сравнит объявленный вид с capability самого ребёнка.

DUC-07/GBH-06: помимо `Schema ⊆ Capabilities`, `PrepareUiHostProperties` сверяет declared `Kind` composite-свойства с capability, которую самостоятельно объявляет её `RendererControl`-target (если тот сам `IGV2UiPropertyHost`) — `ResolveDelegatedChildCapability` (`GBH-06`, заменяет прежний `DoesCapabilityTreeSupportKind`) резолвит конкретную capability ребёнка: по явному `ChildCapabilityName`, иначе по виду (ровно один кандидат), иначе по `FallbackNameHint` (собственное имя composite-свойства), иначе — отказ. Несовпадение вида отклоняется с `core:diagnostic.ui_consumer.target_kind_mismatch`; неразрешимая неоднозначность (ребёнок с двумя capability одного вида, ни явный селектор, ни совпадение по имени не помогли) — с `core:diagnostic.ui_consumer.ambiguous_child_capability`. `CollectionHost`/`NestedScreen`/`CustomControl` targets (репитеры, nested-screen слоты) из этой сверки исключены — они не обязаны самообъявлять capability того же смысла, которым их адресует родитель.

Каждый такой boundary обязан предоставлять в Designer тот же `UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties)) FGV2UiPropertyHostState PropertyHostState`, что и остальные хосты. Sweep проверяет тип и Designer metadata этого состояния, применяет два различных значения `HostIdentity` и читает их обратно; для `IGV2ScreenFieldHost` дополнительно проверяет делегирование `GetScreenFieldId()` в это же значение. Это обычная проверяемая способность property host, а не отдельный per-class protocol. `WBP_Modal` наследует `UGV2ModalWidgetBase`, содержит его обязательные renderer targets и проходит тот же production sweep.

Repeated-field items обязаны иметь deterministic `key`. Общий `FGV2KeyedCollection` и `FGV2KeyedCollectionPropertyConsumer` владеют create/reuse/reorder/remove lifecycle. Ошибка подготовки candidate collection сохраняет предыдущих children. Runtime class/path из Lua отсутствует.

## Central style contract

`DA_UITheme_Default : UGV2UiTheme` является source of truth default visual values UI-kit. `UGV2UiThemeSettings.ThemeAsset` выбирает identity active theme через UE-only project config, но резолюция происходит один раз — при построении session content snapshot (ADR-0043 D1), не заново на каждый `Commit`. Prepare получает Theme только через `FGV2PresentationPrepareContext` и переносит в transaction готовые CommonUI/Slate classes, brushes, colors, spacing и scale policies. Theme object, token lookup и configured accessors Apply-фазе недоступны. Lua, headless runtime и Screen Field DTO не получают asset locator или theme UObject.

`IGV2UiStyleConsumer` является marker-интерфейсом фактических runtime style targets. Их множество перечисляет reflection/inventory gate; каждый target обязан иметь ветку в `GV2CentralStylePreparer` и exhaustive Apply visitor. `DropdownSelect` владеет стилем собственного поддерева, поэтому общий обход не стилизует его `HeaderButton` второй раз. Новые collection entries и nested tabs готовят свою central-style transaction до публикации и применяют её после commit дочерних свойств. Hover-popover создаётся позже, но получает сохранённый prepared payload владельца и применяет его новой value-only transaction без повторного Prepare.

Viewport-зависимые физические значения обязаны обновляться через `ViewportRefresh` той же transaction façade. Фактическое множество таких Widget-классов выводится из canonical text/scale call sites (`validate_viewport_refresh_coverage.py`); каждый реализует `IGV2PreparedViewportRefreshTarget` и пересчитывает только font/popup geometry из сохранённых prepared values. Повторный central-style Prepare или document reconcile при resize запрещён: он заново принял бы semantic decisions и сбросил бы UI-local state. Production test `GV2.Runtime.Presentation.CommittedPresentationRespondsToViewportResize` выполняет реальный engine resize и проверяет изменение шрифта без замены committed Widget.

Theme обязан задавать:

- `TextStyle`, `RichTextStyle`, `ButtonStyle`, `ButtonLabelStyle`, `CheckboxStyle`, `CheckboxLabelStyle`;
- `InputFieldStyle`, `InputFieldLabelStyle`;
- `RichTextInteractiveStyle`, `RichTextPopoverClass`, `RichTextPopoverBackground`, `RichTextPopoverPadding`, `RichTextPopoverMaxWidth`, `RichTextPopoverMaxHeight`;
- `ButtonListItemPadding` и `ImageTint`;
- `ProgressBarStyle` и `ProgressFillColor`;
- `SeparatorBrush` и `SeparatorThickness`;
- `LoadingIndicatorBrush`, `LoadingIndicatorPieces`, `LoadingIndicatorPeriod`, `LoadingIndicatorRadius`;
- `DropdownHeaderStyle`, `DropdownPopupBackground`, `DropdownPopupPadding`, `DropdownMaxPopupHeight`, `DropdownOptionItemPadding`.

## Unified Text Pipeline

Любой runtime-authored display text пересекает portable boundary как:

```json5
{
  text_id: "core:text.screen.test.description",
  args: { player_name: "Игрок" },
  style: "inventory", // optional local theme token
}
```

`text_id` является Stable ID kind `text`; `style` является lowercase local token, а не Stable ID, UE class или asset path. Отсутствующий `style` разрешается через `DefaultTextStyleToken` active theme. Headless runtime сохраняет `TextSpec` unresolved.

`UGV2TextPipeline` является единственной точкой, которая обязана:

1. разрешить `text_id` в localized `FText`;
2. экранировать string arguments и выполнить typed formatting;
3. проверить и нормализовать semantic markup;
4. разрешить style/color/size tokens через active theme;
5. передать renderer-у готовую typography и flat runs.

Шаги 1–4 — semantic resolution и принадлежат Prepare/материализации (тот же принцип, что `STATUS-012` уже закрыл для image resources: Prepare разрешает, подготовленное значение несёт готовый результат). Шаг 5 (Apply) обязан только применить уже разрешённую typography — не выполнять шаги 1–4 заново и не обращаться к active theme напрямую (`ADR-0043` D3, закрывает `PAH-R1`).

Theme хранит `TextCatalog`, `TextStyleTokens`, `TextColorTokens`, `TextSizeTokens` и `DefaultTextStyleToken`. Добавление конкретного token или `text_id` является data change и не требует изменения C++. Duplicate/unknown token, missing `text_id` и style без configured CommonUI class отклоняются до Widget mutation.

Text-bearing Widget Blueprint обязан либо наследовать native adapter, принимающий `FGV2TextViewModel`, либо составлять UI только из таких reusable components. Публичный Blueprint API, принимающий raw `FText` для runtime-authored content, запрещён. В частности, legacy `ApplyTextContent(FText)` и `ApplyRichTextContent(FText)` отсутствуют. UE-local editor labels и статический design-time текст, не зависящий от runtime/Lua/localization, не являются runtime-authored content.

`WBP_RichText` обязан автоматически переносить текст по фактически выделенной ширине. Используется `AllowPerCharacterWrapping`: обычный текст переносится по словам, а непрерывный oversized token при необходимости может быть разорван. `RichTextBlock` обязан находиться внутри вертикального `RichTextScrollBox`; если desired height текста превышает выделенную Screen Template высоту, содержимое прокручивается, а не изменяет размер экрана и не рисуется за границами блока. Каждое применение нового Screen Field сбрасывает scroll offset в начало. Concrete Screen Template обязан ограничить высоту экземпляра `WBP_RichText` layout-правилом (`Fill`, `SizeBox` либо эквивалентным), иначе ScrollBox не получает конечный viewport и не может определить overflow.

Composite Widget не может создавать собственный direct `UCommonRichTextBlock` для runtime-authored текста. Он обязан вкладывать `WBP_RichText` и передавать ему структурированное значение свойства через универсальный конвейер свойств либо использовать другой утверждённый pipeline component. Поэтому `WBP_RichTextPopover.DescriptionText` имеет тип `WBP_RichText`; `PopoverWidth` ограничивает как width, так и height через theme tokens, а `DescriptionText` занимает оставшуюся после title/icon высоту. Popover автоматически наследует wrapping, clipping, scrolling и reset-on-apply без отдельной реализации этих правил.

Текущий полный `WBP_*` inventory:

| Категория | Assets | Text Pipeline rule |
|---|---|---|
| Direct text owners | `WBP_Text`, `WBP_Button`, `WBP_Checkbox`, `WBP_InputField`, `WBP_RichText`, `WBP_RichTextPopover` | Native base применяет только `FGV2TextViewModel` через `UGV2TextPipeline` |
| Text composites | `WBP_ButtonList`, `WBP_DropdownSelect`, `WBP_Testscreen` | Текст существует только во вложенных pipeline components |
| Сейчас не содержат text primitives | `WBP_Image`, `WBP_LoadingIndicator`, `WBP_Modal`, `WBP_Portrait`, `WBP_ProgressBar`, `WBP_Separator`, `WBP_ScreenBase`, `WBP_GameShell` | При добавлении runtime text обязан использовать pipeline component/native adapter |

`WBP_Image` принимает только `resource_id` через generic Image Resource Catalog. Render mode и geometry metadata регулируются [Image Resource Contract](ImageResources.md); raw `FSlateBrush` mutation из Blueprint запрещена.

Canonical localized markup:

```text
Это <color=blue>строка образец</color> для <br/>
<size=huge>теста</size> и <style=inventory>инвентаря</style>.
Наведите на <interactive id="integration">интеграцию</interactive>.
```

Разрешены только `br`, `color`, `size`, `style`, `interactive`. Tags могут вкладываться и обязаны закрываться matching named tag; legacy closing `</>` временно принимается current vertical slice parser-ом. Raw RGB, numeric size, font name/class/path и произвольный UE decorator запрещены. Argument values являются escaped text и не могут внедрять markup.

Parser преобразует вложенные scopes в flat internal `<gv2 ...>...</>` runs для Slate. Этот internal markup запрещено хранить в localization/content. Interactive run наследует полностью разрешённый font/typeface/size/outline окружающего scope и добавляет только hyperlink interaction state.

Central style runtime-компонента является resolved operation общей `FGV2PreparedPresentationTransaction`. Theme resolution выполняется Prepare-фазой из session snapshot; физическое применение получает только prepared values. No-argument `IGV2UiStyleConsumer.ApplyCentralStyle()` удалён, а `NativePreConstruct` не применяет ни runtime style, ни image resource: `PSC-10C` распространил то же правило на `InitialResourceId`, так что lifecycle-колбэк остался чисто value-only design-time поверхностью. RichText run/interactive/popover styles тоже разрешаются заранее и входят в prepared RichText style payload; Slate decorator и создаваемый им popover не читают Theme при рендере.

Designer preview не является исключением к runtime authority boundary. При `IsDesignTime()` компонент может применить только сериализованные Widget/Blueprint defaults через pure value-only helper; configured Theme, snapshot, content lookup и loading ему недоступны. Preview остаётся структурной визуальной подсказкой, но не обязан воспроизводить выбранную runtime Theme до запуска Prepare. Отсутствующий required `BindWidget` остаётся failure; silent local fallback для production component запрещён.

Default CommonUI styles `BP_UIStyle_Text_Default` и `BP_UIStyle_ButtonLabel_Default` обязаны иметь explicit font object и typeface. Development fixture использует engine Roboto, typeface `Regular`; empty font/typeface запрещены, поскольку platform fallback может отображать Cyrillic неверными glyphs.

`InputFieldStyle.TextStyle` подчиняется тому же правилу и хранится только в active theme. Текущий default использует explicit engine Roboto `Regular` размером 16; Widget Blueprint не задаёт собственный font и не зависит от platform fallback.

`WBP_Text`, `WBP_RichText`, `WBP_Image`, `WBP_Button`, `WBP_Checkbox`, `WBP_InputField`, `WBP_ButtonList`, `WBP_DropdownSelect`, `WBP_ProgressBar`, `WBP_Separator` и `WBP_LoadingIndicator` составляют нейтральный baseline UI-kit. `WBP_RichTextPopover` является общей transient support surface интерактивного RichText. Добавление Widget в UI-kit не создаёт автоматически Screen Field schema: boundary schema добавляется только вместе с concrete presentation scenario.

## DropdownSelect contract

`core:schema.ui_field.dropdown_select.v1` состоит из resolved placeholder text, массива option items (каждый с `key` и resolved text) и единого opaque binding handle. `selected_key` (опционально) указывает текущую выбранную опцию.

Dropdown является composite Widget: `HeaderButton: WBP_Button` показывает выбранную опцию или placeholder и переключает popup; `PopupBorder` содержит ограничивающий высоту `PopupSizeBox` и `OptionsScrollBox` со списком option buttons. Open/close popup — UE-local visual state, не пересекающее Lua boundary. Все подписи проходят через `UGV2ButtonWidgetBase` и общий Text Pipeline.

Option items реализованы через `UGV2ButtonWidgetBase` с shared binding handle. Dropdown отключает automatic submission у header и option buttons. Клик по заголовку вызывает собственный обработчик переключения popup (`HandleHeaderActivated`); option key проверяется против applied options (ключ `dropdown_header` зарезервирован за заголовком и отклоняется в опциях), после чего composite выполняет ровно один submit общего handle с required `selected_key`. Промежуточный submit без control value и повторная отправка запрещены. Popup закрывается после `Accepted`, а также при изменении состава опций или выбранного ключа; при повторном применении неизменных свойств текущее состояние раскрытия списка сохраняется. Selected desired state меняется только после republish из Lua.

Input schema: `core:schema.ui_input.dropdown_selected.v1` — required string field `selected_key`.

## Interactive RichText contract

`core:schema.ui_field.rich_text.v3` состоит из resolved `FGV2TextViewModel` (`FText + style token`) и массива semantic spans. Visible word/phrase размечается только тегом `<interactive id="local_span_id">…</interactive>`. Position/range и поиск по отображаемой строке запрещены: localized message владеет word order и обязан сохранять тег вместе с переводимым содержимым.

Интерактивный run обязан наследовать font, size, typeface, outline и остальные typography-параметры текущего resolved scope. `RichTextInteractiveStyle` задаёт interaction colors, underline/button states и padding. Поэтому смена основного composite font, локального `style`/`size` scope или кириллицы не требует дублировать font в hyperlink style.

Каждый `span_id` обязан быть unique lowercase `snake_case`, присутствовать в markup минимум один раз и иметь declarative `hover` content либо opaque click binding. Unknown tag, dangling descriptor, duplicate ID, дополнительный tag attribute и malformed interactive markup отклоняют Screen Field до apply. Один span может встречаться в localized message несколько раз и использует один semantic binding item.

Hover title/description и optional `image_resource_id` являются value-only presentation data. Decorator создаёт tooltip лениво при открытии, а закрытие уничтожает transient popover. Hover/unhover не пересекают Lua boundary. Не разрешённый optional image скрывается без подмены raw asset path.

Popover **ничего не разрешает сам** (`PSC-10B`). Он создаётся по hover, то есть вне цикла prepare/commit экрана, и поэтому не может быть target собственной подготовленной операции — все значения ему передаёт создающий его RichText-виджет, который получил их из Prepare:

- title/description приходят уже resolved (`bHasResolvedPresentation`) и применяются approved Text Pipeline adapters;
- optional icon применяется из `FGV2RichTextHoverViewModel::ResolvedImageBrush`, разрешённого в Prepare через snapshot's image catalog; во время открытия tooltip никакой catalog не консультируется;
- собственный стиль (`FPreparedRichTextPopoverStyle`) и стиль вложенного описания приходят единственным аргументом `InitializePopover(Model, Style)` и записываются через ту же exhaustive Apply façade.

Собственный Blueprint resolver, direct brush mutation для runtime `resource_id`, а также инициализация popover без подготовленного стиля запрещены: `InitializePopover` отвергает вызов, у которого стиль не разрешён, вместо того чтобы отрисоваться нестилизованным.

Click span получает только `FGV2UiBindingHandle`. `SubmitSpanInteraction(span_id)` использует общий `SubmitUiInteraction(handle, {})`; decorator не хранит `command_id`, bound args или Lua callback. Reapply/reset/destruct RichText удаляет local span lookup, а смена UI revision инвалидирует handles через общий binding registry.

## Current WBP_Testscreen contract

`WBP_Testscreen` является physical UMG integration fixture и не вводит новый reusable `widget_id`. Его abstract parent — `WBP_ScreenBase`; template содержит шесть reusable-компонентов:

| Element name | Element | Screen Field |
|---|---|---|
| `DescriptionText` | `WBP_RichText` | Нет — static leaf |
| `CheckboxField` | `WBP_Checkbox` | Нет — static leaf |
| `ClassSelectField` | `WBP_DropdownSelect` | Нет — static leaf |
| `PlayerNameField` | `WBP_InputField` | Нет — static leaf |
| `ButtonList` | `WBP_ButtonList` | Нет — static leaf |
| `GreetingText` | `WBP_Text` | **Да** — `field_id = "greeting"`, `core:schema.ui_field.text.v1` |

Пять из шести компонентов не сконфигурированы Screen Field'ом (`HostIdentity` не задан) и остаются static leaves, как и раньше. `GreetingText` (DUC-02) — доказательство, что базовый элемент без выделенного C++-класса адресуем: `GetScreenFieldIds()` для этого экрана возвращает ровно `["greeting"]` (`GV2.Runtime.Presentation.LuaCreatesRegisteredScreen`). Lua `debug/start.lua` публикует `screens.create("core:screen.test", { greeting = { schema_id = "core:schema.ui_field.text.v1", value = {...} } })` — генерический `GV2ScreenFieldMaterializer`/`PrepareScreenFields`/`CommitScreenFields` доставляет значение до `GreetingText` тем же путём, что и `WBP_LocationScreen`. Пять static leaves продолжают получать состояние напрямую: interaction methods (`SubmitCheckboxState`, `SubmitTextValue(FString)`, `SubmitSelection(FName)`) каждого reusable-компонента вызываются и проверяются по отдельности; Checkbox стартует unchecked (UMG default, не результат Screen Field apply). Concrete class/path отсутствует в runtime source и Lua boundary; C++ не вызывает Lua Screen builder и не принимает `TSubclassOf`.

`WBP_Testscreen` остаётся полезной proving-ground fixture для reusable leaf-компонентов и их opaque interaction handles, а с DUC-02 — также и для Screen Field addressability базовых элементов. Основной, полноценный Screen-Field-driven vertical slice — `WBP_LocationScreen` (`textsystem:screen.location`), см. [Screen Templates § Current vertical slice](ScreenTemplates.md#current-vertical-slice).

Legacy parallel arrays и untyped interaction token не являются Screen Template API. Runtime использует универсальные property hosts с opaque binding handles.

## Mod support

- Data-only mod использует existing Screen Templates/elements и explicit extension slots.
- New element `widget_id` требует compatible cooked Pak/Mod Kit resource, mounted до repository build.
- Mod создаёт widget ID только в own namespace.
- Override core adapter mapping отключён в v1; visual customization использует theme/resource definitions и slots.
- New Screen Template регистрируется через разрешённую mod Screen Registry extension policy; Lua всё равно публикует только `screen_id` и fields.

## Failure and fallback

Unknown/incompatible element registration или Screen Field schema делает owning Screen Template/document invalid до interactive apply. Shipping использует system error surface; development diagnostic включает `screen_id`, `field_id`, `schema_id`, element class и source package provenance. Partial silent substitution запрещён.

## Evolution

Новый Screen Field schema добавляется только декларативно в данных (`*.schema.json5`) и материализуется переносимо; написание C++-адаптера запрещено. Нейтральный visual primitive может существовать без Screen Field schema. Изменение field value semantics требует нового schema ID; существующий опубликованный ID не переиспользуется.

## Tests

Tests покрывают duplicate registration, registry freeze, trusted factory resolution, no raw asset path, element schema validation, required/optional fields, duplicate/unknown field rejection, двухфазный atomic apply (Prepare/Commit), opaque handle propagation и отсутствие gameplay authority. `GV2ScreenFieldMaterializer` тесты фиксируют схематическую материализацию и отсутствие schema-specific C++ адаптеров и DTO; тесты `GV2.UI.StandardPropertyConsumers` и `GV2.UI.CapabilityObservabilityHarness` проверяют потребители свойств и их двухфазный жизненный цикл. `GV2.UI.CapabilityObservabilityCompositeSweep` reflection-ом сверяет каждый production property-host boundary с реальным `WBP_*` и проверяет общую identity surface. UI-kit test загружает active theme, проверяет native parents, mandatory `BindWidget`, `IGV2UiStyleConsumer` и successful style apply. Для `WBP_RichText` он дополнительно проверяет automatic/per-character wrapping и вертикальный `RichTextScrollBox`; для popover — composition через те же text/image leaf adapters и ограниченную theme height; для input/dropdown — submit через общий emitter и Lua-owned desired-state republish. Asset audit обязан перечислять все `WBP_*`, запрещать direct runtime text/content-image primitives в composites, local dynamic collection factories и direct Runtime Subsystem ingress вне общего emitter. Runtime source audit запрещает concrete `screen_id`/`field_id` branches.
