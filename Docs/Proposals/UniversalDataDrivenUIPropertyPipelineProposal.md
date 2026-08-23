---
title: Universal Data-Driven UI Property Pipeline Proposal
status: draft
proposal_state: accepted_for_planning
version: 1.1
updated: 2026-08-23
depends_on:
  - ../UI/ScreenTemplates.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../UI/WidgetRegistry.md
  - ../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../Status/ImplementationStatus.md
decisions:
  - ../ADR/0035-ui-foundation-and-composition.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
---

# Universal Data-Driven UI Property Pipeline

> **Предлагает:** заменить schema-specific цепочку DTO и адаптеров Screen Field одним универсальным property-pipeline с data-driven UI schemas и раздельными фазами Prepare/Commit.
> **Мотив:** повторяющийся класс дефектов «свойство принято границей и молча не применено», воспроизведённый в четырёх раундах проверки подряд.
> **Состояние:** принято к планированию с обязательным гейтом go/no-go после фазы 3 (раздел 26).

## 1. Статус и краткое решение

Этот proposal заменяет текущую schema-specific цепочку:

```text
Lua FValue
    ↓
schema_id
    ↓
FGV2ScreenFieldAdapterRegistry
    ↓
PrepareFoo / BuildFoo
    ↓
FGV2FooViewModel
    ↓
ApplyFooModel
    ↓
Widget
```

на единый pipeline:

```text
Lua portable FValue
    ↓
data-driven UI schema
    ↓
generic portable validation
    ↓
generic UE preparation
    ├── TextSpec  -> prepared Text
    ├── Stable ID -> validated StableId
    ├── BindingSpec -> prepared opaque BindingHandle
    └── primitive/object/array -> prepared value tree
    ↓
FGV2PreparedScreenField
    {
        field_id,
        schema_id,
        properties
    }
    ↓
IGV2UiPropertyHost
    ↓
registered property consumers
    ↓
Prepare mutation plan
    ↓
atomic Commit
    ↓
physical UMG state
```

Главные принятые решения:

1. **Сам Lua -> UE Screen Field становится универсальным property-map.**
2. **UI schemas становятся data-driven** и загружаются через существующий
   `GameData/<package>/schemas` pipeline.
3. Моды могут объявлять собственные UI schemas **только как композицию
   стандартных Core schema kinds**.
4. Новый primitive/schema kind, новый renderer pipeline или новый semantic
   binding type требует Core C++.
5. Для конкретного Widget действует отношение:

   ```text
   SchemaContract ⊆ WidgetCapabilities
   ```

   Widget может уметь больше, чем использует конкретная schema, но **каждое
   property из schema обязано иметь совместимый consumer**.
6. Property names schema непосредственно совпадают с property IDs Widget.
   Alias/mapping layer запрещён.
7. Все потенциально ошибочные действия выполняются в **Prepare** без изменения
   live UI. **Commit** не должен выполнять validation/resolution/loading и
   проектируется практически infallible.
8. Text, image/resource, binding и repeated collection остаются
   стандартизированными Core pipelines; generic property system не создаёт
   альтернативный путь их применения.
9. После миграции schema-specific Screen Field DTO и
   `FGV2ScreenFieldAdapterRegistry` удаляются.
10. Этим же переходом закрываются все UI defects, обнаруженные в review rounds,
    включая REV3-01...REV3-10 и оставшиеся связанные gaps атомарности/preflight.

---

# 2. Причина изменения

Текущий UI boundary уже использует небольшой набор фундаментальных значений:

```text
null
bool
integer
number
string
TextSpec
Stable ID
BindingSpec
array
object
```

Но после пересечения Lua/C++ boundary эти простые значения искусственно
превращаются в большое число schema-specific DTO:

```text
FGV2ButtonViewModel
FGV2CheckboxViewModel
FGV2InputFieldViewModel
FGV2DropdownSelectViewModel
FGV2InteractiveRichTextViewModel
FGV2ProgressBarViewModel
FGV2PortraitViewModel
FGV2ModalViewModel
FGV2LocationTopBarViewModel
FGV2LocationPlayerStatusViewModel
FGV2LocationSceneViewModel
FGV2TabContainerViewModel
...
```

Затем для каждой schema существует отдельная ручная цепочка:

```text
PrepareXxx
BuildXxx
ApplyXxx
CaptureXxx
CanApplyXxx
```

Эта структура уже породила повторяющийся класс дефектов.

## 2.1. Whitelist != consumption

Adapter объявляет property допустимым, но `BuildXxx` его не переносит:

```text
schema accepts:
    is_read_only
    max_length

typed model:
    этих значений нет
```

Ошибочные данные успешно проходят boundary и исчезают.

## 2.2. Partial typed-model consumption

Typed model содержит значение, но composite применяет только часть:

```text
FGV2ProgressBarViewModel
    Percent
    Label

LocationPlayerStatus:
    применяет Percent
    теряет Label
```

## 2.3. Lost failure propagation

Child/leaf сообщает failure, parent игнорирует его:

```text
TextPipeline::Apply() == false
    ↓
Button.ApplyButtonModel() == void
    ↓
ButtonList ApplyItem() == true
    ↓
Screen apply == success
```

## 2.4. Duplicate local pipelines

Composite начинает самостоятельно делать:

```cpp
TextBlock->SetText(...)
```

вместо общего `UGV2TextPipeline`.

То же возможно для images, bindings и repeated children.

## 2.5. Schema-specific C++ ограничивает data-driven extensibility

Чтобы добавить новую комбинацию уже существующих типов данных, сегодня
необходимо писать C++ adapter и новый DTO, даже когда physical Widget уже умеет
показывать все необходимые properties.

Это противоречит общей архитектуре проекта:

```text
gameplay/content/data evolve independently
UE owns presentation implementation
mods extend via data + Lua
```

---

# 3. Цели

## 3.1. Основные цели

Новая архитектура должна гарантировать:

1. Одну универсальную representation Screen Field после Lua boundary.
2. Data-driven validation UI properties.
3. Один generic preparation path.
4. Один generic Widget property apply lifecycle.
5. Структурную невозможность состояния:

   ```text
   schema recognized property
   but implementation silently ignored it
   ```

6. Структурную невозможность успешного apply при child failure.
7. Полную централизацию Text/Image/Binding pipelines.
8. Predictive deep preflight до UI mutation.
9. Transactional field, collection, screen и document application.
10. Возможность data-only mod schemas без нового C++.
11. Headless validation тех же UI schemas без зависимости от UE.
12. Сокращение публичной площади `GV2BridgeTypes.h`.
13. Удаление большей части schema-specific conversion code.
14. Сохранение UE ownership физического layout и renderer implementation.
15. Сохранение canonical gameplay state исключительно в Lua.

## 3.2. Non-goals

Этот proposal **не**:

- передаёт физический UMG tree в Lua;
- позволяет Lua выбирать Widget Blueprint class;
- создаёт внешний screen editor;
- переносит gameplay decisions в UE;
- разрешает mod schema создавать новый primitive kind;
- разрешает mod schema обходить Text/Image/Input pipelines;
- решает `STATUS-001` session replacement lifecycle;
- реализует `STATUS-002` Presentation Effects.

`STATUS-001` и `STATUS-002` остаются отдельными архитектурными задачами.

`STATUS-003` и `STATUS-004` непосредственно связаны с новым pipeline и
предлагается закрыть в рамках этой миграции.

---

# 4. Текущее состояние, которое заменяется

Текущий portable envelope:

```json5
{
  field_id: "player_status",
  schema_id: "textsystem:schema.ui_field.location_player_status.v1",
  value: {
    ...
  },
}
```

сам по себе правильный и сохраняется.

Проблема находится **после** envelope:

```text
schema_id
    ↓
hardcoded FGV2ScreenFieldAdapterRegistry
    ↓
schema-specific function
    ↓
schema-specific USTRUCT
```

Текущий `FGV2ScreenFieldValue` фактически хранит все возможные payload
одновременно:

```text
InteractiveRichTextValue
ButtonListValue
CheckboxValue
InputFieldValue
DropdownSelectValue
ImageValue
ProgressBarValue
PortraitValue
ModalValue
LocationTopBarValue
LocationPlayerStatusValue
LocationSceneValue
TabContainerValue
...
```

Новый pipeline удаляет эту модель.

---

# 5. Целевая модель boundary

## 5.1. Portable stage остаётся на существующем `FValue`

Lua не должен знать UE semantic types.

Boundary продолжает использовать существующий:

```text
GV2RuntimeCore::FValue
```

с фундаментальными portable variants:

```text
null
boolean
integer
number
string
array
object
```

`TextSpec`, `BindingSpec` и Stable ID остаются portable структурными значениями.

Не вводится второй Lua-side Variant.

## 5.2. Screen Field envelope

Envelope сохраняется:

```json5
{
  field_id: "stamina",
  schema_id: "core:schema.ui_field.progress_bar.v2",
  value: {
    percent: 0.82,
    label: {
      text_id: "core:text.location.stamina",
      args: { stamina: 82 },
      style: "body",
    },
  },
}
```

После generic preparation UE получает:

