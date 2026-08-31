---
title: Widget Registry Contract
status: normative
version: 3.0
updated: 2026-08-27
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
---

# Widget Registry Contract

> **Владеет:** базовым набором виджетов, универсальным конвейером свойств (`IGV2UiPropertyHost`), централизованной темой и правилами их использования.
> **Не владеет:** раскладкой конкретного экрана и игровыми данными в нём.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** `Source/GV2/Public/UI/`, `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp`, `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`.
> **Проверки:** `GV2.Runtime.Presentation.*`, `GV2.UI.StandardPropertyConsumers`, `GV2.UI.CapabilityObservabilityHarness`, `GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure`, инвентарь `/Game/UI` в automation.

Widget Registry описывает reusable UI elements, их trusted C++/UMG adapters, capabilities и field schemas. Concrete root screens принадлежат отдельному [Screen Template contract](ScreenTemplates.md) и разрешаются Screen Registry, а не `widget_id` из Lua-authored tree.

## Ownership

- Registry владеет разрешённым adapter/factory mapping для reusable element type.
- `GV2ScreenFieldMaterializer` выполняет материализацию полей на основе скомпилированных схем репозитория (`GV2ContentCore::FCompiledUiFieldSpec`); C++ schema-specific адаптеры и их статический реестр физически удалены.
- Виджеты реализуют `IGV2UiPropertyHost` (`DescribeUiCapabilities`) и `IGV2ScreenFieldHost` (`GetScreenFieldId`), связывая валидированные данные схемы со своим локальным состоянием через универсальные property consumers.
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
UGV2TextWidgetBase            implements IGV2UiStyleConsumer, IGV2UiPropertyHost
UGV2RichTextWidgetBase        implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2RichTextPopoverWidgetBase  implements IGV2UiStyleConsumer
UGV2ImageWidgetBase           implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2ButtonWidgetBase          implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2UiBindingTarget
UGV2CheckboxWidgetBase        implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost, IGV2UiBindingTarget
UGV2InputFieldWidgetBase      implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost, IGV2UiBindingTarget
UGV2DropdownSelectWidgetBase  implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost, IGV2UiBindingTarget
UGV2ButtonListWidgetBase      implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2ProgressBarWidgetBase     implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2PortraitWidgetBase        implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2ModalWidgetBase           implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2TabContainerWidgetBase    implements IGV2UiStyleConsumer, IGV2UiPropertyHost, IGV2ScreenFieldHost
UGV2SeparatorWidgetBase       implements IGV2UiStyleConsumer
UGV2LoadingIndicatorWidgetBase implements IGV2UiStyleConsumer
```

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
- **`Text`**: разрешённая модель локализованного текста `FGV2TextViewModel` (`FText` + `StyleToken` + `NormalizedMarkup`).
- **`Ref`**: Stable ID ресурса или сущности (`FString`).
- **`Binding`**: непрозрачный описатель привязки команды `FGV2UiBindingHandle`.
- **`Object`**: именованный набор свойств `FGV2PreparedUiObject` (`TMap<FName, FGV2PreparedUiValue>`). Используется как контейнер верхнего уровня экрана; прямое потребление свойствами виджетов запрещено (неприменимый вид в фабрике потребителей; композиты используют плоские маппинги свойств либо `CollectionHost`/`NestedScreen`).
- **`Array`**: упорядоченный массив значений `TArray<FGV2PreparedUiValue>`.

### Семейство стандартных потребителей (`FGV2PropertyConsumerFactory`)

Каждая объявленная в виджете capability привязывается к соответствующему потребителю:
- `FGV2ScalarPropertyConsumer` — обновляет скалярные свойства (например, `is_checked`, `value`, `percent`).
- `FGV2KeyPropertyConsumer` — обновляет идентификаторы (`key`, `selected_key`).
- `FGV2TextPropertyConsumer` — обновляет отображаемый текст через `UGV2TextPipeline` (`text`, `label`, `placeholder`).
- `FGV2RefPropertyConsumer` — разрешает и применяет визуальные ресурсы (`resource_id`, `frame_resource_id`) через `UGV2ImageResourceCatalog`.
- `FGV2BindingPropertyConsumer` — передаёт `FGV2UiBindingHandle` виджетам, реализующим `IGV2UiBindingTarget`.
- `FGV2KeyedCollectionPropertyConsumer` — управляет жизненным циклом (создание, переиспользование, удаление) дочерних элементов списков и коллекций (`items`, `buttons`, `options`, `meters`, `effects`, `characters`).
- `FGV2RichTextSpansPropertyConsumer` — готовит и связывает интерактивные текстовые фрагменты со своими дескрипторами и биндингами.

### Двухфазный конвейер мутаций

Мутация любого `IGV2UiPropertyHost` разделена на две строгие фазы:
1. `PrepareUiHostProperties(Value, OutPlan, OutError)`: выполняет полную валидацию, разрешение ресурсов, текста и дочерних коллекций **off-tree**, формируя иммутабельный план `FGV2UiHostMutationPlan`. Ни один физический виджет UMG на этой фазе не модифицируется.
2. `CommitUiHostProperties(Plan)`: атомарно применяет подготовленный план ко всем потребителям свойств.

## Implemented vertical slice API

| Native class | Role & Capabilities | Screen Field | Required `BindWidget` |
|---|---|---|---|
| `UGV2ScreenWidgetBase` | Screen orchestrator (`PrepareScreenFields`, `CommitScreenFields`, `CanApplyScreenFields`, `ApplyScreenFields`) | Aggregate contract | Dynamic hosts находятся через Widget tree по `IGV2ScreenFieldHost` |
| `UGV2TextWidgetBase` | Property host: `text` (Text) | Leaf presentation | `TextBlock: UCommonTextBlock` |
| `UGV2RichTextWidgetBase` | Property host: `text` (Text), `spans` (RichTextSpans) | `core:schema.ui_field.rich_text.v3` при configured `ScreenFieldId` | `RichTextScrollBox: UScrollBox`, `RichTextBlock: UCommonRichTextBlock` |
| `UGV2RichTextPopoverWidgetBase` | Presentation popover: `InitializePopover(FGV2RichTextHoverViewModel)` | Transient tooltip projection | `PopoverBorder: UBorder`, `PopoverWidth: USizeBox`, `TitleText: UCommonTextBlock`, `DescriptionText: WBP_RichText`; optional `Icon` |
| `UGV2ImageWidgetBase` | Property host: `resource_id` (Ref), `key` (Key); `ApplyImageResource` | `core:schema.ui_field.image.v1` при configured `ScreenFieldId` | `Image: UImage` |
| `UGV2ButtonWidgetBase` | Property host: `text` (Text), `binding` (Binding), `key` (Key); implements `IGV2UiBindingTarget` | Leaf interaction element | `LabelText: UCommonTextBlock` |
| `UGV2CheckboxWidgetBase` | Property host: `key` (Key), `text` (Text), `is_checked` (Scalar), `binding` (Binding); `SubmitCheckboxState(bool)` | `core:schema.ui_field.checkbox.v1` при configured `ScreenFieldId` | `Checkbox: UCheckBox`, `LabelText: UCommonTextBlock` |
| `UGV2InputFieldWidgetBase` | Property host: `key` (Key), `label` (Text), `placeholder` (Text), `value` (Scalar), `binding` (Binding); `SubmitTextValue(FString)` | `core:schema.ui_field.input_field.v1` при configured `ScreenFieldId` | `EditableTextBox: UEditableTextBox`; optional `LabelText: UCommonTextBlock` |
| `UGV2DropdownSelectWidgetBase` | Property host: `placeholder` (Text), `selected_key` (Key), `options` (CollectionHost), `binding` (Binding); `SubmitSelection(FName)` | `core:schema.ui_field.dropdown_select.v1` при configured `ScreenFieldId` | `HeaderButton: UGV2ButtonWidgetBase`, `PopupBorder: UBorder`, `PopupSizeBox: USizeBox`, `OptionsScrollBox: UScrollBox` |
| `UGV2ButtonListWidgetBase` | Property host: `items` (CollectionHost для кнопок) | `core:schema.ui_field.button_list.v2` при configured `ScreenFieldId` | `ButtonContainer: UVerticalBox` |
| `UGV2ProgressBarWidgetBase` | Property host: `percent` (Scalar), `label` (Text), `key` (Key) | `core:schema.ui_field.progress_bar.v1` при configured `ScreenFieldId` | `ProgressBar: UProgressBar` |
| `UGV2PortraitWidgetBase` | Property host: `resource_id` (Ref), `frame_resource_id` (Ref), `key` (Key) | `core:schema.ui_field.portrait.v1` при configured `ScreenFieldId` | `PortraitImage: UImage`, `FrameImage: UImage` |
| `UGV2ModalWidgetBase` | Property host: `title` (Text), `content` (Text), `buttons` (CollectionHost), `backdrop_close_action` (Binding) | `core:schema.ui_field.modal.v1` при configured `ScreenFieldId` | Content layout and slot hosts |
| `UGV2TabContainerWidgetBase` | Property host: `default_tab_key` (Key), `tabs` (CollectionHost) | `core:schema.ui_field.tab_container.v1` при configured `ScreenFieldId` | Tab header container and content slot |
| `UGV2SeparatorWidgetBase` | Только central style | Purely visual | `SeparatorSizeBox: USizeBox`, `SeparatorImage: UImage` |
| `UGV2LoadingIndicatorWidgetBase` | Только central style | UE-local operation state | `LoadingIndicator: UCircularThrobber` |

`UGV2ScreenWidgetBase` централизует discovery, двухфазную подготовку и применение Screen Fields. Он не содержит concrete Screen fields или `screen_id` branches. Unset `ScreenFieldId` исключает nested Widget из aggregate contract.

Repeated-field items обязаны иметь deterministic `key`. Общий `FGV2KeyedCollection` и `FGV2KeyedCollectionPropertyConsumer` владеют create/reuse/reorder/remove lifecycle. Ошибка подготовки candidate collection сохраняет предыдущих children. Runtime class/path из Lua отсутствует.

## Central style contract

`DA_UITheme_Default : UGV2UiTheme` является source of truth default visual values UI-kit. `UGV2UiThemeSettings.ThemeAsset` выбирает active theme через UE-only project config. Lua, headless runtime и Screen Field DTO не получают asset locator или theme UObject.

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

Каждый reusable visual component реализует `IGV2UiStyleConsumer.ApplyCentralStyle()` и вызывает его из `NativePreConstruct`. Это обеспечивает одинаковое поведение editor preview и runtime reconstruction. Отсутствующий theme, required style class или required `BindWidget` возвращает failure; silent local fallback для production component запрещён.

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

Hover title/description и optional `image_resource_id` являются value-only presentation data. Decorator создаёт tooltip лениво при открытии, а закрытие уничтожает transient popover. Hover/unhover не пересекают Lua boundary. Не разрешённый optional image скрывается без подмены raw asset path. Popover является composite: title/description используют approved Text Pipeline adapters, а optional icon делегирует resolution/mutation общему `FGV2ImagePresentation`. Собственный Blueprint resolver или direct brush mutation для runtime `resource_id` запрещены.

Click span получает только `FGV2UiBindingHandle`. `SubmitSpanInteraction(span_id)` использует общий `SubmitUiInteraction(handle, {})`; decorator не хранит `command_id`, bound args или Lua callback. Reapply/reset/destruct RichText удаляет local span lookup, а смена UI revision инвалидирует handles через общий binding registry.

## Current WBP_Testscreen contract

`WBP_Testscreen` является physical UMG integration fixture и не вводит новый reusable `widget_id`. Его abstract parent — `WBP_ScreenBase`; template объявляет:

| Field | Element | Required |
|---|---|---|
| `description` | `WBP_RichText` | Yes |
| `checkbox` | `WBP_Checkbox` | Yes |
| `class_select` | `WBP_DropdownSelect` | Yes |
| `player_name` | `WBP_InputField` | Yes |
| `buttons` | `WBP_ButtonList` | Yes |

Lua command handler публикует Screen request, включая rich text semantic spans, desired checkbox state, text value и dropdown selection. Session Coordinator выполняет подготовку биндингов через `GV2ScreenFieldMaterializer::PrepareBindingDefinitions`, материализует поля через `GV2ScreenFieldMaterializer::BuildFields` в пять `FGV2ScreenFieldValue`, затем атомарно публикует binding records и применяет поля к generic `UGV2ScreenWidgetBase` через раздельные фазы Prepare и Commit. Checkbox submit содержит required boolean `is_checked`, input submit — required string `value`, dropdown submit — required string `selected_key`; каждый содержит только opaque handle и schema control value. После Command Dispatcher Lua обновляет desired state и перепубликует Screen. C++ не вызывает Lua Screen builder и не принимает `TSubclassOf`; concrete class/path отсутствует в runtime source и Lua boundary.

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

Tests покрывают duplicate registration, registry freeze, trusted factory resolution, no raw asset path, element schema validation, required/optional fields, duplicate/unknown field rejection, двухфазный atomic apply (Prepare/Commit), opaque handle propagation и отсутствие gameplay authority. `GV2ScreenFieldMaterializer` тесты фиксируют схематическую материализацию и отсутствие schema-specific C++ адаптеров и DTO; тесты `GV2.UI.StandardPropertyConsumers` и `GV2.UI.CapabilityObservabilityHarness` проверяют потребители свойств и их двухфазный жизненный цикл. UI-kit test загружает active theme, проверяет native parents, mandatory `BindWidget`, `IGV2UiStyleConsumer` и successful style apply. Для `WBP_RichText` он дополнительно проверяет automatic/per-character wrapping и вертикальный `RichTextScrollBox`; для popover — composition через те же text/image leaf adapters и ограниченную theme height; для input/dropdown — submit через общий emitter и Lua-owned desired-state republish. Asset audit обязан перечислять все `WBP_*`, запрещать direct runtime text/content-image primitives в composites, local dynamic collection factories и direct Runtime Subsystem ingress вне общего emitter. Runtime source audit запрещает concrete `screen_id`/`field_id` branches.
