---
title: Widget Registry Contract
status: draft
version: 0.5
updated: 2026-08-10
depends_on:
  - ../Architecture/StableIDSpecification.md
  - UIDocumentAndReconciliation.md
---

# Widget Registry Contract

Widget Registry сопоставляет logical `widget_id` с разрешённой C++/UMG factory и property schema. Lua никогда не передаёт Widget Blueprint class/path.

## Registry entry

```json5
{
  widget_id: "core:widget.button",
  property_schema_id: "core:schema.widget.button_props.v1",
  allowed_child_mode: "none",
  factory_resource_id: "core:resource.widget.button.default",
  source_package_id: "core",
}
```

Entry регистрируется до session registry freeze. Duplicate ID с несовместимым factory/schema — fatal.

## Responsibilities

- Validate widget ID и props schema.
- Create/reuse/destroy physical Widget.
- Declare child/slot policy.
- Bind local visual state and semantic input adapter.
- Resolve style/factory resource через Presentation Resource Resolver.
- Provide typed error widget for invalid document in development/recovery.

Registry не хранит gameplay-state и не выбирает command availability.

## Native adapters and Blueprint bases

Widget Registry создаёт physical Widgets через trusted C++ factory/adapter mapping. Mapping может использовать cooked soft class reference, разрешённый из `factory_resource_id`; Lua и UI-document не содержат `/Game/...` path или Blueprint class name.

Native base classes предоставляют Blueprint только typed presentation data и local event surface:

```text
UGV2RichTextWidgetBase
UGV2ButtonWidgetBase
UGV2ButtonListWidgetBase
UGV2TestScreenWidgetBase
```

`WBP_RichText`, `WBP_Button`, `WBP_ButtonList` и `WBP_Testscreen` наследуют соответствующие native bases. Blueprint отвечает за layout/style/animation; C++ adapter применяет props, управляет reuse и передаёт Semantic Input Adapter-у opaque binding handle.

Production button view model:

```text
FGV2ButtonViewModel
  Text: FText
  Binding: FGV2UiBindingHandle
```

`FText` создаётся Presentation из `TextSpec`, а `FGV2UiBindingHandle` создаётся reconciler-ом из validated document command binding. Ни `command_id`, ни Lua callback/function name не являются editable Blueprint data.

### Implemented vertical slice API

| Native class | Public apply API | Required `BindWidget` |
|---|---|---|
| `UGV2RichTextWidgetBase` | `ApplyRichTextContent(FText)` | `RichTextBlock: UCommonRichTextBlock` |
| `UGV2ButtonWidgetBase` | `ApplyButtonModel(FGV2ButtonViewModel)` | `LabelText: UCommonTextBlock` |
| `UGV2ButtonListWidgetBase` | `ApplyButtonModels(FGV2ButtonViewModel[])` | `ButtonContainer: UVerticalBox` |
| `UGV2TestScreenWidgetBase` | `ApplyScreenModel(FGV2TestScreenViewModel)` | `DescriptionText`, `ButtonList` |

`UGV2ButtonWidgetBase` отправляет binding в `UGV2RuntimeSubsystem` из `NativeOnClicked`; resolved result поднимается через typed `OnBindingInvoked`. List и test screen могут re-broadcast этот typed result для diagnostics, но не получают resolved command/test action.

`ApplyButtonModels` сначала валидирует input и создаёт candidate Widgets, затем полностью заменяет children. Ошибка до commit сохраняет старый список. Concrete ButtonList Blueprint обязан настроить trusted `ButtonWidgetClass`; runtime path из Lua отсутствует.

## Mod support

- Data-only mod использует existing widget IDs и named slots.
- New widget ID requires compatible cooked Pak/Mod Kit resource mounted before repository build.
- Mod creates widget ID only in own namespace.
- Override existing widget mapping is disabled in v1; visual customization uses theme/resource definitions and slots.

## Unknown widget

Unknown `widget_id` makes target UI-document invalid before interactive apply. Shipping uses system error surface; development additionally reports package, node key, element ID and source definition provenance. Partial tree substitution is not automatic.

## Initial registry

Minimal vertical slice may include:

```text
core:widget.container
core:widget.button
core:widget.button_list
core:widget.text
core:widget.rich_text
core:widget.image
core:widget.portrait
core:widget.modal
```

Новый widget type добавляется только при невозможности выразить scenario существующим component/template, а не для каждого отдельного screen.

## Experimental test screen

`WBP_Testscreen` является physical UMG fixture для проверки composition и input propagation. Он не создаёт новый `widget_id`: screen собирается из существующих `WBP_RichText` и `WBP_ButtonList`, а каждый пункт списка использует `WBP_Button`.

Логическая входная модель:

```json5
{
  description_text: "Test description",
  buttons: [
    {
      text: "First option",
      action: "core:command.test.first_option",
    },
  ],
}
```

`action` в fixture является opaque test value, а не именем Blueprint/Lua function и не callback. Он разрешён только для изолированного composition test. Production adapter обязан заменить его на `FGV2UiBindingHandle`; Blueprint не должен интерпретировать или передавать raw `command_id` как authoritative input.

Legacy child-widget API временно сохраняется для isolated composition fixtures и flatten-ит `buttons` в два ordered массива:

| Asset | API | Contract |
|---|---|---|
| `WBP_RichText` | `SetContent(Content: Text)` | Полностью заменяет отображаемый rich text |
| `WBP_Button` | `SetButtonData(label: Text, action: String)` | Устанавливает label и opaque action; click публикует `action_selected(action)` |
| `WBP_ButtonList` | `SetButtons(button_texts: Text[], button_actions: String[])` | Индекс задаёт пару `text/action`; массивы обязаны иметь одинаковую длину |

`WBP_Testscreen.SetScreenData` и его raw action wiring удалены после подключения `UGV2TestScreenWidgetBase`. Нормативный screen API — `ApplyScreenModel(FGV2TestScreenViewModel)` и typed `OnBindingInvoked`.

Несовпадающие `button_texts` и `button_actions` в legacy ButtonList отклоняются до очистки существующего списка и создают diagnostic. Эта форма не используется native Testscreen path. Lua UI-document обязан использовать `buttons[]` из objects с `TextSpec` и document command binding; production C++/Blueprint boundary использует `FGV2ButtonViewModel[]` с localized text и opaque binding handle, а не parallel arrays.

## Tests

Tests cover duplicate registration, freeze, props validation, stable-key reuse, child policy, trusted factory resolution, no raw asset path, data-only mod usage, Pak requirement, unknown widget atomic failure, typed view-model apply, opaque handle propagation и absence of gameplay authority. Для vertical slice проверяются native parent classes, обязательные `BindWidget`, configured `ButtonWidgetClass`, compile четырёх `WBP_*`, full list rebuild и `GV2.Runtime.TestBackend.ButtonRoundTrip` без raw action в Testscreen API.
