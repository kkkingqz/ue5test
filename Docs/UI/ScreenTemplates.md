---
title: Blueprint Screen Template Contract
status: draft
version: 1.3
updated: 2026-08-20
depends_on:
  - ../Architecture/StableIDSpecification.md
  - WidgetRegistry.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0035-ui-foundation-and-composition.md
---

# Blueprint Screen Template Contract

> **Владеет:** базовым Screen Blueprint, реестром экранов, устройством Screen Fields и правилами их применения.
> **Не владеет:** тем, какой экран показать — это решает Lua; и содержимым полей.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** `Source/GV2/Private/UI/GV2ScreenRegistry.cpp`, `GV2ScreenWidgetBase.cpp`, `Content/UI/`.
> **Проверки:** `GV2.Runtime.Presentation.*`.

## Purpose and scope

Screen Template задаёт UE-authored layout конкретного Screen и schema динамических данных, которые Lua может менять без знания UMG structure. Контракт охватывает base class, Dynamic Screen Elements, Screen Fields и Screen Registry; он не передаёт gameplay ownership в Blueprint.

## Ownership and source of truth

- Lua владеет desired screen instance, значениями полей и доступными Command bindings.
- Concrete Widget Blueprint владеет layout, slots, animation, focus navigation и выбором Dynamic Screen Elements.
- `UGV2ScreenWidgetBase` владеет generic validation/apply lifecycle.
- `FGV2ScreenFieldAdapterRegistry` владеет fixed mapping `schema_id → trusted boundary adapter` и преобразованием portable value в typed `FGV2ScreenFieldValue`.
- Dynamic Screen Element владеет преобразованием одного declared field schema в local Widget state.
- Screen Registry является единственным UE presentation mapping `screen_id → trusted Widget Blueprint class`.

## Invariants

- Concrete Screen Blueprint обязан наследовать `WBP_ScreenBase`; сам `WBP_ScreenBase` обязан оставаться abstract.
- Lua/portable runtime не получает Blueprint class/path и не знает имён Widget в tree.
- C++ generic screen layer не содержит switch/branch по concrete `screen_id`.
- `field_id` unique внутри Screen Template и имеет lowercase `snake_case`.
- Каждый configured element объявляет non-empty Stable ID `schema_id` и required/optional policy.
- Lua публикует полный набор полей текущего screen instance, а не mutation operations.
- Blueprint не интерпретирует `command_id`, не вызывает Lua function и не меняет canonical gameplay-state.
- Добавление нового Dynamic Screen Element schema требует concrete scenario и обновления этого или owning component contract.
- Scrollable Dynamic Screen Element обязан получать конечную viewport geometry от layout concrete Screen Template. Template не может оставлять такой элемент с unbounded desired height: overflow policy принадлежит reusable component, а доступная доля экрана — concrete layout.
- Generic runtime принимает только ordered `field_id + schema_id + value` envelopes и запрещает concrete field names. Schema-specific conversion принадлежит registered field adapter.
- Registry строится до первого использования, не хранит session state и запрещает duplicate `schema_id`. Unknown schema отклоняет весь candidate Screen request.
- Равномерное масштабирование кадра (uniform frame scale) запрещено: раскладка отзывчивая (responsive) и распределяет фактический viewport.
- Текст масштабируется нелинейной кривой темы и никогда не опускается ниже `MinReadableFontSize` (10 pt).

## Responsive Layout and Scaling Model (ADR-0035)

Верстка экранов строится на принципах отзывчивого адаптивного дизайна:

### Dual Resolution Basis

1. **Разрешение авторинга растра (4K / 3840 × 2160)**: все растровые фоны, 9-slice плашки и иконки создаются с высоким разрешением для чистого даунскейлинга (`FGV2LayoutConstants::RasterAuthoringWidth/Height`).
2. **Единицы виртуальной раскладки (1080p / 1920 × 1080)**: размеры отступов, слотов и сеток проектируются в базисе 1920×1080 (`FGV2LayoutConstants::VirtualLayoutWidth/Height`).

### Разделение Layout Policy и Content Scaling Policy

Контракт строго разделяет внешнее распределение пространства слота и внутреннее поведение визуального примитива:

- **Layout Policy (внешнее)**: правила контейнеров UMG (anchors, margins, safe zone offsets, Auto/Fill, Min/Max dimensions, Grid/Box slots). Контейнер распределяет доступный прямоугольник viewport.
- **Content Scaling Policy (внутреннее)**: режим заполнения слота примитивом (`EGV2PrimitiveScalePolicy`: `FreeStretch`, `Tile`, `NineSlice`, `PreserveAspect`). Примитив обязан быть совместим с режимом ресурса.

### Нелинейная кривая масштаба текста (Non-linear Text Scaling)

