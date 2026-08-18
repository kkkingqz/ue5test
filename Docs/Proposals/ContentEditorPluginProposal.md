---
title: Content Editor Plugin Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-08-18
depends_on:
  - DesignerLuaAuthoringProposal.md
  - ../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../Architecture/GameDataRepositoryContract.md
  - ../Architecture/BuildAndTooling.md
  - ../UI/ScreenTemplates.md
decisions:
  - ../ADR/0018-portable-content-core-module.md
  - ../ADR/0026-core-and-gameplay-ownership.md
---

# Предложение по редактору контента в Unreal Editor

> **Предлагает:** плагин Unreal Editor как визуальный frontend поверх канонических `.json5`, без переноса данных в `.uasset`.
> **Затрагивает:** [Definition Envelope](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [Build and Tooling](../Architecture/BuildAndTooling.md).
> **Не является нормативным:** до реализации действует текущий contract.

Плагин — один из frontend авторского слоя наравне с `gv2-content` и правкой файлов вручную. Lua-часть авторского слоя — в [Designer Lua Authoring](DesignerLuaAuthoringProposal.md).

## 1. Цель

Предоставить разработчику и gameplay designer встроенную в Unreal Editor среду для работы с:

```text
Actors
Items
Locations
Schemas
References
Resources
Text
Scenarios
другими gameplay definitions
```

без необходимости вручную редактировать JSON5 в типовых случаях.

## 2. Главный принцип

> **Plugin редактирует GV2 data, но не превращает GV2 data в Unreal assets.**

Source of truth остаётся:

```text
GameData/<package>/...
```

Plugin не должен требовать создания `.uasset` для gameplay definitions.

## 3. Общая архитектура

```text
GameData JSON5
      ↓
GV2 Content Core / Authoring API
      ↓
GV2 Editor Model
      ↓
FInstancedPropertyBag
      ↓
PropertyEditor / DetailsView
      ↓
Unreal Editor UI
```

При сохранении:

```text
Details UI
      ↓
PropertyBag
      ↓
GV2 Authoring Model
      ↓
JSON5 serializer
      ↓
GameData
      ↓
GV2 validation
```

## 4. Почему `FInstancedPropertyBag`

Не рекомендуется генерировать отдельный `USTRUCT` для каждого gameplay schema.

Например нежелательно:

```cpp
USTRUCT()
struct FRHActor
{
    UPROPERTY()
    int32 HP;

    UPROPERTY()
    int32 Gold;
};
```

потому что изменение gameplay schema потребует изменения C++.

Вместо этого plugin должен строить dynamic property model из GV2 schema.

Пример:

```text
GV2 Actor Schema

runtime_type → enum
hp           → integer
gold         → integer
home         → ref<location>

            ↓

FInstancedPropertyBag

            ↓

UE Details Panel
```

## 5. Основной Editor Tab

Plugin должен регистрировать отдельную вкладку Unreal Editor:

```text
GV2 Content
```

Предлагаемая структура:

```text
┌──────────────────────────────────────────────────────────────┐
│ GV2 Content                                                  │
├────────────────┬────────────────────────────┬────────────────┤
│ Definitions    │ Properties                 │ References     │
│                │                            │                │
│ Actors         │ Actor: Aria                │ Used by        │
│  Aria          │                            │ quest.main     │
│  Cassia        │ Type     NPC               │ tavern_intro   │
│  Marcus        │ HP       100               │                │
│                │ Gold     50                │                │
│ Items          │ Home     Tavern            │                │
│  Sword         │ Portrait AriaPortrait      │                │
│                │                            │                │
│ Locations      │ [+ Property]               │                │
│  Tavern        │ [+ Scenario]               │                │
├────────────────┴────────────────────────────┴────────────────┤
│ + New   Duplicate   Delete      Validate      Save           │
└──────────────────────────────────────────────────────────────┘
```

## 6. Definition Browser

Левая панель показывает definitions по kind.

Например:

```text
Actors
├── Aria
├── Cassia
└── Marcus

Items
├── Sword
└── Bread

Locations
├── Tavern
└── Market
```

Browser должен получать данные через GV2 Content tooling, а не напрямую интерпретировать package overlay independently.

## 7. Definition Selection

При выборе definition plugin:

1. определяет Stable ID;
2. получает canonical/authoring data;
3. определяет применимую schema;
4. создаёт dynamic PropertyBag;
5. показывает его через DetailsView.

## 8. Property Mapping

Минимальное отображение:

```text
GV2 type       UE property

integer     → int
number      → float/double
boolean     → bool
string      → FString
enum        → enum-like selector
array       → array
object      → nested property group
```

GV2-specific types получают custom editor widgets.

## 9. Custom GV2 Property Widgets

Минимальный набор custom controls:

```text
Definition Reference Picker
Resource Picker
Text Picker
Stable ID Viewer
Package Reference Picker
```

## 10. Definition Reference Picker

Поле:

```text
home : ref<location>
```

не должно отображаться как обычный string.

Вместо:

```text
rh:location.tavern
```

показывается:

```text
Home Location
[Tavern ▼] [Find]
```

Dropdown получает список compatible definitions через GV2 tooling.

## 11. Resource Picker

Для resource reference:

```text
Portrait
[AriaPortrait] [Browse...] [Preview]
```

Plugin может использовать UE Asset Browser только как chooser physical presentation asset, если это соответствует Resource contract.

В gameplay data сохраняется GV2 Resource ID, а не physical asset path, если runtime contract требует именно этого.

## 12. Text Picker

Для text references:

```text
Name
[Aria]
[Edit Text...]
```

Plugin может открывать отдельный text/localization editor.

Однако text workflow реализуется отдельным этапом и не должен менять existing Text contract.

## 13. Categories и Layout

Schema authoring metadata может задавать:

```text
category
display name
order
description
editor hint
```

Например:

```text
General
  Runtime Type
  Name

Gameplay
  HP
  Gold

World
  Home Location

Presentation
  Portrait
```

DetailsView использует metadata для группировки.

## 14. Create Definition

Кнопка:

```text
+ New
```

открывает selection:

```text
Actor
Item
Location
Scenario
...
```

После выбора:

```text
Package: rh
Local ID: aria
```

Plugin создаёт корректный authoring file:

```text
GameData/rh/actors/aria.json5
```

и заполняет defaults из schema.

## 15. Duplicate Definition

`Duplicate` должен:

1. предложить новый local ID;
2. создать новый definition;
3. скопировать допустимые authoring values;
4. не копировать Stable ID буквально;
5. выполнить validation.

## 16. Delete Definition

Удаление должно проверять references.

Если definition используется:

```text
rh:actor.aria

Used by:
rh:quest.main
rh:scenario.tavern_intro
```

Plugin должен показать impact и следовать существующему removal/reference contract.

Silent delete с dangling references запрещён.

## 17. Find Usages

Правая панель:

```text
References / Used by
```

показывает входящие ссылки.

Например:

```text
rh:quest.main
rh:scenario.tavern_intro
rh:location.tavern
```

Источник данных — existing Content Core/reference index.

## 18. Add Property

Для schema-backed entity должна быть доступна:

```text
+ Add Property
```

Это **Schema Authoring operation**, а не вставка произвольного JSON key.

## 19. Add Property Dialog

Пример:

```text
Add Property

Name:        reputation
Type:        Integer
Required:    No
Default:     0
Category:    Social

[Add]
```

После подтверждения:

```text
Gameplay Schema
      ↓
new field
      ↓
PropertyBag rebuilt
      ↓
DetailsView updated
```

## 20. Scope Property

Если architecture допускает несколько schema layers, UI может спрашивать:

```text
Apply to:

Actor
NPC
Specific extension
```

На v1 следует показывать только реально поддерживаемые scopes.

Не следует вводить arbitrary per-definition properties.

## 21. Property Operations

Context menu property может содержать:

```text
Edit Schema Field
Rename
Set Default
Change Category
Find Usages
Remove
```

Операции должны проходить через Schema Authoring API.

## 22. Schema Change Validation

Изменение schema может влиять на множество definitions.

Например изменение:

```text
reputation: integer
```

на:

```text
reputation: enum
```

должно вызвать impact analysis.

Plugin должен показать:

```text
Affected definitions: 17
Invalid after change: 3
```

и не silently ломать package.

## 23. Create Scenario/Event

Для Actor/NPC UI может показывать:

```text
Scenarios

First Meeting      tavern_intro
Recruited          —
Death              —

[+ Create Scenario]
[Link Existing]
```

## 24. Create Scenario Dialog

Пример:

```text
Create Scenario

ID:        aria_first_meeting

Trigger:
[ Actor enters location ▼ ]

Actor:
[ Aria ]

Location:
[ Tavern ]

Conditions:
[ + Add ]

Actions:
[ + Add ]

[Create]
```

Точная структура зависит от принятой scenario architecture.

## 25. Gameplay Event и Scenario

Plugin должен различать:

```text
Gameplay Event
```

как runtime post-fact event:

```text
rh:event.actor.recruited
```

и:

```text
Scenario
```

как content definition, реагирующую на trigger/event.

UI не должен использовать одно слово `Event` для обеих сущностей без пояснения.

## 26. Scenario Stub Generation

Если scenario требует Lua handler, plugin может создать минимальный stub.

Например:

```lua
scenario("aria_first_meeting", function(ctx)
    -- TODO
end)
```

Но generation должен использовать documented template/API.

Plugin не должен генерировать gameplay code, обходящий canonical authoring/runtime layer.

## 27. Link Existing

Кнопка:

```text
Link Existing...
```

показывает совместимые definitions.

Например для Actor Scenario:

```text
tavern_intro
aria_recruited
aria_defeated
```

и сохраняет canonical/package-relative reference согласно authoring contract.

## 28. Validation Button

`Validate` запускает authoritative GV2 validation.

UI-level PropertyBag validation не считается достаточной.

Pipeline:

```text
UI validation
     ↓
Serialize temporary authoring model
     ↓
GV2 Content validation
     ↓
Diagnostics
```

## 29. Diagnostics Panel

Ошибки должны отображаться в human-friendly виде.

Например:

```text
Aria → Home Location

"tavrn" does not resolve to a Location.

Did you mean:
Tavern
```

При возможности diagnostic должен содержать:

```text
file
field
Stable ID
schema field
typed error code
```

## 30. Save

`Save`:

1. синхронизирует PropertyBag в authoring model;
2. сериализует JSON5;
3. запускает required validation;
4. обновляет reference view;
5. сохраняет обычный filesystem file.

## 31. Auto-Save

Auto-save не рекомендуется вводить на первом этапе.

Изменение schema или references может иметь широкий impact.

Предпочтительно explicit Save + dirty state indication.

## 32. JSON5 Raw View

Для advanced users может существовать:

```text
[Properties] [Raw JSON5]
```

Raw mode не является обязательным для v1.

Если он реализуется, переключение назад в Properties должно повторно parse/validate content.

## 33. Source Control

Поскольку data остаются обычными text files, plugin автоматически сохраняет совместимость с Git.

Дополнительная Git integration может быть добавлена позже, но не является обязанностью первого этапа.

## 34. UE Editor Only

Plugin находится в Editor module и не должен попадать в shipping runtime dependency.

Например:

```text
GV2ContentEditor
Type = Editor
```

Runtime/Headless остаются независимыми.

## 35. Предлагаемые модули

Концептуальная структура:

```text
Plugins/GV2/

Source/
├── GV2Runtime/
├── GV2ContentCore/
└── GV2ContentEditor/
```

или существующая equivalent project structure.

`GV2ContentEditor` зависит от runtime/content tooling, но runtime не зависит от Editor module.

## 36. UI Technology

Рекомендуется использовать:

```text
Slate
PropertyEditor
DetailsView
FInstancedPropertyBag
```

Editor Utility Widgets могут использоваться для prototype, но production version рекомендуется оформить как полноценный Editor Plugin.

## 37. Почему не DataAsset

Gameplay definitions не рекомендуется превращать в `UDataAsset`.

Это ухудшит:

```text
headless compatibility
external modding
text diffs
LLM access
CLI tooling
UE independence
```

`UObject`/PropertyBag используются только как temporary editor representation.

## 38. Почему не DataTable как основное хранилище

DataTable может быть полезен как optional bulk-view:

```text
Actor    HP   Gold
Aria     100  50
Cassia    80  25
```

но не подходит как canonical representation сложных nested definitions.

Поэтому DataTable не заменяет Details-based editor.

## 39. Возможный Bulk Editor

На последующем этапе можно добавить табличный режим:

```text
[Details] [Table]
```

для однотипных scalar fields.

Например:

```text
Actor     HP   Gold   Home
Aria      100  50     Tavern
Cassia     80  20     Market
Marcus    120  10     Tavern
```

## 40. Что должно быть готово до начала работы

Раздел добавлен при review. Плагин опирается на возможности, которых сейчас нет; без них он будет генерировать изменения, которые нечем проверить.

### 40.1. Блокирующее

**Правило версионирования схем.** Разделы про `+ Add Property` описывают изменение схемы из UI, но не говорят, какие изменения допустимы на месте, а какие требуют поднятия `schema_version`. Нужна нормативная классификация: добавление необязательного поля с default; добавление обязательного поля (делает невалидными существующие definitions); сужение типа или диапазона; удаление поля; переименование. Для каждого класса — что происходит с уже написанным контентом и с сохранениями, чьи секции ссылаются на эти definitions. Кнопка, меняющая схему без такого правила, — способ сломать пакет одним кликом. Impact analysis из §22 показывает последствия, но не отвечает, разрешено ли изменение вообще.

**Правка поля в существующем файле.** `Json5AstRewriter` умеет ровно три вещи: создать файл, вставить новую запись и заменить строковые токены (этим работает `rename`). Изменить значение поля или удалить запись он не умеет. Плагин без этого может только создавать и переименовывать; сохранение изменённого поля потребует перезаписи файла целиком, что уничтожит комментарии и форматирование — и это заметит первый же автор, открывший diff.

**Authoring-метаданные схемы.** Раздел про метаданные (`label`, `category`, `order`, widget hint) описывает то, чего в схемах нет. Нужно решить, где они живут — отдельный блок в файле схемы или отдельный файл рядом, — и зафиксировать, что они не влияют на `content_hash`, по образцу PO-каталогов ([LOC-03](../Plans/Archive/LocalizationPipeline/README.md)). Без этого форма строится по одному лишь порядку полей в схеме, и любая перестановка полей меняет UI.

### 40.2. Уже готово и переиспользуется

| Возможность плагина | Чем закрыта сегодня |
|---|---|
| Схема для построения формы | `gv2-content describe --format=json` отдаёт типы, обязательность, ограничения, `target_kind` ссылок и `resource_class` |
| Find Usages | `gv2-content refs --format=json` с файлом, строкой, колонкой и JSON-указателем |
| Create Definition | `gv2-content new` создаёт запись с плейсхолдерами и создаёт файл, если его нет |
| Переименование со ссылками | `gv2-content rename` с проверкой `package_frozen` |
| Валидация и диагностика | `gv2-content validate --format=json`, диагностики с позицией и `definition_id` |
| Списки для пикеров | `gv2-content index --format=json` |
| Разрешение схем по набору пакетов | Реализовано: команды видят пакет вместе с его зависимостями |

Отдельно: `duplicate` в CLI нет, хотя раздел про дублирование его предполагает — либо добавить в CLI, либо реализовать в плагине через `new` плюс копирование значений.

### 40.3. Рекомендуемый порядок

1. Правило версионирования схем — нормативное решение, нужно до любого UI, меняющего схему.
2. Правка поля в `Json5AstRewriter` плюс round-trip тест, сохраняющий комментарии.
3. Authoring-метаданные и их выдача в `describe`.
4. Прототип формы только на чтение — проверить, что `FInstancedPropertyBag` покрывает реальные схемы `rh`.
5. Запись, начиная с одного kind.

Первые три пункта не требуют Unreal Editor вовсе и проверяются существующими средствами.

## 41. Первый Prototype

Первый prototype должен поддерживать только:

```text
Actor Browser
Actor Details
GV2 schema → PropertyBag
integer field
enum field
string field
location reference picker
load JSON5
save JSON5
Validate
```

Пример:

```text
Actor: Aria

Type             NPC
HP               100
Gold             50
Home Location    Tavern
```

## 42. Второй этап

После успешного prototype:

```text
create/duplicate/delete
find usages
resource picker
text picker
schema categories
Add Property
```

## 43. Третий этап

После подтверждения workflow реальным gameplay:

```text
Create Scenario
Link Scenario
schema editing
impact analysis
raw JSON5 view
bulk edit
```

## 44. Четвёртый этап

Только при необходимости:

```text
visual conditions/actions
scenario graph
localization workspace
external mod editor
live content preview
```

## 45. Не входит в Plugin

Plugin не должен:

```text
владеть canonical state
выполнять gameplay rules
создавать второй repository
изменять save format
создавать gameplay UObject model
делать UE обязательным для mods/headless
обходить Content Core validation
```

## 46. Критерии успеха

Plugin считается успешным, если gameplay designer может:

1. открыть `GV2 Content`;
2. выбрать `Actors → Aria`;
3. изменить `HP`;
4. выбрать `Home Location` из dropdown;
5. добавить новый schema property через UI;
6. создать/привязать scenario;
7. сохранить;
8. получить обычный валидный GV2 JSON5 content;
9. не знать внутреннюю реализацию `FInstancedPropertyBag`, Repository или Stable ID parser.

## 47. Главный архитектурный инвариант

> **GV2 Content Editor Plugin — это UE-native frontend к существующей data-driven архитектуре GV2, а не новая gameplay architecture внутри Unreal Editor.**

## 48. Ожидаемый результат

Типичный workflow должен выглядеть так:

```text
GV2 Content
  ↓
Actors
  ↓
Aria
  ↓
edit form
  ↓
+ Property
  ↓
+ Scenario
  ↓
Validate
  ↓
Save
```

а результатом остаются:

```text
JSON5 definitions
Lua gameplay modules
GV2 schemas
Stable IDs
canonical Commands/Events
```

без необходимости вручную работать с ними в большинстве типовых authoring-задач.
