---
title: Blueprint Screen Template Contract
status: normative
version: 1.9
updated: 2026-09-01
depends_on:
  - ../Architecture/StableIDSpecification.md
  - WidgetRegistry.md
decisions:
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0035-ui-foundation-and-composition.md
  - ../ADR/0040-universal-ui-property-pipeline.md
---

# Blueprint Screen Template Contract

> **Владеет:** базовым Screen Blueprint, реестром экранов, устройством Screen Fields и правилами их применения.
> **Не владеет:** тем, какой экран показать — это решает Lua; и содержимым полей.
> **Инварианты:** [INV-014](../Architecture/Invariants.md)
> **Реализация:** `Source/GV2/Private/UI/GV2ScreenRegistry.cpp`, `GV2ScreenWidgetBase.cpp`, `GV2ScreenFieldMaterializer.cpp`, `Content/UI/`.
> **Проверки:** `GV2.Runtime.Presentation.*`, `GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure`.

## Purpose and scope

Screen Template задаёт UE-authored layout конкретного Screen и schema динамических данных, которые Lua может менять без знания UMG structure. Контракт охватывает base class, Screen Field Hosts, Screen Fields и Screen Registry; он не передаёт gameplay ownership в Blueprint.

## Ownership and source of truth

- Lua владеет desired screen instance, значениями полей и доступными Command bindings.
- Concrete Widget Blueprint владеет layout, slots, animation, focus navigation и размещением Screen Field Hosts.
- `UGV2ScreenWidgetBase` владеет двухфазным generic validation/apply lifecycle (Prepare/Commit) и preflight-проверкой полей экрана.
- Репозиторий контента (`GV2ContentCore`) владеет декларативными UI-схемами (`schema_domain: "ui_field"` / `"ui_value"`), их компиляцией, валидацией замкнутости полей и разрешением `schema_ref`.
- Screen Field Host (`IGV2ScreenFieldHost`) идентифицирует виджет в дереве экрана как приёмник конкретного `field_id`.
- Property Host (`IGV2UiPropertyHost`) объявляет capabilities виджета и применяет подготовленные свойства через универсальные property consumers.
- Screen Registry является единственным UE presentation mapping `screen_id → trusted Widget Blueprint class`.

### LocationScreen: template и values definition

`textsystem:screen.location` — единственный `screen_id` шаблона LocationScreen в Screen Registry. Он разрешается в `WBP_LocationScreen` слоя `location_content` и не содержит gameplay content.

`<game>:screen.location.<path>` — Definition, выбранное из `location.screen_ids`; оно поставляет набор значений для того же шаблона. Такой ID **запрещено** передавать как `screen_id`, добавлять в Screen Registry или разрешать как Widget Blueprint class. Один ID не может одновременно быть template и values definition.

Маршрут LocationScreen обязан публиковать `screen_id: "textsystem:screen.location"` и `instance_key: "location"`. Переход между локациями меняет только fields; неизменная пара сохраняет physical widget и UE-local state.

## Invariants