В отличие от растра, текст не масштабируется линейно пропорционально высоте экрана, чтобы избежать нечитаемости на малых экранах и чрезмерно гигантского шрифта на 4K:

- Применяется `UGV2UiTheme::EvaluateTextScale(ViewportHeight)` на основе настраиваемой кривой `TextScaleCurve`.
- На 720p масштаб составляет ~0.85 (вместо линейного 0.66), гарантируя читаемость.
- На 1080p масштаб составляет 1.0 (базовый).
- На 1440p масштаб составляет ~1.25.
- На 4K (2160p) масштаб составляет ~1.60 (вместо линейного 2.0).
- Итоговый физический размер шрифта ограничен снизу порогом `MinReadableFontSize` (10 pt) через `UGV2UiTheme::GetEffectiveFontSize`.

### Матрица целевых разрешений

Шаблоны экранов тестируются и сохраняют целостность на 6 стандартных разрешениях:
1. `3840 × 2160` (4K 16:9)
2. `2560 × 1440` (QHD 16:9)
3. `1920 × 1080` (FHD 16:9 — reference)
4. `1280 × 720` (HD 16:9 — минимальная цель, обязательные контролы не обрезаются)
5. `3440 × 1440` (UWQHD 21:9 — ultrawide safe area)
6. `2560 × 1080` (UWFHD 21:9 — ultrawide safe area)

## Screen Registry

Нормативная registry entry имеет логический вид:

```json5
{
  screen_id: "core:screen.main",
  layer: "location_content",
  is_singleton: true,
  // UE-only trusted soft class reference; Lua это поле не получает.
  widget_class: "WBP_Screen_Main",
}
```

`widget_class` показан как editor-facing label, а не boundary value и не raw `/Game/...` locator. Registry строится до session registry freeze. Duplicate `screen_id`, class не-наследник `WBP_ScreenBase` или отсутствующий cooked class являются startup validation error.

Текущая реализация использует `UGV2ScreenRegistry : UDataAsset`. Единственный bootstrap locator задаётся UE-only настройкой `UGV2ScreenRegistrySettings.RegistryAsset` в `DefaultGame.ini`; Lua его не получает. При `UGV2RuntimeSubsystem.Initialize` asset загружается один раз, все entries валидируются, soft classes разрешаются и копируются в private immutable lookup текущего subsystem lifetime. Session не переходит в test `Ready`, если registry не готов.

Текущий asset `DA_ScreenRegistry` содержит entry:

```json5
{
  screen_id: "core:screen.test",
  widget_class: "WBP_Testscreen",
  layer: "location_content",
  is_singleton: true,
}
```

Добавление concrete screen меняет только Widget Blueprint и `DA_ScreenRegistry`. Окончательная стабилизация public C++ façade отложена, но runtime resolution уже не содержит concrete class/path.

## Class hierarchy

```text
UCommonUserWidget
└── UGV2ScreenWidgetBase
    └── WBP_ScreenBase (abstract)
        └── WBP_Testscreen
```

Game Shell имеет отдельную обязательную иерархию:

```text
UCommonActivatableWidget
└── UGV2GameShellWidgetBase
    └── WBP_GameShell
```

`WBP_GameShell` владеет только layout слоёв и authored host-контейнерами; он не является Screen Template и не регистрируется по `screen_id`.

Concrete screens не обязаны иметь собственный native subclass. Общие lifecycle hooks и field apply находятся в `UGV2ScreenWidgetBase`; визуально специфичное поведение остаётся Blueprint-local и не меняет field semantics.

## Dynamic Screen Element contract

Каждый поддерживаемый элемент реализует `IGV2DynamicScreenElement`:

```text
GetScreenFieldDescriptor() -> { field_id, schema_id, is_required }
CanApplyScreenField(value) -> bool
CaptureScreenField() -> value
ApplyScreenField(value) -> bool
ResetScreenField() -> bool
```

Unset `field_id` означает, что Widget используется как обычный nested presentation element и не участвует в screen contract.

Value-only Screen Field имеет форму:

```json5
{
  field_id: "description",
  schema_id: "core:schema.ui_field.rich_text.v3",
  value: {
    text: {
      text_id: "core:text.screen.inventory.description",
      args: {},
    },
    spans: [],
  },
}
```

UE apply использует prepared typed `FGV2ScreenFieldValue`; portable boundary передаёт generic envelope, а schema adapter преобразует его до этого типа. Поддерживаются:

| `schema_id` | Element adapter | Значение |
|---|---|---|
| `core:schema.ui_field.rich_text.v3` | `WBP_RichText` / `UGV2RichTextWidgetBase` | resolved `FGV2TextViewModel` + semantic spans с UE-local hover payload и optional opaque click binding |
| `core:schema.ui_field.button_list.v2` | `WBP_ButtonList` / `UGV2ButtonListWidgetBase` | ordered items с resolved `FGV2TextViewModel` и opaque binding handle |
| `core:schema.ui_field.checkbox.v1` | `WBP_Checkbox` / `UGV2CheckboxWidgetBase` | resolved `FGV2TextViewModel`, desired `is_checked: boolean` и opaque binding handle |
| `core:schema.ui_field.input_field.v1` | `WBP_InputField` / `UGV2InputFieldWidgetBase` | resolved label/placeholder, desired `value: string` и opaque binding handle |
| `core:schema.ui_field.dropdown_select.v1` | `WBP_DropdownSelect` / `UGV2DropdownSelectWidgetBase` | resolved placeholder/options, optional selected key и opaque binding handle |
| `core:schema.ui_field.image.v1` | `WBP_Image` / `UGV2ImageWidgetBase` | resolved `FGV2ImageFieldViewModel` с валидируемым Stable ID ресурса |
| `core:schema.ui_field.progress_bar.v1` | `WBP_ProgressBar` / `UGV2ProgressBarWidgetBase` | resolved `FGV2ProgressBarViewModel` с `percent: float` (0.0..1.0) и опциональным label |
| `core:schema.ui_field.portrait.v1` | `WBP_Portrait` / `UGV2PortraitWidgetBase` | resolved `FGV2PortraitViewModel` с `fixed_aspect` portrait и опциональной рамкой |
| `core:schema.ui_field.modal.v1` | `WBP_Modal` / `UGV2ModalWidgetBase` | resolved `FGV2ModalViewModel` с `title`, `content`, кнопками и backdrop close binding |
| `core:schema.ui_field.tab_container.v1` | `WBP_TabContainer` / `UGV2TabContainerWidgetBase` | resolved `FGV2TabContainerViewModel` с `default_tab_key`, упорядоченным списком вкладок `{key, title: TextSpec, screen_id, fields}` |

Каждый registry adapter выполняет две deterministic фазы. `PrepareBindings` валидирует schema-specific value и добавляет binding definitions в порядке обхода поля. После единой подготовки candidate binding set `BuildField` потребляет ровно соответствующие opaque handles и создаёт typed field value. Registry не публикует bindings и не меняет active Screen; атомарная публикация остаётся ответственностью Session Coordinator.

Production Lua document обязан использовать `TextSpec`; localization adapter создаёт `FGV2TextViewModel` до apply. Button model содержит только resolved display text, semantic style token и opaque binding handle, а не Lua callback.

### Designer Authoring Layer (ADR-0027)

Синтаксис `text`, `action`, `button`, `tab`, `tabs` и `show_*`, его package attribution и runtime adaptation задаёт [Authoring Surface Contract](../Architecture/AuthoringSurfaceContract.md). Этот contract владеет результатом: `TextSpec`, stable element keys, Screen Field schema, layer/instance identity и atomic apply. Authoring helper обязан создавать структуры этой модели и не может вводить callback, raw user-facing string или параллельный Screen format.

### Источник презентации и автоматическая инвалидация (SAS-14..16, ADR-0028)

Геймплейные команды не управляют интерфейсом и не вызывают перестроение экранов вручную. Вместо этого:

1. **Регистрация источника**: пакет регистрирует функцию-источник презентации через `game.presentation.register_source(fn)` на фазе `register`. Повторная регистрация (`PresentationSourceDuplicateRegistration`), невалидный тип (`InvalidPresentationSource`) и регистрация после freeze (`PresentationSourceRegistryFrozen`) отклоняются.
2. **Автоматическая инвалидация**: рантайм вызывает `game.presentation.resolve()` после каждой **успешно закоммиченной** команды вне окна мутации (`mutation_window`). При отказе или runtime fault источник не вызывается. Попытка мутации состояния из источника презентации блокируется ошибкой `MutationWindowClosed`.
3. **Шов под UI document**: источник презентации разрешает активный экран из текущего состояния и является архитектурным швом, который в будущем будет заменён маршрутизатором UI document без изменения геймплейного кода.

## Apply lifecycle

`ApplyScreenFields` выполняется атомарно на логическом presentation level:

1. Registry adapters валидируют portable fields и готовят ordered binding definitions.
2. Session Coordinator создаёт единый candidate binding set; registry adapters строят typed fields, потребляя подготовленные handles.
3. Обойти Widget tree и собрать configured Dynamic Screen Elements.
4. Отклонить invalid/duplicate `field_id`, unknown payload field, missing required field и `schema_id` mismatch.
5. Вызвать `CanApplyScreenField` для каждого candidate и захватить старые значения.
6. Применить все present fields; absent optional field сбросить через `ResetScreenField`.
7. При commit failure восстановить уже изменённые элементы и не публиковать screen interactive.
8. Только после полного success вызвать `OnScreenFieldsApplied` и commit binding revision.