```text
FGV2PreparedScreenField
    FieldId
    SchemaId
    CompiledSchema
    Properties: FGV2PreparedUiObject
```

Никакого `ProgressBarValue` внутри нет.

---

# 6. UE prepared value tree

Portable `FValue` недостаточен внутри UE, потому что после preparation появляются:

- localized `FText`;
- semantic typography;
- opaque session binding handles;
- проверенные Stable IDs;
- подготовленные nested values.

Предлагается UE-only recursive value tree.

## 6.1. `FGV2PreparedUiValue`

Логические kinds:

```text
Null
Boolean
Integer
Number
String
Key
Text
StableId
Binding
Object
Array
```

### Boolean

```text
bool
```

### Integer

```text
int64
```

### Number

```text
double
```

### String

Обычная portable строка, не имеющая специальной semantics.

### Key

Проверенный repeated/local identity.

Не является display text.

### Text

Существующий semantic resolved payload:

```text
FGV2TextViewModel
    FText Text
    FName StyleToken
    FString NormalizedMarkup
```

### StableId

```text
canonical ID
kind
```

Например:

```text
core:resource.character.aria
kind = resource
```

или:

```text
core:screen.inventory
kind = screen
```

`Resource` не требует отдельного fundamental variant: это `StableId` с
`target_kind = resource`.

### Binding

Только:

```text
FGV2UiBindingHandle
```

Widget никогда не получает `command_id`, args или Lua callback.

### Object

Immutable ordered property collection.

### Array

Immutable ordered values.

## 6.2. Representation

Prepared tree не обязан быть `BlueprintType`.

Рекомендуется private native representation:

```text
FGV2PreparedUiValue
FGV2PreparedUiObject
FGV2PreparedUiArray
```

на основе `std::variant` / `TVariant` + immutable shared nodes.

Причина:

- recursive USTRUCT не нужен Blueprint;
- меньше reflection overhead;
- проще обеспечить immutable prepared candidate;
- легче гарантировать deterministic iteration.

Object хранит:

```text
sorted property entries
+
optional lookup index
```

Порядок обхода properties канонический и не зависит от JSON parser map order.

---

# 7. Data-driven UI schemas

## 7.1. Используется существующий schema pipeline

UI schemas находятся вместе с остальными package schemas:

```text
GameData/<package>/schemas/
```

Например:

```text
GameData/core/schemas/ui_button_list_v3.schema.json5
GameData/core/schemas/ui_input_field_v2.schema.json5
GameData/core/schemas/ui_modal_v2.schema.json5

GameData/textsystem/schemas/ui_location_player_status_v2.schema.json5
```

Новый отдельный UI-schema loader не создаётся.

Существующий package discovery, provider ordering, diagnostics и repository
build используются повторно.

## 7.2. Schema domain

Существующие definition schemas продолжают работать без обязательной миграции.

Для UI добавляется:

```json5
schema_domain: "ui_field"
```

Для reusable nested schema:

```json5
schema_domain: "ui_value"
```

Existing schemas с `definition_type` продолжают интерпретироваться как
definition schemas.

## 7.3. Standard Core schema kinds

### Scalar

```text
bool
integer
number
string
```

### Semantic

```text
key
text
ref
binding
```

### Structural

```text
object
array
screen_fields
```

### Composition directive

```text
schema_ref
```

`schema_ref` не является runtime value kind. Compiler заменяет его referenced
compiled schema node.

## 7.4. `key`

```json5
{
  kind: "key",
  required: true,
}
```

Generic validator применяет единую grammar:

```text
[a-z0-9_.@:-]+
length 1..192
```

и запрет text-derived identity.

### `key` не приводится из `string`

`key` является отдельным fundamental kind, а не строкой с соглашением.
Объявление вида:

```text
key: { kind: "string" }
```

является ошибкой компиляции schema, а не допустимым сокращением. Причина
записана дефектами предыдущих раундов: идентичность, выведенная из
отображаемого значения (`resource_id` персонажа) или из позиции в массиве
(`item_0`), дважды переносила UI-local состояние между разными сущностями.
Пока идентичность выразима строкой, это повторится.

## 7.5. `text`

```json5
{
  kind: "text",
  required: true,
}
```

Portable validation:

- объект является valid TextSpec;
- `text_id` — Stable ID kind `text`;
- args portable;
- style token имеет допустимую grammar.

UE preparation:

```text
TextSpec
    ↓
UGV2TextPipeline preparation
    ↓
FGV2TextViewModel
```

## 7.6. `ref`

```json5
{
  kind: "ref",
  target_kind: "resource",
  required: true,
}
```

Или:

```json5
{
  kind: "ref",
  target_kind: "screen",
  required: true,
}
```

Portable validator проверяет Stable ID grammar/kind и repository constraints,
если target является repository entity.

## 7.7. `binding`

```json5
{
  kind: "binding",
  required: true,
  input_schema_id: "core:schema.ui_input.dropdown_selected.v1",
}
```

`input_schema_id` optional.

Button с no-value click:

```json5
{
  kind: "binding",
  required: true,
}
```

Binding node generic preparer:

1. validates BindingSpec;
2. создаёт binding definition;
3. записывает placeholder ordinal;
4. после `PrepareBindings` materializes соответствующий
   `FGV2UiBindingHandle`.

## 7.8. `object`

UI objects всегда closed.

Пример:

```json5
{
  kind: "object",
  fields: {
    text: { kind: "text", required: true },
    enabled: { kind: "bool", required: false, default: true },
  },
}
```

Для `ui_field` запрещается `open: true`.

Неизвестный property — fatal candidate error.

## 7.9. `array`

```json5
{
  kind: "array",
  keyed_by: "key",
  items: {
    kind: "object",
    fields: {
      key: { kind: "key", required: true },
      text: { kind: "text", required: true },
    },
  },
}
```

Если collection участвует в Widget reconciliation, `keyed_by` обязателен.

Array subtree, содержащий `binding`, также обязан иметь stable keyed identity.

Индекс массива никогда не используется как Widget identity.

## 7.10. `screen_fields`

Специальный Core structural kind для nested screens, прежде всего TabContainer.

Portable форма:

```json5
fields: {
  description: {
    schema_id: "core:schema.ui_field.rich_text.v3",
    value: { ... },
  },
  controls: {
    schema_id: "core:schema.ui_field.button_list.v3",
    value: { ... },
  },
}
```

Generic preparer рекурсивно применяет UI schemas каждого nested field.

Моды могут **использовать** `screen_fields`, но не могут изменить его semantics.

## 7.11. `schema_ref`

Пример:

```json5
items: {
  kind: "array",
  keyed_by: "key",
  items: {
    kind: "schema_ref",
    schema_id: "core:schema.ui_value.button_item.v1",
  },
}
```

Compiler:

- разрешает reference до repository freeze;
- запрещает cycle;
- provenance сохраняется;
- runtime lookup не выполняется.

---

# 8. Пример schemas

## 8.1. ProgressBar

```json5
{
  id: "core:schema.ui_field.progress_bar.v2",
  schema_domain: "ui_field",
  schema_version: 2,

  root: {
    kind: "object",
    fields: {
      percent: {
        kind: "number",
        required: true,
        min: 0.0,
        max: 1.0,
      },

      label: {
        kind: "text",
        required: false,
      },
    },
  },
}
```

`style` отсутствует. Visual ProgressBar style принадлежит UE Theme.

## 8.2. Button item

```json5
{
  id: "core:schema.ui_value.button_item.v1",
  schema_domain: "ui_value",
  schema_version: 1,

  root: {
    kind: "object",
    fields: {
      key: {
        kind: "key",
        required: true,
      },

      text: {
        kind: "text",
        required: true,
      },

      binding: {
        kind: "binding",
        required: true,
      },
    },
  },
}
```

## 8.3. ButtonList

```json5
{
  id: "core:schema.ui_field.button_list.v3",
  schema_domain: "ui_field",
  schema_version: 3,

  root: {
    kind: "object",
    fields: {
      items: {
        kind: "array",
        required: true,
        keyed_by: "key",
        items: {
          kind: "schema_ref",
          schema_id: "core:schema.ui_value.button_item.v1",
        },
      },
    },
  },
}
```

## 8.4. InputField

```json5
{
  id: "core:schema.ui_field.input_field.v2",
  schema_domain: "ui_field",
  schema_version: 2,

  root: {
    kind: "object",
    fields: {
      text: {
        kind: "text",
        required: false,
      },

      placeholder_text: {
        kind: "text",
        required: false,
      },

      value: {
        kind: "string",
        required: true,
        default: "",
      },

      is_read_only: {
        kind: "bool",
        required: false,
        default: false,
      },

      max_length: {
        kind: "integer",
        required: false,
        min: 0,
      },

      binding: {
        kind: "binding",
        required: true,
        input_schema_id: "core:schema.ui_input.input_value.v1",
      },
    },
  },
}
```

То есть ранее accepted-but-ignored `is_read_only` и `max_length` становятся
реальными supported properties, а не удаляются молча.

## 8.5. Modal