- Concrete Screen Blueprint обязан наследовать `WBP_ScreenBase`; сам `WBP_ScreenBase` обязан оставаться abstract.
- Lua/portable runtime не получает Blueprint class/path и не знает имён Widget в tree.
- C++ generic screen layer не содержит switch/branch по concrete `screen_id`.
- `field_id` unique внутри Screen Template и имеет lowercase `snake_case`.
- Каждый configured element объявляет non-empty Stable ID `schema_id` и required/optional policy.
- UI-схемы объявляются данными (`GameData/<package>/schemas/`) с доменом `schema_domain: "ui_field"` или `schema_domain: "ui_value"`.
- UI-схемы собираются исключительно из стандартных kinds: скалярных (`bool`, `integer`, `number`, `string`), семантических (`key`, `text`, `ref`, `binding`) и структурных (`object`, `array`, `screen_fields`, `schema_ref`). Попытка ввести нестандартный примитивный kind отклоняется.
- Схемы всех Screen Fields и всех их вложенных объектов являются **замкнутыми (closed schemas)**. Любой не объявленный в схеме ключ на любом уровне вложенности (значение поля, элемент коллекции, `TextSpec`, `Binding`) является невалидным и приводит к типизированному отказу построения и применения поля.
- **Владение namespace**: пакет объявляет схемы только своего namespace (`core:`, `textsystem:`, `rh:`, `<mod>:`). Попытка объявить чужой namespace отклоняется на стадии сборки репозитория.
- **Политика отказа для мода**: несовместимая или некорректная UI-схема мода отбраковывает мод, а не приводит к сбою сессии.
- Lua публикует полный набор полей текущего screen instance, а не mutation operations.
- Blueprint не интерпретирует `command_id`, не вызывает Lua function и не меняет canonical gameplay-state.
- Добавление нового Screen Field не требует C++-адаптера и осуществляется декларативной схемой в данных; схемы компилируются и материализуются переносимо (`GV2ContentCore`), создание C++ класса-адаптера запрещено.
- Scrollable screen element обязан получать конечную viewport geometry от layout concrete Screen Template. Template не может оставлять такой элемент с unbounded desired height: overflow policy принадлежит reusable component, а доступная доля экрана — concrete layout.
- Generic runtime принимает только ordered `field_id + schema_id + value` envelopes и запрещает concrete field names. Валидация выполняется переносимым универсальным валидатором на основе скомпилированной UI-схемы.
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

## Screen Field Host and Property Host contract

Каждый виджет экрана, являющийся приёмником поля верхнего уровня, реализует `IGV2ScreenFieldHost` и `IGV2UiPropertyHost`:

```text
IGV2ScreenFieldHost:
  GetScreenFieldId() -> FName

IGV2UiPropertyHost:
  DescribeUiCapabilities(FGV2UiCapabilityBuilder& Builder)
  PrepareUiHostProperties(Value, OutPlan, OutError) -> bool
  CommitUiHostProperties(Plan) -> bool
```

`GetScreenFieldId()` возвращает имя поля экрана (`field_id`), настроенное для данного виджета (например, `description`, `buttons`, `top_bar`, `scene`). Возврат `NAME_None` означает, что виджет не сконфигурирован как приёмник поля экрана и исключается из экранного контракта.

### Host Identity (DUC-01)

`field_id` — не собственное свойство `IGV2ScreenFieldHost`. Это одно конкретное значение общего `HostIdentity`, которое несёт **любой** `IGV2UiPropertyHost` через `FGV2UiPropertyHostState`:

```text
FGV2UiPropertyHostState:
  UPROPERTY(EditAnywhere) FName HostIdentity;
  GetHostIdentity() -> FName
  SetHostIdentity(FName)
```

Экран и блок (DUC-05+) различаются только тем, что экран сверяется с матрицей разрешений, а блок — нет; смысл самого значения одинаков — **идентичность внутри объемлющего хоста**:

- На уровне экрана это `field_id`: `IGV2ScreenFieldHost::GetScreenFieldId()` делегирует в `GetHostIdentity()`, не хранит собственное отдельное значение. Изначально — только у четырёх Location-композитов; DUC-02 дал ту же адресуемость восьми базовым элементам (`UGV2TextWidgetBase`, `UGV2RichTextWidgetBase`, `UGV2ImageWidgetBase`, `UGV2ButtonWidgetBase`, `UGV2CheckboxWidgetBase`, `UGV2InputFieldWidgetBase`, `UGV2ProgressBarWidgetBase`, `UGV2PortraitWidgetBase`) без единого нового C++-класса — см. [Widget Registry](WidgetRegistry.md#native-adapters-and-blueprint-bases).
- На уровне composite-свойства (DUC-05+) это имя свойства, под которым родительский композит адресует данного ребёнка в своём плоском списке capability.

Два разных свойства идентичности дали бы автору ассета два способа выразить одно и то же с неочевидным приоритетом — поэтому оно ровно одно, и его Designer-поверхность (`meta = (ShowOnlyInnerProperties)` на `UPROPERTY() FGV2UiPropertyHostState PropertyHostState;` каждого хоста) идентична независимо от уровня, на котором виджет размещён.

Дубликат идентичности среди `IGV2ScreenFieldHost` одного экрана отклоняется до `Ready`: `UGV2ScreenWidgetBase`'s discovery (`CollectScreenFieldHosts`) поддерживает `SeenFieldIds` и возвращает ошибку `duplicate screen field host '<value>'` при повторе — это регрессионно проверено `GV2.Runtime.Presentation.HostIdentityIsSharedNotPerClass` после переноса значения на общую поверхность.

`GV2.UI.CapabilityObservabilityCompositeSweep` обязан проверять эту поверхность для каждого production property-host boundary: два разных `HostIdentity` round-trip через общее состояние, а у Screen Field host `GetScreenFieldId()` возвращает установленное общее значение. Новый direct native host без real `WBP_*` fixture делает sweep красным; полное правило source set и исключения тестовых подделок принадлежат [Widget Registry](WidgetRegistry.md#охват-capability-sweep-duc-04).

`UGV2ScreenWidgetBase` управляет двухфазным жизненным циклом применения полей:

```text
PrepareScreenFields(ScreenFields, OutPlan, OutError) -> bool
CommitScreenFields(Plan) -> bool
CanApplyScreenFields(ScreenFields) -> bool (preflight: Prepare и отбрасывание плана)
ApplyScreenFields(ScreenFields) -> bool (one-shot Prepare + Commit)
```

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

Материализатор `GV2ScreenFieldMaterializer` выполняет универсальное schema-driven преобразование:
1. `PrepareBindingDefinitions` выполняет детерминированный обход скомпилированной UI-схемы поля (`GV2ContentCore::FCompiledUiFieldSpec`) и значений из Lua, собирая определения биндингов `FGV2UiBindingDefinition`.
2. После подготовки candidate binding set в `FGV2UiBindingRegistry` функция `BuildFields` потребляет выданные `FGV2UiBindingHandle` и материализует candidate values в типизированные структуры `FGV2ScreenFieldValue`.

Каждый `FGV2ScreenFieldValue` содержит `FieldId` (`FName`), `SchemaId` (`FString`), материализованное значение `PreparedValue` (`TSharedPtr<const FGV2PreparedUiObject>`) и скомпилированную схему `CompiledSchema` (`std::shared_ptr<const FCompiledUiFieldSpec>`). C++ schema-specific классы-адаптеры отсутствуют.

Поддерживаемые стандартные схемы UI-полей:

| `schema_id` | Native Widget Class | Property Host Capabilities | `IGV2ScreenFieldHost`? |
|---|---|---|---|
| `core:schema.ui_field.rich_text.v3` | `WBP_RichText` / `UGV2RichTextWidgetBase` | `text` (Text), `spans` (RichTextSpans) | Нет — nested property host only |
| `core:schema.ui_field.button_list.v2` | `WBP_ButtonList` / `UGV2ButtonListWidgetBase` | `items` (CollectionHost для кнопок) | Нет — nested property host only |
| `core:schema.ui_field.checkbox.v1` | `WBP_Checkbox` / `UGV2CheckboxWidgetBase` | `key` (Key), `text` (Text), `is_checked` (Scalar), `binding` (Binding) | Нет — nested property host only |
| `core:schema.ui_field.input_field.v1` | `WBP_InputField` / `UGV2InputFieldWidgetBase` | `key` (Key), `label` (Text), `placeholder` (Text), `value` (Scalar), `binding` (Binding) | Нет — nested property host only |
| `core:schema.ui_field.dropdown_select.v1` | `WBP_DropdownSelect` / `UGV2DropdownSelectWidgetBase` | `placeholder` (Text), `selected_key` (Key), `options` (CollectionHost), `binding` (Binding) | Нет — nested property host only |
| `core:schema.ui_field.image.v1` | `WBP_Image` / `UGV2ImageWidgetBase` | `resource_id` (Ref), `key` (Key) | Нет — nested property host only |
| `core:schema.ui_field.progress_bar.v1` | `WBP_ProgressBar` / `UGV2ProgressBarWidgetBase` | `percent` (Scalar), `label` (Text), `key` (Key) | Нет — nested property host only |
| `core:schema.ui_field.portrait.v1` | `WBP_Portrait` / `UGV2PortraitWidgetBase` | `resource_id` (Ref), `frame_resource_id` (Ref), `key` (Key) | Нет — nested property host only |
| `core:schema.ui_field.modal.v1` | `WBP_Modal` / `UGV2ModalWidgetBase` | `title` (Text), `content` (Text), `buttons` (CollectionHost), `backdrop_close_action` (Binding) | Нет — nested property host only |
| `core:schema.ui_field.tab_container.v1` | `WBP_TabContainer` / `UGV2TabContainerWidgetBase` | `default_tab_key` (Key), `tabs` (CollectionHost) | Нет — nested property host only |
| `textsystem:schema.ui_field.location_top_bar.v1` | `UGV2LocationTopBarWidgetBase` | `day` (Text), `location` (Text), `primary_resource` (Text) | **Да** |
| `textsystem:schema.ui_field.location_player_status.v1` | `UGV2LocationPlayerStatusWidgetBase` | `name` (Text), `portrait_resource_id` (Ref), `meters` (CollectionHost), `items` (CollectionHost), `effects` (CollectionHost) | **Да** |
| `textsystem:schema.ui_field.location_scene.v1` | `UGV2LocationSceneWidgetBase` | `background_tile_resource_id` (Ref), `background_resource_id` (Ref), `context_text` (Text), `characters` (CollectionHost) | **Да** |
| `textsystem:schema.ui_field.location_commands.v1` | `UGV2LocationCommandPanelWidgetBase` | `items` (CollectionHost) | **Да** |

Первые десять строк — валидируемые, протестированные на уровне `PrepareUiHostProperties`/`CommitUiHostProperties` schema/capability пары; их Native Widget Class реализует `IGV2UiPropertyHost`, но не `IGV2ScreenFieldHost`, поэтому ни одна из них не может быть настроена как самостоятельный top-level Screen Field сейчас — только как nested property (вложенное свойство composite'а, например `CollectionHost` entry) либо материал для будущего host. Только последние четыре строки — реально используемый, production Screen Field pipeline (`textsystem:screen.location`, см. [Current vertical slice](#current-vertical-slice)).

Схема команд экрана локации — `textsystem:schema.ui_field.location_commands.v1` (namespace `textsystem`, не `core:schema.ui_field.button_list.v2`); она независима от generic `button_list.v2` несмотря на схожую форму (`items` CollectionHost) и покрыта отдельными тестами (`GV2UiPropertyHostTests.cpp`, `GV2PropertyConsumersTests.cpp`, `GV2RuntimeSubsystemTests.cpp`).

Production Lua document использует `TextSpec`; `UGV2TextPipeline` выполняет централизованное разрешение локализации, экранирование аргументов и форматирование разметки. Button binding содержит только семантический `command_id` и opaque `FGV2UiBindingHandle`, а не Lua callback.

### LocationTopBar Field Contract (`textsystem:schema.ui_field.location_top_bar.v1`)

Схема верхней информационной панели экрана локации отображает статус времени/дня, текущую локацию и основной ресурс игрока:

- `day` (required `TextSpec`): спецификация локализованного текста для отображения текущего дня/времени суток.
- `location` (required `TextSpec`): спецификация локализованного текста названия текущей локации.
- `primary_resource` (required `TextSpec`): спецификация локализованного текста для отображения запаса ключевого ресурса/валюты.

Схема является замкнутой: любые лишние ключи в значении поля отклоняются.

### LocationPlayerStatus Field Contract (`textsystem:schema.ui_field.location_player_status.v1`)

Схема статусной панели игрока экрана локации отображает имя, портрет, шкалы состояния, а также слоты предметов и активных эффектов:

- `name` (required `TextSpec`): спецификация локализованного текста имени персонажа игрока.
- `portrait_resource_id` (optional string): Stable ID ресурса портрета игрока (`core:resource.*` или `<game>:resource.*`). При отсутствии или пустой строке используется fallback-заглушка `"core:resource.ui.missing_portrait"`.
- `meters` (optional array of objects): упорядоченная коллекция шкал состояния персонажа (HP, выносливость, мана и др.).
  Каждый элемент массива `meters` обязан быть объектом со структурой:
  - `key` (required non-empty string / `FName`): уникальный в пределах массива идентификатор шкалы (например, `"hp"`, `"stamina"`); дубликаты и пустые строки отклоняются;
  - `percent` (optional number `0.0..1.0`): уровень заполнения шкалы. Принимаются оба числовых подтипа — с плавающей точкой и целый: `math.min(1, x)` в Lua возвращает целое при полном значении, поэтому отклонение целых обнуляло бы именно заполненную шкалу. Нечисловое значение отклоняется, а не подменяется нулём; числовое clamp-ится в диапазоне `[0.0, 1.0]` на границе, поэтому виджет никогда не получает значения вне диапазона;
  - `label` (optional `TextSpec`): спецификация текста, отображаемого поверх или рядом со шкалой.
  Элемент `meters` является замкнутым: посторонние ключи отклоняются.
- `items` (optional array of objects): коллекция иконок имеющихся предметов. Каждый элемент обязан быть объектом со структурой:
  - `key` (required non-empty string): идентичность предмета, а не его изображения. Ключ обязан удовлетворять [грамматике ключа повторяемого элемента](UIDocumentAndReconciliation.md#reconciliation) и быть уникальным в пределах коллекции; источником служит `instance_id` предмета либо иная семантическая стабильная идентичность. Вывод ключа из позиции в массиве или из `resource_id` запрещён: и то и другое меняется при событиях, не меняющих сам предмет, и переносит UI-local состояние между разными сущностями;
  - `resource_id` (optional string): Stable ID ресурса иконки. При отсутствии используется заглушка `"core:resource.ui.missing_icon"`.
- `effects` (optional array of objects): коллекция иконок активных эффектов; форма элемента и правило ключа совпадают с `items`.

Элементы `items` и `effects` являются замкнутыми: посторонние ключи отклоняются.

Схема поля и элементы `meters` являются замкнутыми: любые лишние ключи на любом уровне вложенности приводят к типизированному отказу построения и применения поля.

### LocationScene Field Contract (`textsystem:schema.ui_field.location_scene.v1`)

Схема поля сцены экрана локации описывает визуальное окружение и расположенных на сцене персонажей:

- `background_tile_resource_id` (optional string): Stable ID ресурса бесшовной фоновой текстуры/плитки (например, `"core:resource.ui.old_paper_tile_256"`).
- `background_resource_id` (optional string): Stable ID ресурса основного фонового арта сцены (`PreserveAspect`). При отсутствии ресурса подставляется fallback-заглушка `"textsystem:resource.ui.missing_background"`.
- `context_text` (optional `TextSpec`): контекстное художественное описание текущей обстановки локации.
- `characters` (optional array of objects): упорядоченная коллекция персонажей сцены, отрисовываемая через host динамической коллекции (`CharacterRepeater` / `CharacterContainer`) с масштабированием `PreserveAspect` и вертикальной привязкой к нижнему краю (`VAlign_Bottom`).
  Каждый элемент массива `characters` обязан быть объектом со структурой:
  - `key` (required non-empty string / `FName`): идентичность персонажа, а не его спрайта. Ключ обязан удовлетворять [грамматике ключа повторяемого элемента](UIDocumentAndReconciliation.md#reconciliation) и быть уникальным в пределах массива; нарушение грамматики, пустая строка и дубликаты отклоняются на фазах `PrepareLocationScene` / `BuildLocationScene`. Вывод ключа из `resource_id` запрещён: смена спрайта того же персонажа не является сменой персонажа;
  - `resource_id` (optional string): Stable ID ресурса портрета/спрайта персонажа (например, `"rh:resource.character.tavern_keeper"`). Если ресурс не задан, используется системная заглушка `"textsystem:resource.ui.missing_character"`.
  Элемент `characters` является замкнутым: посторонние ключи отклоняются.

Несоответствие контракта поля (включая невалидный тип элементов `characters`, посторонние ключи, дублирование ключей или передачу плоского массива строк) приводит к типизированному отказу применения поля (`CanApplyScreenFields`/`PrepareScreenFields` возвращает `false`), предотвращая повреждение presentation state.

### Location Commands Field Contract (`textsystem:schema.ui_field.location_commands.v1`)

Схема панели команд экрана локации — собственная схема namespace `textsystem`, а не generic `core:schema.ui_field.button_list.v2` (форма похожа — `items` CollectionHost, — но это разные, независимо версионируемые Stable ID):

- `items` (required array of objects): упорядоченный список доступных команд/кнопок навигации.
  Каждый элемент массива `items` обязан быть объектом со структурой:
  - `key` (required non-empty string / `FName`): уникальный в пределах массива идентификатор кнопки/команды;
  - `text` (required `TextSpec`): спецификация локализованного текста кнопки;
  - `binding` (optional `Binding` object): объект привязки семантической команды со структурой `{ command_id: string, args?: object }`.
- `key` (optional non-empty string / `FName`): собственный ключ панели, если панель сама является элементом внешней коллекции.

Схема поля и элементы `items` являются замкнутыми: посторонние ключи отклоняются.

### Designer Authoring Layer (ADR-0027)

Синтаксис `text`, `action`, `button`, `tab`, `tabs` и `show_*`, его package attribution и runtime adaptation задаёт [Authoring Surface Contract](../Architecture/AuthoringSurfaceContract.md). Этот contract владеет результатом: `TextSpec`, stable element keys, Screen Field schema, layer/instance identity и atomic apply. Authoring helper обязан создавать структуры этой модели и не может вводить callback, raw user-facing string или параллельный Screen format.

### Источник презентации и автоматическая инвалидация (SAS-14..16, ADR-0028)

Геймплейные команды не управляют интерфейсом и не вызывают перестроение экранов вручную. Вместо этого:

1. **Регистрация источника**: пакет регистрирует функцию-источник презентации через `game.presentation.register_source(fn)` на фазе `register`. Повторная регистрация (`PresentationSourceDuplicateRegistration`), невалидный тип (`InvalidPresentationSource`) и регистрация после freeze (`PresentationSourceRegistryFrozen`) отклоняются.
2. **Автоматическая инвалидация**: рантайм вызывает `game.presentation.resolve()` после каждой **успешно закоммиченной** команды вне окна мутации (`mutation_window`). При отказе или runtime fault источник не вызывается. Попытка мутации состояния из источника презентации блокируется ошибкой `MutationWindowClosed`.
3. **Шов под UI document**: источник презентации разрешает активный экран из текущего состояния и является архитектурным швом, который в будущем будет заменён маршрутизатором UI document без изменения геймплейного кода.

## Apply lifecycle

`ApplyScreenFields` выполняется атомарно через раздельные фазы Prepare и Commit:

1. `GV2ScreenFieldMaterializer::PrepareBindingDefinitions` валидирует поля документа против скомпилированных UI-схем и формирует упорядоченный candidate definitions set.
2. `FGV2SessionCoordinator` готовит кандидатный набор биндингов в `FGV2UiBindingRegistry`.
3. `GV2ScreenFieldMaterializer::BuildFields` потребляет выданные opaque handles и строит материализованные `FGV2ScreenFieldValue` с `PreparedValue` (`FGV2PreparedUiObject`) и `CompiledSchema` для каждого поля.
4. `UGV2ScreenWidgetBase::PrepareScreenFields` обходит дерево виджетов экрана, собирает сконфигурированные `IGV2ScreenFieldHost`, сопоставляет `GetScreenFieldId()` со списком полей документа (проверяя биекцию: каждое объявленное поле обязано иметь host, каждый сконфигурированный host обязан получить значение) и готовит мутационный план (`PrepareUiHostProperties`) off-tree для каждого поля без мутации живых виджетов UMG.
5. Предиктивная проверка `CanApplyScreenFields` выполняет фазу Prepare и отбрасывает план, гарантируя обнаружение ошибок глубоких детей и коллекций до вызова мутаций.
6. При ошибке подготовки хотя бы одного поля план мутаций отбрасывается, и виджеты остаются в прежнем состоянии (компенсирующий откат устранён, так как мутация не начиналась).
7. `UGV2ScreenWidgetBase::CommitScreenFields` исполняет подготовленный план мутаций (`CommitUiHostProperties`).
8. Только после полного успеха вызывается `OnScreenFieldsApplied` и коммитятся подготовленные биндинги ревизии.

`GetScreenFieldIds` возвращает сконфигурированные `field_id` экрана и используется validation/tests.

## Current vertical slice

`WBP_LocationScreen` (`screen_id = "textsystem:screen.location"`, `instance_key = "location"`) — единственный экран, реально проходящий через `GV2ScreenFieldMaterializer` + `IGV2ScreenFieldHost` discovery. Он объявляет ровно четыре Screen Field, по одному на каждый Location-композит:

| `field_id` | Existing element | Schema |
|---|---|---|
| `top_bar` | `UGV2LocationTopBarWidgetBase` | `textsystem:schema.ui_field.location_top_bar.v1` |
| `player_status` | `UGV2LocationPlayerStatusWidgetBase` | `textsystem:schema.ui_field.location_player_status.v1` |
| `scene` | `UGV2LocationSceneWidgetBase` | `textsystem:schema.ui_field.location_scene.v1` |
| `commands` | `UGV2LocationCommandPanelWidgetBase` | `textsystem:schema.ui_field.location_commands.v1` |

Lua presenter (`GameData/textsystem/scripts/presentation/location_presenter.lua`, `M.build_screen_request`) публикует все четыре поля через `game.presentation.register_source` при каждой успешно закоммиченной команде (см. [Источник презентации](#источник-презентации-и-автоматическая-инвалидация-sas-1416-adr-0028)). `GV2ScreenFieldMaterializer` генерически материализует значения полей и биндинги по скомпилированным схемам; Runtime разрешает class только через `DA_ScreenRegistry`. Идентичность route зафиксирована ([UI Document § Устойчивая идентичность LocationScreen](UIDocumentAndReconciliation.md)): `screen_id`/`instance_key` не меняются между локациями, переход обновляет поля существующего widget.

`WBP_Testscreen` (`core:screen.test`) остаётся отдельной, более старой proving-ground fixture: пять из шести её reusable-компонентов (`DescriptionText`, `CheckboxField`, `ClassSelectField`, `PlayerNameField`, `ButtonList`) — static leaves, не Screen Field hosts; Lua управляет каждым напрямую через его interaction API (`SubmitCheckboxState`, `SubmitTextValue`, `SubmitSelection`). Шестой, `GreetingText` (`WBP_Text`, DUC-02), — единственный настоящий Screen Field этого экрана: `HostIdentity = "greeting"`, `GetScreenFieldIds()` возвращает ровно `["greeting"]`; Lua публикует его через обычный `fields` (`GameData/sample/scripts/debug/start.lua`), генерический материализатор доставляет значение тем же путём, что и `WBP_LocationScreen`. См. [Widget Registry § Current WBP_Testscreen contract](WidgetRegistry.md#current-wbptestscreen-contract).

## Failure and recovery

- Invalid field contract запрещает interactive apply и создаёт structured diagnostic с screen/field/schema context.
- План мутаций готовится off-tree до мутации виджетов; отказ подготовки не трогает ни одного виджета, поэтому компенсирующий откат устранён физически.
- Failed candidate не изменяет current published screen/bindings.
- Unknown `screen_id` или invalid registry class открывает system error surface; Lua gameplay-state не меняется.

## Compatibility and evolution

- Добавление optional field совместимо, если старый template корректно работает после reset/default.
- Удаление required field, смена смысла `field_id` или несовместимая смена schema являются breaking change.
- Опубликованный `screen_id` или field schema Stable ID не переиспользуется для другого смысла.
- Layout/style/animation могут меняться без schema version, если observable field/input contract сохраняется.
- Новый field schema объявляется в данных (`GameData/<package>/schemas/`) с доменом `schema_domain: "ui_field"` / `"ui_value"` и не требует написания C++-адаптера; Session Coordinator изменять запрещено.
- LocationScreen fields принадлежат `textsystem`; raw string, raw asset path, physical layout value and invalid resource ID are rejected before apply. Semantic bindings создаёт только commands field.

## Verification

- `WBP_GameShell` имеет native parent `UGV2GameShellWidgetBase`; все шесть layer hosts существуют в его отображаемом Widget tree. Отсутствующий host не заменяется runtime fallback-контейнером.
- `WBP_ScreenBase` загружается как abstract Blueprint class и имеет native parent `UGV2ScreenWidgetBase`.
- `WBP_Testscreen` является его child class и компилируется без test-specific native parent.
- `DA_ScreenRegistry` загружается через config, содержит `core:screen.test` и разрешает concrete non-abstract child `WBP_ScreenBase`.
- `WBP_Testscreen` содержит deterministic `ButtonList`, `CheckboxField`, `ClassSelectField`, `DescriptionText`, `PlayerNameField` по имени (`BindWidget`/named children) — они static leaves, не Screen Fields; единственный настоящий Screen Field этого экрана — `greeting` (`GreetingText`, DUC-02), required, со схемой `core:schema.ui_field.text.v1`.
- `DescriptionSurface` ограничивает `WBP_RichText` оставшейся высотой экрана; длинный текст переносится и прокручивается внутри блока.
- Unknown, duplicate, missing required и schema mismatch payloads отклоняются до mutation.
- Предиктивный preflight `CanApplyScreenFields` предсказывает ошибки глубоких детей до мутаций (`ScreenPreflightPredictsDeepChildFailure`).
- Button click пересекает boundary только как opaque handle и проходит Semantic Input/Command Dispatcher.
- Checkbox change пересекает boundary как opaque handle + `is_checked`, после чего Lua публикует новое desired state.
- Input commit пересекает boundary как opaque handle + `value`, после чего Lua публикует новое desired state.
- Dropdown option activation пересекает boundary ровно один раз как opaque handle + `selected_key`, после чего Lua публикует новое desired state.
- Добавление нового Screen Blueprint и registry entry не требует изменения C++.
- Runtime source не содержит `/Game/UI/Widgets/WBP_Testscreen` и не принимает Blueprint class из Lua/Blueprint façade.
- Session Coordinator не содержит concrete Screen Field schema IDs; универсальная валидация UI-полей работает поверх скомпилированных `FCompiledUiFieldSpec` из репозитория и отклоняет unknown/duplicate registration и незамкнутые поля.
- Automation проходит через обычные Session, Semantic Input и Screen request entry points; test-only runtime methods отсутствуют.