`GetScreenFieldContract` возвращает descriptors в deterministic order по `field_id` и используется validation/tests, но не заменяет build-time schema declaration.

## Current vertical slice

`WBP_Testscreen` наследует `WBP_ScreenBase` и объявляет ровно пять required полей:

| `field_id` | Existing element | Schema |
|---|---|---|
| `description` | `DescriptionText: WBP_RichText` | `core:schema.ui_field.rich_text.v3` |
| `checkbox` | `CheckboxField: WBP_Checkbox` | `core:schema.ui_field.checkbox.v1` |
| `class_select` | `ClassSelectField: WBP_DropdownSelect` | `core:schema.ui_field.dropdown_select.v1` |
| `player_name` | `PlayerNameField: WBP_InputField` | `core:schema.ui_field.input_field.v1` |
| `buttons` | `ButtonList: WBP_ButtonList` | `core:schema.ui_field.button_list.v2` |

`DescriptionText` находится в `DescriptionSurface`, чей `VerticalBoxSlot` использует `Fill`; `PlayerNameField`, `ClassSelectField`, `CheckboxField` и `ButtonList` используют `Automatic`. Поэтому controls занимают требуемую высоту, описание получает оставшуюся высоту экрана, а overflow обрабатывается внутренним `RichTextScrollBox` компонента.

Lua command handler публикует Screen request с `screen_id = "core:screen.test"` и generic fields. Schema adapters готовят RichText, ButtonList, Checkbox, InputField и DropdownSelect values и candidate bindings. Checkbox binding объявляет required `is_checked: boolean`, input binding — required `value: string`, dropdown binding — required `selected_key: string`. Runtime разрешает class только через `DA_ScreenRegistry`; C++ не предоставляет screen builder/factory с параметрами, не имеет test-specific apply API и не знает concrete field names.

## Failure and recovery

- Invalid field contract запрещает interactive apply и создаёт structured diagnostic с screen/field/schema context.
- Failed candidate не изменяет current published screen/bindings.
- Rollback failure является presentation fault; input остаётся закрытым до полного rebuild из последнего desired document.
- Unknown `screen_id` или invalid registry class открывает system error surface; Lua gameplay-state не меняется.

## Compatibility and evolution

- Добавление optional field совместимо, если старый template корректно работает после reset/default.
- Удаление required field, смена смысла `field_id` или несовместимая смена schema являются breaking change.
- Опубликованный `screen_id` или field schema Stable ID не переиспользуется для другого смысла.
- Layout/style/animation могут меняться без schema version, если observable field/input contract сохраняется.
- Новый field schema добавляет один adapter и registry entry, DTO/contract fixtures и tests; Session Coordinator изменять запрещено.

## Verification

- `WBP_GameShell` имеет native parent `UGV2GameShellWidgetBase`; все шесть layer hosts существуют в его отображаемом Widget tree. Отсутствующий host не заменяется runtime fallback-контейнером.
- `WBP_ScreenBase` загружается как abstract Blueprint class и имеет native parent `UGV2ScreenWidgetBase`.
- `WBP_Testscreen` является его child class и компилируется без test-specific native parent.
- `DA_ScreenRegistry` загружается через config, содержит `core:screen.test` и разрешает concrete non-abstract child `WBP_ScreenBase`.
- Contract `WBP_Testscreen` содержит deterministic `buttons`, `checkbox`, `class_select`, `description`, `player_name`; все поля required и имеют ожидаемые schemas.
- `DescriptionSurface` ограничивает `WBP_RichText` оставшейся высотой экрана; длинный текст переносится и прокручивается внутри блока.
- Unknown, duplicate, missing required и schema mismatch payloads отклоняются до mutation.
- Button click пересекает boundary только как opaque handle и проходит Semantic Input/Command Dispatcher.
- Checkbox change пересекает boundary как opaque handle + `is_checked`, после чего Lua публикует новое desired state.
- Input commit пересекает boundary как opaque handle + `value`, после чего Lua публикует новое desired state.
- Dropdown option activation пересекает boundary ровно один раз как opaque handle + `selected_key`, после чего Lua публикует новое desired state.
- Добавление нового Screen Blueprint и registry entry не требует изменения C++.
- Runtime source не содержит `/Game/UI/Widgets/WBP_Testscreen` и не принимает Blueprint class из Lua/Blueprint façade.
- Session Coordinator не содержит concrete Screen Field schema IDs; fixed adapter registry содержит десять опубликованных schemas из таблицы этого contract и отклоняет unknown/duplicate registration.
- Automation проходит через обычные Session, Semantic Input и Screen request entry points; test-only runtime methods отсутствуют.