```json5
{
  id: "core:schema.ui_field.modal.v2",
  schema_domain: "ui_field",
  schema_version: 2,

  root: {
    kind: "object",
    fields: {
      title: {
        kind: "text",
        required: true,
      },

      content: {
        kind: "text",
        required: true,
      },

      buttons: {
        kind: "array",
        required: false,
        keyed_by: "key",
        items: {
          kind: "schema_ref",
          schema_id: "core:schema.ui_value.button_item.v1",
        },
      },

      backdrop_close_action: {
        kind: "binding",
        required: false,
      },
    },
  },
}
```

## 8.6. LocationPlayerStatus

```json5
{
  id: "textsystem:schema.ui_field.location_player_status.v2",
  schema_domain: "ui_field",
  schema_version: 2,

  root: {
    kind: "object",
    fields: {
      name: {
        kind: "text",
        required: true,
      },

      portrait_resource_id: {
        kind: "ref",
        target_kind: "resource",
        required: false,
      },

      meters: {
        kind: "array",
        required: false,
        keyed_by: "key",
        items: {
          kind: "object",
          fields: {
            key: {
              kind: "key",
              required: true,
            },

            percent: {
              kind: "number",
              required: true,
              min: 0.0,
              max: 1.0,
            },

            label: {
              kind: "text",
              required: false,
            },
          },
        },
      },

      items: {
        kind: "array",
        required: false,
        keyed_by: "key",
        items: {
          kind: "object",
          fields: {
            key: {
              kind: "key",
              required: true,
            },

            resource_id: {
              kind: "ref",
              target_kind: "resource",
              required: true,
            },
          },
        },
      },

      effects: {
        kind: "array",
        required: false,
        keyed_by: "key",
        items: {
          kind: "object",
          fields: {
            key: {
              kind: "key",
              required: true,
            },

            resource_id: {
              kind: "ref",
              target_kind: "resource",
              required: true,
            },
          },
        },
      },
    },
  },
}
```

## 8.7. TabContainer

```json5
{
  id: "core:schema.ui_field.tab_container.v2",
  schema_domain: "ui_field",
  schema_version: 2,

  root: {
    kind: "object",
    fields: {
      default_tab_key: {
        kind: "key",
        required: false,
      },

      tabs: {
        kind: "array",
        required: true,
        keyed_by: "key",

        items: {
          kind: "object",
          fields: {
            key: {
              kind: "key",
              required: true,
            },

            title: {
              kind: "text",
              required: true,
            },

            screen_id: {
              kind: "ref",
              target_kind: "screen",
              required: true,
            },

            fields: {
              kind: "screen_fields",
              required: false,
            },
          },
        },
      },
    },
  },
}
```

---

# 9. Mod extensibility

## 9.1. Разрешено

Мод может:

- создать schema ID в **своём namespace**;
- использовать standard Core kinds;
- создавать новые object/array combinations;
- использовать `schema_ref`;
- задавать более строгие scalar constraints;
- ссылаться на собственные text/resource/screen IDs своего package;
- использовать standard Core binding input contracts;
- использовать schema там, где extension point/Screen Template предоставляет
  compatible physical Widget.

Пример:

```json5
{
  id: "mymod:schema.ui_field.reputation_meter.v1",
  schema_domain: "ui_field",
  root: {
    kind: "object",
    fields: {
      percent: {
        kind: "number",
        required: true,
        min: 0.0,
        max: 1.0,
      },
      label: {
        kind: "text",
        required: true,
      },
    },
  },
}
```

Эта schema может использовать существующий ProgressBar capability без нового C++.

## 9.2. Запрещено

Мод не может:

- создать новый `kind`;
- зарегистрировать C++ property consumer;
- зарегистрировать renderer pipeline;
- передать raw UClass/Blueprint path;
- передать `FSlateBrush`, `UTexture`, font class/path;
- создать новый binding semantics/input serializer без Core consumer;
- override чужой Core UI schema ID;
- объявить open UI object;
- отключить key validation;
- отключить Text/Image pipeline;
- подключить Lua closure как Widget callback.

## 9.3. Schema ownership

UI schema ID принадлежит package namespace.

```text
core:schema...
```

может объявляться только Core.

```text
textsystem:schema...
```

может объявляться только TextSystem.

Mod может создать только:

```text
<mod>:schema...
```

UI schemas считаются compatibility contracts.

Breaking semantic change опубликованной schema требует нового versioned ID
согласно общей Compatibility Policy.

## 9.4. Несовместимая mod schema не роняет сессию

Раздел 17.1 требует проверять совместимость до session Ready. Для Core и
TextSystem это верно: несовместимая schema там — ошибка сборки проекта, и
сессия обязана не стартовать.

Для мода такое поведение неприемлемо: сторонние данные не должны иметь
возможности сделать игру незапускаемой. Поэтому исход зависит от владельца
schema:

| Владелец schema | Несовместимость с Widget capability |
|---|---|
| `core`, `textsystem`, `rh` | Session не становится `Ready`; ошибка сборки/загрузки |
| Пакет мода | Отклоняется сам мод с диагностикой раздела 32; сессия продолжается без него |

Это решение trust boundary (раздел 34), и его следует зафиксировать в ADR фазы
0, а не оставлять на усмотрение реализации: сегодня оба поведения выглядят
одинаково правдоподобно, и выбор по умолчанию окажется случайным.

---

# 10. Widget capabilities

Data-driven schema не знает UMG.

Physical Widget публикует только **capability tree**.

## 10.1. Fundamental invariant

```text
SchemaContract ⊆ WidgetCapabilities
```

Пример Widget:

```text
Button capabilities:

text     : Text
binding  : Binding(no input)
enabled  : Boolean
icon     : StableId(resource)
tooltip  : Text
```

Schema:

```text
text
binding
```

валидна.

Schema:

```text
text
binding
unknown_property
```

невалидна.

Schema:

```text
text : String
```

невалидна, потому что Widget ожидает `Text`.

## 10.2. Никаких alias

Запрещено:

```text
schema "label"
    ↓ alias
widget "text"
```

Property ID обязан совпадать:

```text
schema "text"
widget capability "text"
```

Это позволяет автоматически проверять coverage.

## 10.2a. Capability обязана быть наблюдаемой

Инвариант `SchemaContract ⊆ WidgetCapabilities` сам по себе класс дефектов не
закрывает. Он проверяет, что у свойства есть **объявленный** consumer, а
объявление capability пишется руками — ровно так же, как сегодня руками пишется
список `ConsumedKeys`.

Прогоните текущий дефект через новую модель:

```text
ProgressBar объявляет capability:
    percent
    label

schema использует оба
static check проходит

label consumer зарегистрирован,
но пишет в непривязанный LabelText
    ↓
значение снова теряется молча
```

Без дополнительного правила ложь переезжает из адаптера в таблицу capability, и
вокруг неё становится больше механики. Поэтому объявление capability обязано
быть **проверяемым**, а не декларативным:

1. **Связь с физическим target.** Объявленная capability обязана указывать
   конкретный renderer/collection target и считаться невыполненной, если этот
   target не привязан на экземпляре. Проверка выполняется до публикации экрана
   (раздел 17.2) и является нормативной частью инварианта, а не вспомогательной
   проверкой.
2. **Наблюдаемость.** Capability, применение которой не меняет наблюдаемого
   состояния виджета, не является capability. Формально:

   ```text
   для каждой объявленной capability C виджета W:
       существуют значения A != B, для которых
       Capture(W после Commit(A)) != Capture(W после Commit(B))
   ```

Второе правило превращает объявление в фальсифицируемое утверждение. Именно его
отсутствие делает возможными все дефекты раздела 2: `whitelist != consumption`
означает ровно «объявление не наблюдаемо».

## 10.3. Recursive compatibility

Compatibility проверяется рекурсивно:

- property exists;
- kind compatible;
- `ref.target_kind` compatible;
- numeric schema range входит в consumer-supported range;
- binding input contract compatible;
- object child properties compatible;
- array item contract compatible;
- keyed collection policy compatible.

Extra Widget capability разрешена.

Extra schema property — fatal.

---

# 11. `IGV2UiPropertyHost`

Текущий единый UCLASS parent невозможен без искусственного изменения CommonUI
иерархии:

```text
UGV2ButtonWidgetBase
    -> UCommonButtonBase

многие остальные
    -> UCommonUserWidget
```

Поэтому общее поведение задаётся интерфейсом + reusable native helper.

## 11.1. Новый interface

```text
IGV2UiPropertyHost
```

Conceptual API:

```cpp
DescribeUiCapabilities(FGV2UiCapabilityBuilder&) const;

bool PrepareUiProperties(
    const FGV2PreparedUiObject& Candidate,
    const FCompiledUiSchema& Schema,
    FGV2UiHostMutationPlan& OutPlan) const;

void CommitUiProperties(
    FGV2UiHostMutationPlan&& Plan);

const FGV2PreparedUiObject& GetLastCommittedUiProperties() const;
```

`CommitUiProperties` не выполняет fallible lookup/validation.

## 11.2. Reusable helper

Каждый host содержит generic helper:

```text
FGV2UiPropertyHostState
```

Он:

- хранит registered consumers;
- строит capability tree;
- проверяет schema coverage;
- подготавливает mutation plans;
- хранит last committed desired properties;
- выполняет commit;
- ведёт diagnostics.

Widget-specific `ApplyFooModel()` больше не является primary runtime path.

## 11.3. Dynamic Screen Element

`IGV2DynamicScreenElement` сохраняется как Screen-specific identity/config surface:

```text
GetScreenFieldDescriptor()
    FieldId
    SchemaId
    Required
```

Но schema-specific:

```text
CanApplyScreenField(FGV2ScreenFieldValue)
ApplyScreenField(FGV2ScreenFieldValue)
CaptureScreenField(...)
```

удаляются после миграции.

`UGV2ScreenWidgetBase`:

1. discovery Dynamic Screen Elements;
2. получает configured `SchemaId`;
3. получает `IGV2UiPropertyHost`;
4. сверяет compiled schema с capabilities;
5. готовит generic field mutation.

Nested Widget, например Button item, реализует `IGV2UiPropertyHost`, но не обязан
быть Dynamic Screen Element.

---

# 12. Standard property consumers

Property Host не должен вручную делать:

```cpp
Properties.Find("text")
Properties.Find("binding")
```

для каждого Widget.

Он регистрирует reusable consumers.

## 12.1. Text consumer

```text
TextPropertyConsumer
```

Responsibilities:

- принимает только prepared `Text`;
- preflight renderer compatibility;
- использует `UGV2TextPipeline`;
- подготавливает resolved CommonUI/Slate style;
- учитывает DPI;
- различает plain/rich renderer capabilities;
- Commit применяет уже подготовленный renderer state.

Raw:

```cpp
SetText(...)
```

для runtime-authored content за пределами Text consumer запрещён.

## 12.2. StableId resource consumer

```text
ImageResourcePropertyConsumer
```

Responsibilities:

- принимает `StableId(resource)`;
- проверяет target `ScalePolicy`;
- разрешает resource через `FGV2ImagePresentation`;
- проверяет render-mode compatibility;
- подготавливает brush;
- Commit только меняет prepared brush/resource state.

Raw brush mutation за consumer запрещена.

## 12.3. Primitive consumers

```text
BooleanPropertyConsumer
IntegerPropertyConsumer
NumberPropertyConsumer
StringPropertyConsumer
KeyPropertyConsumer
```

Generic type/range validation уже выполнена schema engine.

Consumer отвечает только за target-specific supported range/configuration.

## 12.4. Binding consumer

```text
BindingPropertyConsumer
```

Принимает только:

```text
FGV2UiBindingHandle
```

и знает ожидаемый input contract physical Widget.

Widget event handler не знает Command ID.

## 12.5. Object consumer

Делегирует nested object в nested property host/capability.

## 12.6. Keyed collection consumer

```text
KeyedCollectionPropertyConsumer
```

использует `FGV2KeyedCollection` как structural primitive, но добавляет
property-host transactionality reused children.

Responsibilities:

1. schema требует `keyed_by`;
2. keys validated generic schema engine;
3. existing children match по key;
4. new children создаются off-tree;
5. каждый child является `IGV2UiPropertyHost`;
6. **Prepare child properties выполняется для всех children до mutation**;
7. если любой child prepare fails — live collection не меняется;
8. Commit применяет prepared child plans;
9. container reorder/add/remove фиксируется только после успешной подготовки
   всей коллекции.

Это возвращает сильную гарантию item-state atomicity, которой не хватает
низкоуровневому `FGV2KeyedCollection`.

## 12.7. Screen consumer

Для nested Screen, например Tab:

```text
ScreenPropertyConsumer
```

Responsibilities:

- `screen_id` resolved только через Screen Registry;
- class создаётся off-tree;
- nested `screen_fields` generic prepare;
- никаких `CreateWidget<UGV2ScreenWidgetBase>(StaticClass())` fallback;
- failure child field preparation блокирует parent;
- Commit вставляет уже полностью prepared child Screen.

---

# 13. Prepare -> Commit вместо fallible Apply

Это центральный lifecycle proposal.

## 13.1. Почему недостаточно `CanApply + Apply`

Текущая модель допускает:

```text
CanApply == true
Apply:
    child A mutated
    child B fails
```

или:

```text
child returns false
parent ignores
parent returns true
```

В новой архитектуре fallible work выносится из mutation phase.

## 13.2. Property Prepare

Для каждого property:

```text
Prepare(value, target)
```

может:

- проверить renderer host;
- проверить style token;
- resolve Text style;
- normalize markup;
- resolve image;
- проверить ScalePolicy;
- проверить binding input contract;
- создать child Widget off-tree;
- подготовить nested child mutation;
- проверить collection host;
- проверить Screen Registry;
- создать nested Screen off-tree.

Но **не меняет live presentation state**.

## 13.3. Property Commit

После successful prepare всей candidate tree:

```text
Commit(plan)
```

выполняет только уже подготовленные mutations.

Commit:

- не грузит asset;
- не ищет schema;
- не валидирует property;
- не resolve-ит localization;
- не создаёт binding;
- не создаёт child class, который может отсутствовать;
- не вызывает fallible child apply.

Expected commit result — no-fail.

Unexpected engine-level commit failure считается invariant violation и
переводит presentation в controlled rebuild/recovery path, а не в обычный
`false` от leaf Widget.

### 13.3a. Аварийный путь Commit обязан быть определён и проверяем

Формулировка «переводит в controlled rebuild/recovery path» является
утверждением без средства измерения до тех пор, пока этот путь не описан. Такие
утверждения в этом репозитории уже дважды переживали закрытие плана и
обнаруживались следующим раундом проверки.

Поэтому ADR фазы 0 обязан зафиксировать **наблюдаемое** поведение отказа
Commit, а не намерение:

1. Отказ Commit является нарушением инварианта, а не обычным результатом.
2. Экран, чей Commit отказал, **не публикуется** как interactive; предыдущая
   ревизия остаётся активной.
3. Отказ обязан произвести структурированную диагностику формата раздела 32 с
   полным `property_path`.
4. Поведение проверяется **инъекцией отказа**: тест принудительно проваливает
   commit одного property и утверждает, что предыдущая ревизия цела, экран не
   стал interactive, а диагностика содержит путь до проваленного property.

Пункт 4 не является опциональным. Без него разделы 20 и 21 остаются
утверждением об атомарности, которое ничем не проверяется, — то есть ровно тем,
чем был `REV3-07` до этого proposal.

Отдельно фиксируется: `STATUS-002` (Presentation Effects) числится в non-goals,
и аварийный путь Commit **не** должен от него зависеть.

---

# 14. Full-field semantics и reset

Screen Document по-прежнему публикует full desired state.

Для каждого property, объявленного schema:

```text
present property
    -> Apply prepared value

absent optional property + schema default
    -> Apply materialized default

absent optional property without default
    -> Reset consumer
```

Widget capabilities, которых **нет в schema**, не затрагиваются.

Пример:

Button умеет:

```text
text
binding
enabled
tooltip
```

Schema содержит:

```text
text
binding
```

Применение этой schema не сбрасывает `enabled/tooltip`, потому что schema ими не
владеет.

### Владение привязано к паре (экземпляр, поле)

У правила есть острый край. Владение свойством определяется парой
`(экземпляр Widget, field_id)`, а не классом виджета. Переиспользуемый
экземпляр, чьё поле получило schema, этим свойством **не** владеющую, обязан
сбросить свойство, а не сохранить прежнее значение.

Иначе переиспользование экземпляров воскрешает ту же проблему устаревшего
состояния, ради которой вводится reset-семантика: экран выглядит применённым
целиком, но часть его состояния принадлежит предыдущей ревизии и никем не
объявлена.

---

# 15. Generic binding traversal

Ручные `PrepareButtonList`, `PrepareModal`, `PrepareTabContainer` больше не
строят binding paths.

Schema walker делает это generic.

## 15.1. Canonical traversal

Property object traversal — canonical property ID order.

Array сохраняет authored order.

Keyed collection path использует key, а не array index.

Пример:

```text
route
main
commands
items
travel_market
binding
```

или normalized compact path:

```text
route / main / commands / travel_market
```

Конкретный final path format фиксируется отдельным contract, но правило одно:

> Binding identity никогда не зависит от display text или position in array.

## 15.2. Interactive arrays обязаны быть keyed

Если schema compiler находит `binding` внутри array subtree без `keyed_by`, UI
schema отклоняется до repository publication.

---

# 16. Schema compilation и repository integration

## 16.1. Build phase

Каждая UI schema:

1. JSON5 parse;
2. common schema structural validation;
3. namespace ownership;
4. kind validation;
5. constraints validation;
6. `schema_ref` resolution;
7. cycle detection;
8. binding input schema validation;
9. compiled immutable schema creation;
10. publication в `GameDataRepository`.

## 16.2. Shared portable validation

Portable validator находится вне UE UI module.

Его используют:

```text
UE
Headless
tests
content validation tools
```

Он проверяет:

- unknown/missing keys;
- scalar type;
- min/max;
- key grammar;
- duplicate keyed items;
- Stable ID kind;
- TextSpec structure;
- BindingSpec structure;
- nested schemas.

Headless не создаёт `FText`, brush, UWidget или BindingHandle.

## 16.3. UE preparation

UE-specific layer добавляет:

- localization/Text preparation;
- Theme lookup;
- Image catalog resolution;
- BindingHandle materialization;
- ScreenRegistry resolution;
- physical Widget capability validation.

---

# 17. Schema <-> Widget compatibility validation

Compatibility должна проверяться **до session Ready**.

## 17.1. Static capability check

Для каждого configured Dynamic Screen Element:

```text
FieldId
SchemaId
Widget class
```

проверяется:

```text
compiled schema root
    ⊆
Widget capability root
```

Ошибка:

```text
SchemaPropertyUnsupported
SchemaPropertyTypeMismatch
SchemaRefKindMismatch
SchemaBindingInputMismatch
SchemaCollectionIdentityMismatch
```

блокирует Screen Registry/application readiness.

## 17.2. Instance wiring check

После создания WBP instance, но до публикации Screen:

- required renderer target существует;
- configured consumer target имеет правильный UMG type;
- required collection host существует;
- required child Widget class resolvable;
- explicit authored policies присутствуют.

Никакого lazy `NewObject` из pure getter.

## 17.3. Schema can be narrower

Разрешено:

```text
Widget:
    text
    binding
    enabled
    tooltip

Schema:
    text
    binding
```

## 17.4. Schema cannot be wider

Запрещено:

```text
Schema:
    text
    binding
    sound

Widget:
    text
    binding
```

Session/Screen validation fails до runtime apply.

---

# 18. Explicit ScalePolicy и закрытие STATUS-003

Image capability validation тесно связана с новым mechanism.

Предлагается одновременно добавить:

```text
EGV2PrimitiveScalePolicy::Unset
```

Default:

```text
Unset
```

Каждый runtime resource consumer обязан иметь explicit authored:

```text
FreeStretch
Tile
NineSlice
PreserveAspect
```

`Unset`:

```text
startup / WBP capability validation failure
```

Таким образом забытый ScalePolicy больше не маскируется default
`PreserveAspect`.

Это закрывает `STATUS-003`.

---

# 19. Predictive preflight и закрытие STATUS-004

Сейчас public `CanApplyScreenFields` не способен предсказать deep child failure.

После перехода:

```text
PrepareDocument
  -> PrepareScreen
    -> PrepareField
      -> PrepareProperty
        -> PrepareCollection
          -> PrepareChild
```

проходит до любого mutation.

Следовательно:

```text
successful Prepare
```

означает, что:

- schema valid;
- renderer targets существуют;
- classes существуют;
- text compatible;
- resources compatible;
- nested screens resolvable;
- all children prepared.

Это становится реальной predictive preflight.

Старый `STATUS-004` удаляется после migration + conformance tests.

---

# 20. Screen-level transaction

`UGV2ScreenWidgetBase` больше не вызывает последовательные fallible
`ApplyScreenField`.

Новая фаза:

```text
PrepareScreenFields(candidate)
```

1. validates complete field set;
2. checks required fields;
3. checks SchemaId;
4. checks schema/capability compatibility;
5. prepares every PropertyHost mutation;
6. prepares resets for removed optional fields;
7. stores complete Screen mutation plan.

Только затем:

```text
CommitScreenFields(plan)
```

Commit не может вернуть обычный schema/resource/child failure.

---

# 21. Document-level transaction

`FGV2LayeredUiReconciler` также переводится на Prepare/Commit.

Это необходимо, потому что Screen atomicity не защищает replacement всего Screen.

## 21.1. Prepare document

Для всех incoming instances:

1. validate layers;
2. validate unique `(layer, instance_key)`;
3. validate layer hosts;
4. resolve Screen classes;
5. reuse matching existing Screen или создать replacement **off-tree**;
6. prepare all Screen fields;
7. prepare modal interactivity plan;
8. prepare attach/detach plan;
9. не detach-ить старые Screens;
10. не менять `ActiveScreens`.

Если любой шаг fails:

```text
live GameShell unchanged
ActiveScreens unchanged
bindings unchanged
```

## 21.2. Commit document

Для new/replacement Screen:

1. fully prepared new Screen already exists;
2. attach new Screen;
3. только после successful attach удалить replaced old Screen;
4. commit reused Screen mutations;
5. apply prepared layer interactivity;
6. atomically replace `ActiveScreens`;
7. commit prepared bindings/revision.

Missing host больше не может привести к invisible-but-successful Screen.

## 21.3. Binding transaction

`PrepareBindings` должен выполнять всю validation.

`CommitPreparedBindings` после successful UI preparation должен быть простой
atomic publication operation без нового validation/failure surface.

---

# 22. Migration всех существующих UI elements

Ниже перечислены все текущие baseline/relevant UI roles и их состояние после
миграции.

## 22.1. `UGV2TextWidgetBase` / `WBP_Text`

Capabilities:

```text
text : Text
```

Consumer:

```text
TextPropertyConsumer -> UGV2TextPipeline
```

`ApplyText(FGV2TextViewModel)` перестаёт быть runtime boundary API.

## 22.2. `UGV2RichTextWidgetBase` / `WBP_RichText`

Capabilities:

```text
text  : Text(rich)
spans : Array keyed by key
```

Span item:

```text
key
span_id
hover:
    title
    description
    image_resource_id
binding
```

Specialized Core RichText consumer использует generic nested prepared values.

## 22.3. `UGV2RichTextPopoverWidgetBase` / `WBP_RichTextPopover`

Transient internal PropertyHost.

Capabilities:

```text
title       : Text
description : Text
image       : StableId(resource)
```

Raw `SNew(STextBlock)` fallback удаляется.

## 22.4. `UGV2ImageWidgetBase` / `WBP_Image`

Capabilities:

```text
resource_id : StableId(resource)
```

Не boundary properties:

```text
scaling_policy
custom_width
custom_height
render_mode
brush
texture
```

ScalePolicy остаётся authored UE configuration.

## 22.5. `UGV2IconWidgetBase`

Specialization Image capability:

```text
resource_id : StableId(resource)
```

## 22.6. `UGV2ButtonWidgetBase` / `WBP_Button`

Capabilities:

```text
text    : Text(plain)
binding : Binding(no-input)
```

`void ApplyButtonModel(...)` удаляется как primary runtime path.

## 22.7. `UGV2CheckboxWidgetBase` / `WBP_Checkbox`

Capabilities:

```text
text       : Text(plain)
is_checked : Boolean
binding    : Binding(checkbox input contract)
```

## 22.8. `UGV2InputFieldWidgetBase` / `WBP_InputField`

Capabilities:

```text
text             : Text(plain) optional
placeholder_text : Text(plain) optional
value            : String
is_read_only     : Boolean
max_length       : Integer
binding          : Binding(input value contract)
```

Placeholder и label используют общий TextPipeline.

## 22.9. `UGV2DropdownSelectWidgetBase` / `WBP_DropdownSelect`

Capabilities:

```text
placeholder  : Text
selected_key : Key optional
items        : keyed collection
binding      : Binding(dropdown selected input contract)
```

Options создаются как Button PropertyHosts.

## 22.10. `UGV2ButtonListWidgetBase` / `WBP_ButtonList`

Capabilities:

```text
items : keyed collection<Button>
```

Использует generic `KeyedCollectionPropertyConsumer`.

## 22.11. `UGV2ProgressBarWidgetBase` / `WBP_ProgressBar`

Capabilities:

```text
percent : Number [0..1]
label   : Text optional
```

`label` больше не может потеряться между composite и leaf.

## 22.12. `UGV2PortraitWidgetBase` / `WBP_Portrait`

Capabilities:

```text
resource_id       : StableId(resource)
frame_resource_id : StableId(resource) optional
```

Missing renderer + present resource -> Prepare failure.

## 22.13. `UGV2ModalWidgetBase` / `WBP_Modal`

Capabilities:

```text
title                 : Text
content               : Text
buttons               : keyed collection<Button>
backdrop_close_action : Binding optional
```

Все properties имеют обязательные consumers.

## 22.14. `UGV2TabContainerWidgetBase` / `WBP_TabContainer`

Capabilities:

```text
default_tab_key : Key optional
tabs            : keyed collection<Tab>
```

Tab:

```text
key
title
screen_id
fields
```

`screen_id` разрешается только через Screen Registry.

## 22.15. `UGV2SeparatorWidgetBase` / `WBP_Separator`

Pure UE-local theme visual.

Runtime Screen Field schema не требуется.

## 22.16. `UGV2LoadingIndicatorWidgetBase` / `WBP_LoadingIndicator`

UE-local operation state/theme component.

## 22.17. Panel / ScrollArea

Structural Widgets.

Layout остаётся UE-owned.

## 22.18. ListView / Repeater

`FGV2KeyedCollection` остаётся structural helper.

`KeyedCollectionPropertyConsumer` добавляет deep Prepare и transactionality.

## 22.19. `UGV2ScreenWidgetBase` / `WBP_ScreenBase`

Становится generic aggregator:

```text
field envelope
    ↓
compiled data schema
    ↓
prepared property object
    ↓
Dynamic Element PropertyHost
```

## 22.20. `UGV2GameShellWidgetBase` / `WBP_GameShell`

Не становится Lua property host.

Document transaction проверяет layer hosts и attach result до publication.

## 22.21. `UGV2LocationTopBarWidgetBase`

Capabilities:

```text
day              : Text
location         : Text
primary_resource : Text
```

## 22.22. `UGV2LocationPlayerStatusWidgetBase`

Capabilities:

```text
name                 : Text
portrait_resource_id : StableId(resource)
meters               : keyed collection<ProgressBar>
items                : keyed collection<Image/Icon>
effects              : keyed collection<Image/Icon>
```

Meter:

```text
key
percent
label
```

## 22.23. `UGV2LocationSceneWidgetBase`

Capabilities:

```text
background_tile_resource_id : StableId(resource) optional
background_resource_id      : StableId(resource) optional
context_text                : Text optional
characters                  : keyed collection<Image>
```

## 22.24. `UGV2LocationCommandPanelWidgetBase`

Capabilities:

```text
items : keyed collection<Button>
```

Использует Core ButtonList schema/consumer.

## 22.25. `UGV2DebugStartScreenWidget`

Retired development-only fixture.

Не является причиной сохранять legacy DTO.

---

# 23. Что удаляется из `GV2BridgeTypes`

После полной миграции boundary DTO удаляются:

```text
FGV2ButtonViewModel
FGV2CheckboxViewModel
FGV2InputFieldViewModel
FGV2DropdownOptionViewModel
FGV2DropdownSelectViewModel
FGV2InteractiveRichTextViewModel
FGV2RichTextSpanViewModel
FGV2ProgressBarViewModel
FGV2PortraitViewModel
FGV2ModalViewModel
FGV2LocationTopBarViewModel
FGV2LocationIconEntry
FGV2LocationCharacterEntry
FGV2LocationMeterEntry
FGV2LocationPlayerStatusViewModel
FGV2LocationSceneViewModel
FGV2TabItemViewModel
FGV2TabContainerViewModel
FGV2ScreenFieldValue
```

Обязательно остаются:

```text
FGV2TextViewModel
FGV2UiBindingHandle
FGV2UiControlValue
prepared generic UI value types
document/screen identity types
```

---

# 24. Что удаляется из C++ application layer

После cutover удаляется:

```text
FGV2ScreenFieldAdapterRegistry
```

и все schema-specific:

```text
PrepareXxx
BuildXxx
MakeXxx
```

Их заменяет один recursive schema walker/preparer.

---

# 25. Legacy public apply APIs

Следующие методы после migration не должны оставаться production authority:

```text
ApplyButtonModel
ApplyCheckboxModel
ApplyInputFieldModel
ApplyDropdownModel
ApplyButtonModels
ApplyProgress
ApplyPortrait
ApplyModal...
```

Допускается временный thin wrapper во время migration, если он делегирует
generic PropertyHost и не содержит отдельной rendering logic.

Permanent dual-stack запрещён.

---

# 26. Migration strategy без permanent dual-stack

## 26.0. Обратная совместимость не поддерживается

Проект не поддерживает обратную совместимость: нет внешних потребителей, нет
сохранённых сессий, переживающих смену формата presentation, и нет published
schema, которую нельзя переписать вместе с кодом. Это меняет стратегию
миграции в трёх местах, и не воспользоваться этим было бы ошибкой.

**Виджет мигрирует вместе со своим контентом и своим адаптером удаляется в том
же change set.** Не нужно поддерживать оба пути для одного элемента: как только
`WBP_Button` переведён на property host, `PrepareButton`/`BuildButton`,
`FGV2ButtonViewModel` и соответствующая ветка `FGV2ScreenFieldValue` удаляются
немедленно, а Lua-форма правится тем же коммитом. Двойной стек существует
только **между** мигрированными и не мигрированными элементами, а не внутри
одного элемента.

**Раздел 27.1 перестаёт быть ограничением.** Требование «не менять shapes без
необходимости» имело смысл как защита совместимости. Без неё форма значения
приводится к целевой сразу, а не переносится как есть с обещанием
нормализовать позже. Перенос кривой формы «чтобы не ломать» — это способ
получить третий раунд findings об идентичности и потерянных ключах.

**Фаза 7 перестаёт быть cutover.** Вместо одномоментного переключения удаление
идёт непрерывно: каждая мигрированная поверхность уменьшает legacy на свою
величину. К фазе 7 остаются только общие каркасы (`FGV2ScreenFieldAdapterRegistry`
как контейнер, union `FGV2ScreenFieldValue`), а не тринадцать живых веток.

### Гейт монотонного убывания

Чтобы «непрерывное удаление» не осталось намерением, вводится измеримый гейт:

```text
тест считает:
    число schema-specific Prepare/Build функций
    число payload-членов FGV2ScreenFieldValue
    число schema-specific DTO в GV2BridgeTypes.h

каждое число зафиксировано в тесте как верхняя граница
граница обязана уменьшаться и не может расти
```

Попытка добавить новый schema-specific адаптер после фазы 2 краснит сборку.
Это то же средство, что и раздел 31.14 (no legacy adapter gate), но действующее
**во время** миграции, а не после неё.

### Что дополнительно становится удаляемым

Отсутствие совместимости открывает удаление, которое иначе пришлось бы
откладывать:

- парные `CanApplyXxx` / `ApplyXxx` / `CaptureXxx` на каждом виджете —
  заменяются одним lifecycle property host;
- варианты `ApplyOptionalXxx` (`ApplyOptionalImageResource`,
  `ApplyOptionalPortrait`) — политика подстановки заглушки становится свойством
  schema, а не второй перегрузкой C++;
- `BindWidgetOptional` там, где свойство объявлено обязательным schema:
  обязательность перестаёт быть договорённостью и становится проверкой
  экземпляра (раздел 17.2);
- остатки поверхности совместимости, помеченные `DeprecatedProperty`.

## Phase 0 — ADR/contract freeze

Зафиксировать:

- proposal;
- новый ADR;
- data schema ownership;
- standard kinds;
- mod restrictions;
- Prepare/Commit semantics.

## Phase 1 — schema infrastructure

Добавить:

- `schema_domain: ui_field/ui_value`;
- UI schema compiler;
- standard kinds;
- schema_ref;
- keyed arrays;
- `screen_fields`;
- shared portable validator;
- repository indexes по UI schema ID.

## Phase 2 — prepared value + property host

Добавить:

```text
FGV2PreparedUiValue
FGV2PreparedUiObject
IGV2UiPropertyHost
FGV2UiPropertyHostState
standard property consumers
mutation plans
```

## Phase 3 — baseline leaf migration

Порядок:

1. Text
2. Image/Icon
3. Button
4. Checkbox
5. InputField
6. ProgressBar
7. Portrait
8. RichText

## Phase 3a — гейт go/no-go

**Обязательная остановка.** Text, Image и Button — это proving slice: три
элемента, покрывающие текст, ресурс и binding, то есть все три
стандартизированных Core pipeline.

Продолжение фазой 4 разрешено, только если на этом срезе выполнено всё:

1. Тест наблюдаемости capability (31.3a) написан и **краснеет**, если consumer
   заменить на no-op или отвязать renderer target в ассете.
2. Тест распространения отказа краснеет, если parent начинает игнорировать
   отказ ребёнка.
3. Тест чистоты Prepare (31.5) краснеет, если в Prepare внести мутацию live
   состояния.
4. Инъекция отказа Commit (13.3a) даёт описанное наблюдаемое поведение.
5. Три удалённых адаптера действительно удалены, гейт монотонного убывания
   зелёный.

Если хотя бы один пункт не выполняется — миграция останавливается и
пересматривается, а не продолжается по инерции. Причина прямая: тремя
элементами доказывается, что новая архитектура **способна** производить
проверки, которых не хватало старой. Если на трёх элементах их написать не
удалось, на двадцати пяти не удастся тем более, и результатом станет тот же
класс дефектов внутри более сложной машинерии.

Оценку выполнения пунктов 1–3 делать по правилу `AGENTS.md`: не «тест
существует и зелёный», а «тест краснеет на внесённой регрессии».

## Phase 4 — collection/composite migration

Порядок:

1. KeyedCollection consumer
2. ButtonList
3. Dropdown
4. RichText spans/popover
5. Modal
6. TabContainer

## Phase 5 — LocationScreen

Порядок:

1. TopBar
2. PlayerStatus
3. Scene
4. CommandPanel

## Phase 6 — Screen/Document transaction

Перевести:

```text
UGV2ScreenWidgetBase
FGV2LayeredUiReconciler
SessionCoordinator presentation prepare
BindingRegistry prepared commit
```

на document Prepare/Commit.

## Phase 7 — снятие каркаса

Не cutover: к этому моменту schema-specific ветки уже удалены поэлементно
(раздел 26.0). Остаётся снять общие каркасы:

```text
FGV2ScreenFieldAdapterRegistry
FGV2ScreenFieldValue union payload
остаточные schema-specific DTOs
legacy fallible Apply paths
```

Гейт монотонного убывания в этой фазе доводит все три счётчика до нуля и
превращается в постоянный запрет (раздел 31.14).

## Phase 8 — docs/status cleanup

Обновить:

```text
Docs/UI/WidgetRegistry.md
Docs/UI/ScreenTemplates.md
Docs/UI/UIDocumentAndReconciliation.md
Docs/UI/ImageResources.md
Docs/Architecture/Invariants.md
Docs/Status/ImplementationStatus.md
```

Archive plans не переписывать.

---

# 27. Lua/content migration policy

## 27.1. Не менять shapes без необходимости

Архитектурная смена C++ transport сама по себе не требует изменять Lua value
shape.

## 27.2. Удалить accidental accepted properties

Image:

```text
scaling_policy
custom_width
custom_height
```

не являются Lua presentation data.

## 27.3. Реализовать advertised properties

Input:

```text
is_read_only
max_length
```

становятся настоящими properties.

## 27.4. Identity

Все Widget collections используют explicit `key`.

No positional fallback.

Resource ID не является identity для semantic entity.

---

# 28. Полное закрытие review defects

> **Живые дефекты исправлены отдельно и до миграции.** `REV3-01`, `REV3-02`,
> `REV3-03`, `REV3-04`, `REV3-05`, `REV3-06`, `REV3-08`, `REV3-09` и `REV3-10`
> — это баги на несколько строк каждый, и держать их сломанными на всё время
> восьмифазной миграции нельзя. Они устранены точечно; ниже описано, что
> закрывает их **как класс**, то есть делает структурно невозможными.
>
> Это разделение принципиально. Точечная починка убирает экземпляр и ничего не
> говорит о рецидиве: `meters[].label` был третьим экземпляром одного семейства
> за три раунда, после того как два предыдущих экземпляра были «закрыты».
> Ценность proposal — не в списке ниже, а в том, что список перестаёт
> пополняться.
>
> `REV3-07` (атомарность `FGV2LayeredUiReconciler`) точечно не исправлялся: он
> требует document-level Prepare/Commit и закрывается фазой 6.

## REV3-01 — `meters[].label` теряется

Closure:

Meter child получает полный generic object:

```text
percent
label
```

через ProgressBar PropertyHost.

## REV3-02 — ProgressBar label обходит Text Pipeline

Closure:

`label` consumer — только `TextPropertyConsumer`.

## REV3-03 — accepted-but-ignored schema keys

Closure:

Нет `ConsumedKeys`.

Нет `BuildXxx`.

Каждый schema property materializes prepared value и имеет consumer.

Specific:

- `input.is_read_only` implemented;
- `input.max_length` implemented;
- `image.scaling_policy/custom_*` rejected;
- `progress.style` rejected;
- `portrait.style` rejected.

## REV3-04 — Modal partial consumption

Closure:

Modal capabilities:

```text
title
content
buttons
backdrop_close_action
```

Каждый property имеет consumer.

## REV3-05 — ButtonList/CommandPanel игнорируют Text failure

Closure:

Button Text consumer выполняет fallible renderer preparation до collection
Commit.

## REV3-06 — TabContainer игнорирует ScreenId/child failure

Closure:

Nested Screen обрабатывает `ScreenPropertyConsumer`.

`screen_id` разрешается Screen Registry.

## REV3-07 — LayeredUiReconciler не атомарен

Closure:

Document Prepare/Commit.

Old Screen не detach до fully prepared replacement.

## REV3-08 — InputField typography/property loss

Closure:

`placeholder_text` и label — Text consumers.

`is_read_only/max_length` — primitive consumers.

## REV3-09 — RichText tooltip silent raw fallback

Closure:

Missing required Popover renderer -> Prepare failure.

Raw fallback удалён.

## REV3-10 — Portrait success при missing renderer

Closure:

Resource consumer Prepare требует target renderer.

---

# 29. Закрытие более ранних UI findings

## Reused Widget state atomicity

`KeyedCollectionPropertyConsumer` готовит child mutations до live mutation.

## Actual font consumer verification

Сохраняется actual renderer test и расширяется на все Text consumers.

## Predictive deep preflight

Закрывается recursive Prepare tree и удаляет `STATUS-004`.

## BlueprintPure lazy mutation

Capability getters read-only.

Lazy `NewObject` из pure getter запрещён.

## Missing child Widget class silent success

Child class resolution — Prepare requirement.

## Items/effects boundary loss

Нет separate boundary adapter, способного забыть collection.

## Repeated-key grammar drift

Один `kind: key` validator используется всеми UI schemas.

## Character identity from resource

`key` и `resource_id` остаются разными properties.

## 720p/full geometry regression

Существующий viewport matrix остаётся mandatory gate.

## Legacy `Character` / `StaminaMeter`

После Location migration obsolete single-widget properties удаляются.

---

# 30. STATUS-003 / STATUS-004

После полного внедрения proposal:

```text
STATUS-003
```

удаляется благодаря `ScalePolicy::Unset` + startup capability validation.

```text
STATUS-004
```

удаляется благодаря recursive target-specific Prepare.

`STATUS-001` и `STATUS-002` остаются.

---

# 31. Verification strategy

## 31.1. Portable schema compiler tests

Для каждого standard kind:

- valid;
- wrong type;
- missing required;
- unknown property;
- default;
- min/max;
- invalid Stable ID kind;
- invalid TextSpec;
- invalid BindingSpec;
- invalid key;
- duplicate keyed collection entry.

## 31.2. Mod schema tests

Accept:

```text
mod namespace
standard kinds only
valid schema_ref
compatible constraints
```

Reject:

```text
core namespace from mod
unknown kind
open object
unknown renderer token
raw UE locator
cycle schema_ref
new binding semantic type
```

## 31.3. Exact materialization test

Для candidate schema object:

```text
number of schema-present properties
==
number of prepared properties
```

с учётом defaults/reset semantics.

**Этого недостаточно, и одного этого теста быть не должно.** Счётное равенство
доказывает, что фаза Prepare всё потребила, и ничего не говорит о том, что
Commit что-то изменил. Это та же форма «тест доказывает более слабое свойство,
чем заявляет», которая перечислена в `AGENTS.md` как defect при review.

## 31.3a. Capability observability test

Обязательный парный тест, закрывающий раздел 10.2a. Для каждого мигрированного
класса виджета перебрать **все** объявленные capability и для каждой проверить:

```text
Commit(A)  -> S1 = Capture(widget)
Commit(B)  -> S2 = Capture(widget)
S1 != S2
```

где `A` и `B` различаются только этим property.

Тест обязан краснеть, если consumer заменить на no-op, если renderer target
отвязать в ассете, или если capability объявить без реализации. Отсутствие пары
`A/B`, дающей различимое состояние, означает, что capability объявлена ложно, и
является ошибкой сборки, а не пропуском теста.

Этот тест — единственная проверка во всём наборе, которая делает объявление
capability неподделываемым. Все остальные проверки раздела 31 подтверждают
внутреннюю согласованность pipeline, а не факт отображения.

## 31.4. Schema/capability compatibility tests

Проверить:

```text
schema subset accepted
extra widget capability accepted
extra schema property rejected
kind mismatch rejected
ref kind mismatch rejected
binding input mismatch rejected
collection item mismatch rejected
```

## 31.5. Prepare purity

Для каждого migrated Widget:

```text
Prepare(valid)
```

не меняет physical Widget.

```text
Prepare(invalid)
```

не меняет physical Widget.

## 31.6. Commit tests

После successful Prepare:

```text
Commit
```

даёт ожидаемое physical state.

## 31.7. Collection transaction

Baseline:

```text
A old
B old
```

Candidate:

```text
A new valid
B invalid
```

Expected:

```text
Prepare false
A old
B old
same pointers
same order
same child count
```

## 31.8. Modal end-to-end

Проверить:

- title;
- content;
- buttons;
- backdrop click;
- binding submission;
- no raw text fallback.

## 31.9. Input end-to-end

Проверить:

- label;
- placeholder;
- current value;
- read-only;
- max length;
- binding input;
- actual DPI font на 720/1080/1440/2160.

## 31.10. Tab end-to-end

Проверить actual Screen Registry class, child fields, reuse, replacement и
failure atomicity.

## 31.11. Document transaction tests

Проверить:

- missing host;
- replacement factory fail;
- second-screen failure;
- binding commit only after UI commit.

## 31.12. Text pipeline source audit

Запретить direct runtime-authored `SetText(...)` вне approved pipeline.

## 31.13. Image pipeline source audit

Запретить raw brush mutation вне ImagePresentation.

## 31.14. No legacy adapter gate

После final cutover запрещены:

```text
FGV2ScreenFieldAdapterRegistry
PrepareLocation...
BuildLocation...
MakeLocation...
schema-specific FGV2ScreenFieldValue payload members
```

---

# 32. Diagnostics

Ошибки имеют structured path:

```text
screen_id
instance_key
field_id
schema_id
property_path
widget_class
capability
code
message
```

Пример:

```text
UiPropertyUnsupported
screen=textsystem:screen.location
field=player_status
schema=textsystem:schema.ui_field.location_player_status.v2
path=meters[stamina].label
widget=WBP_ProgressBar
```

---

# 33. Performance

1. UI schemas компилируются один раз при repository build.
2. Widget capability descriptor строится один раз и cache-ится.
3. Runtime не делает UPROPERTY reflection lookup по string.
4. Prepared candidate immutable.
5. Collections продолжают reuse children по semantic key.

---

# 34. Security/trust boundary

Lua/mod boundary видит:

```text
standard values
schema IDs
screen IDs
resource IDs
TextSpec
BindingSpec
```

Не видит:

```text
UClass
UObject
Widget path
raw asset path
FSlateBrush
font class
texture
renderer class
delegate
Lua closure pointer
```

---

# 35. Compatibility

## 35.1. Schema ID означает data contract

`schema_id` больше не означает C++ adapter.

Он означает:

```text
compiled data shape + semantic kinds + constraints
```

## 35.2. Implementation refactor не требует version bump

Если portable shape и semantics не меняются, перенос на generic engine сам по
себе не требует нового schema ID.

## 35.3. Shape/semantic change

При изменении requiredness/type/identity semantics используется новый versioned
schema ID.

---

# 36. Документация после migration

Обновить:

```text
WidgetRegistry.md
ScreenTemplates.md
UIDocumentAndReconciliation.md
ImageResources.md
Architecture/Invariants.md
ImplementationStatus.md
```

Удалить из документации fixed C++ schema adapter mapping и schema-specific DTO
как нормативный путь.

---

# 37. Acceptance criteria / Definition of Done

## Architecture

- [ ] Все production UI Screen Fields используют data-driven schemas.
- [ ] Нет fixed C++ `schema_id -> PrepareFoo/BuildFoo` registry.
- [ ] `FGV2ScreenFieldValue` schema-specific union удалён.
- [ ] Production field payload представлен generic prepared property tree.
- [ ] Каждый runtime-controlled Widget использует `IGV2UiPropertyHost`.
- [ ] Каждый schema property имеет compatible registered consumer.
- [ ] Extra schema property блокирует startup/apply.
- [ ] Extra Widget capability разрешена.
- [ ] Mods могут объявлять own schemas только standard Core kinds.
- [ ] Mods не могут регистрировать new kind/renderer/binding semantics.

## Pipeline

- [ ] Shared portable schema validator работает в UE и Headless.
- [ ] Text только через TextPipeline.
- [ ] Images/resources только через ImagePresentation.
- [ ] Bindings только через opaque handles.
- [ ] Widget identity никогда не выводится из array position/display
      text/resource при semantic entity identity.
- [ ] Repeated collections используют `keyed_by`.

## Transactionality

- [ ] Field Prepare не мутирует live UI.
- [ ] Collection Prepare не мутирует reused children.
- [ ] Screen Prepare не мутирует live Screen.
- [ ] Document Prepare не detach/attach live Screens.
- [ ] Commit не выполняет normal fallible validation/loading.
- [ ] Failed candidate оставляет previous physical UI и bindings неизменными.

## Existing UI

- [ ] Text migrated.
- [ ] RichText migrated.
- [ ] RichTextPopover migrated.
- [ ] Image/Icon migrated.
- [ ] Button migrated.
- [ ] Checkbox migrated.
- [ ] InputField migrated.
- [ ] Dropdown migrated.
- [ ] ButtonList migrated.
- [ ] ProgressBar migrated.
- [ ] Portrait migrated.
- [ ] Modal migrated.
- [ ] TabContainer migrated.
- [ ] LocationTopBar migrated.
- [ ] LocationPlayerStatus migrated.
- [ ] LocationScene migrated.
- [ ] LocationCommandPanel migrated.
- [ ] Screen aggregate migrated.
- [ ] Document reconciler migrated.
- [ ] Structural/style-only widgets explicitly documented as no runtime property
      host required.

## Defects

- [ ] REV3-01 closed.
- [ ] REV3-02 closed.
- [ ] REV3-03 closed.
- [ ] REV3-04 closed.
- [ ] REV3-05 closed.
- [ ] REV3-06 closed.
- [ ] REV3-07 closed.
- [ ] REV3-08 closed.
- [ ] REV3-09 closed.
- [ ] REV3-10 closed.
- [ ] Reused collection child-state atomicity regression protected.
- [ ] Missing renderer/class false-success regression protected.
- [ ] Generic key grammar regression protected.
- [ ] Character/resource identity separation protected.
- [ ] Item/effect reorder/resource-change identity protected.
- [ ] STATUS-003 removed after evidence.
- [ ] STATUS-004 removed after evidence.

## Verification

- [ ] All portable tests green.
- [ ] All UE automation tests green.
- [ ] Headless self-test/check-scripts green.
- [ ] docs validation green.
- [ ] clean GV2Editor build green.
- [ ] source audit confirms no legacy adapters and no parallel runtime pipelines.

---

# 38. Риски и trade-offs

## 38.1. Generic tree менее удобен при ручном C++ coding

Schema-specific DTO удобнее autocomplete.

Компенсация:

- Widget не читает tree вручную;
- standard consumers типизированы;
- capability builder типизирован;
- diagnostics показывают property path.

## 38.2. Property capability description дублирует часть schema

Это намеренно.

Schema отвечает:

```text
что data contract разрешает передать
```

Widget capability отвечает:

```text
что physical Widget способен отобразить
```

Независимость нужна для проверки:

```text
SchemaContract ⊆ WidgetCapabilities
```

## 38.3. Data schema может существовать без compatible Widget

Допустимо на repository уровне.

Ошибка возникает, только когда schema реально связывается с physical field.

## 38.4. Prepare/Commit сложнее простого setter

Эта сложность реализуется один раз в Core infrastructure и заменяет десятки
локальных rollback/preflight implementations.

---

# 39. Почему не reflection UPROPERTY binding

Не выбран вариант:

```text
property name
    ↓
FindPropertyByName
    ↓
automatic UPROPERTY assignment
```

Причины:

- rename C++ member становится protocol change;
- Blueprint internals становятся public Lua contract;
- сложно гарантировать Text/Image pipelines;
- трудно контролировать mod permissions;
- невозможно выразить target-specific preflight;
- ухудшается diagnostics.

---

# 40. Почему не raw FValue до Widget

Не выбран вариант:

```text
Lua FValue
    ↓
Widget сам разбирает object
```

Потому что это повторяет текущую проблему внутри Widgets.

Schema/preparation должны завершаться до Widget.

Widget получает только prepared semantic values через registered consumers.

---

# 41. Resulting architecture

```text
Lua canonical presentation source
        │
        │ portable FValue
        ▼
FUiDocument
        │
        ▼
GameDataRepository
compiled data-driven UI schemas
        │
        ▼
Portable UiSchemaValidator
        │
        ├── closed objects
        ├── types/ranges
        ├── key grammar
        ├── keyed uniqueness
        ├── Stable ID kinds
        ├── TextSpec validation
        └── BindingSpec validation
        │
        ▼
UE Generic UiPreparer
        │
        ├── TextSpec -> FGV2TextViewModel
        ├── BindingSpec -> BindingHandle
        ├── StableId -> validated semantic ID
        └── recursive prepared object/array
        │
        ▼
FGV2PreparedUiDocument
        │
        ▼
Document Prepare
        │
        ├── Screen Registry
        ├── layer hosts
        ├── Screen Field schema/capability
        └── all Widget mutation plans
        │
        ▼
IGV2UiPropertyHost
        │
        ├── TextPropertyConsumer
        ├── ImageResourcePropertyConsumer
        ├── PrimitivePropertyConsumers
        ├── BindingPropertyConsumer
        ├── KeyedCollectionPropertyConsumer
        └── ScreenPropertyConsumer
        │
        ▼
infallible-style Commit
        │
        ▼
physical UE presentation
```

Gameplay authority не меняется:

```text
Lua
    owns gameplay state and decisions

GameDataRepository
    owns immutable static definitions + compiled schemas

UE
    owns physical presentation and platform/rendering details
```

---

# 42. Итоговая рекомендация

Не исправлять REV3 defects как независимые локальные patches поверх текущего
schema-specific pipeline.

Правильный corrective direction:

```text
1. ввести data-driven UI schemas;
2. ввести generic prepared property tree;
3. ввести Widget capabilities + standard consumers;
4. перевести lifecycle на Prepare -> Commit;
5. мигрировать baseline leaves;
6. мигрировать composites/collections;
7. мигрировать LocationScreen;
8. сделать Document transaction;
9. удалить old adapters/DTO;
10. только после полного cutover закрыть review findings/status gaps.
```

Главный архитектурный инвариант после перехода:

> **Ни одно значение, разрешённое UI schema, не может быть успешно принято,
> если оно не было подготовлено, не имеет compatible physical consumer или не
> дошло до этого consumer.**

И второй:

> **Ни один fallible presentation operation не выполняется после начала commit
> live UI.**

Именно эти два правила устраняют общий root cause почти всех обнаруженных в
последних review UI defects.
