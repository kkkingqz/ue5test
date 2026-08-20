---
title: Content Editor Plugin Proposal
status: archived
proposal_state: implemented
version: 1.0
updated: 2026-08-20
depends_on:
  - DesignerLuaAuthoringProposal.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Architecture/BuildAndTooling.md
  - ../../UI/ScreenTemplates.md
decisions:
  - ../../ADR/0018-portable-content-core-module.md
  - ../../ADR/0026-core-and-gameplay-ownership.md
  - ../../ADR/0029-content-authoring-and-schema-evolution.md
---

# Предложение по редактору контента в Unreal Editor

> **Предлагает:** Unreal Editor plugin как UE-native frontend поверх существующего GV2 authoring/tooling stack и канонических `.json5`.
>
> **Не предлагает:** новую gameplay-модель, второй repository, gameplay `UObject` model или перенос canonical content в `.uasset`.

## 1. Цель

Предоставить разработчику и gameplay designer встроенную в Unreal Editor среду для работы с GV2 definitions:

```text
Actors
Items
Locations
Schemas
References
Resources
Text references
Scenarios как обычные definitions
другими gameplay definitions
```

без необходимости вручную редактировать JSON5 в типовых случаях.

Plugin является одним из frontend авторского слоя наряду с `gv2-content`, ручным редактированием файлов и Lua authoring layer.

## 2. Главный архитектурный принцип

> **GV2 Content Editor Plugin — frontend к существующей GV2 authoring architecture, а не отдельная content architecture внутри Unreal Editor.**

Source of truth остаётся:

```text
GameData/<package>/...
```

Gameplay definitions не превращаются в `UDataAsset`, canonical `UDataTable`, generated USTRUCT model или persistent gameplay UObject graph.

Runtime и Headless не зависят от Editor plugin.

## 3. Authoritative backend

Authoritative реализация авторинга — **общая библиотека**, а не подпроцесс.

```text
Unreal Editor UI
        ↓
GV2 Editor Adapter
        ↓
gv2_content_authoring  (библиотека)
        ↓
GameData JSON5
```

Та же библиотека является backend-ом `gv2-content`. CLI и Editor становятся двумя frontend-ами одной реализации; ни один из них не является backend-ом другого.

### 3.1. Почему не CLI как backend

Цель «одна реализация авторинга на CLI, Editor, CI и тесты» правильная, но подпроцесс её не даёт: он добавляет границу процесса поверх кода, который и так общий.

Для **чтения** общая реализация доступна редактору напрямую уже сегодня: UE-модуль линкует `GV2ContentCore` и `GV2ContentHostSupport`, поэтому построение репозитория, разрешение схем и валидация выполняются в процессе.

Для **записи** препятствие реальное: `Json5AstRewriter` и реализации операций находятся внутри исполняемого таргета `gv2-content`, а не в библиотеке. Их нельзя вызвать ниоткуда, кроме самого CLI.

Отсюда предусловие: **слой авторинга поднимается в библиотеку до начала работы над плагином.** Это не оптимизация «на потом», а буквальное исполнение принципа единственной реализации.

### 3.2. Состав библиотеки

Библиотека владеет операциями, меняющими канонический контент:

```text
new
set
delete
rename
duplicate
```

плюс точечной правкой JSON5 с сохранением комментариев и форматирования.

Чтение (`describe`, `index`, `refs`, `validate`) остаётся за `GV2ContentCore` и `GV2ContentHostSupport`, которые уже являются библиотеками.

### 3.3. Транспорт скрыт адаптером

`GV2 Editor Adapter` скрывает способ вызова общей библиотеки. Запуск CLI-подпроцесса из Editor запрещён: это создало бы второй operation path с отличающимися lifecycle и error semantics.

## 3a. Атомарность записи

Операция сохранения обязана быть атомарной на диске: либо применены все изменённые поля, либо ни одного.

Последовательность из нескольких независимых `set`, каждый из которых перезаписывает файл, этому не удовлетворяет: отказ на третьем поле оставляет применёнными первые два, а состояние редактора расходится с файлом. То же относится к дублированию определения, если оно выражено серией `set`: сбой посередине оставляет в репозитории наполовину скопированное определение.

Требования:

- набор изменённых полей применяется **одной операцией**, а не серией независимых;
- запись выполняется по схеме «подготовить результат → провалидировать → заменить файл»;
- отказ на любом шаге оставляет файл в исходном состоянии;
- дублирование является одной операцией библиотеки, а не оркестрацией в редакторе.

Формулировка «UI воспринимает несколько операций как один Save» описывает поведение интерфейса и гарантией на диске не является.

## 3b. Внешние изменения файла

Канонический контент — текстовые файлы под контролем версий, и это ценно (см. раздел о source control). Обратная сторона: файл может измениться под редактором — `git pull`, правка руками, второй экземпляр редактора, внешний инструмент.

Редактор обязан:

- фиксировать состояние файла в момент загрузки определения;
- отказывать в записи, если файл изменился с этого момента, — типизированной ошибкой, а не молчаливой перезаписью;
- предлагать перезагрузку с явной потерей несохранённых правок либо их сохранение в отдельное место;
- обнаруживать изменение и без попытки записи, чтобы показанные значения не расходились с диском незаметно.

Молчаливая перезапись чужой правки — худший из возможных исходов для редактора поверх файлов под git.

## 4. Общая архитектура

```text
                    Unreal Editor
                         │
                  GV2 Content Tab
                         │
                  GV2 Editor Adapter
                   /              \
                  /                \
          Field Adapters        Diagnostics UI
                │
                ▼
        PropertyBag / custom rows
                │
                ▼
        gv2_content_authoring
                │
                ▼
             GameData
```

При сохранении:

```text
edited UI fields
      ↓
dirty field set
      ↓
одна атомарная операция авторинга
      ↓
authoritative validation
      ↓
reload canonical definition DTO
      ↓
refresh UI/reference/diagnostics views
```

## 5. Не вводить отдельную mutable Editor content model

Plugin не должен поддерживать полноценную вторую mutable копию GV2 definition model.

В памяти Editor достаточно иметь:

```text
LoadedDefinitionDTO
current UI values
dirty fields
selection state
temporary widget/editor state
```

Canonical content изменяется только через authoring operations.

Не должно возникать конкурирующих моделей `PropertyBag → Editor Model → JSON5 AST → Repository` с отдельной synchronization logic.

## 6. Standard renderer не является canonical model

Стандартные schema fields могут отображаться `FInstancedPropertyBag` либо единым Slate renderer-ом:

```text
integer
number
boolean
string
enum
simple arrays
simple nested objects
```

Но plugin не предполагает, что любой GV2 field обязан полностью выражаться стандартным PropertyBag.

```text
Schema Field Descriptor
        ↓
Field Adapter Registry
        ↓
Editor representation
        ├─ standard renderer row
        └─ custom editor row/widget
```

Выбор renderer-а является implementation detail field adapter-а и не вводит canonical Editor model.

## 7. Field Adapter Registry

Plugin содержит небольшой внутренний registry field adapters. Это не публичная plugin system.

```text
integer              → DefaultPropertyAdapter
number               → DefaultPropertyAdapter
boolean              → DefaultPropertyAdapter
string               → DefaultPropertyAdapter
enum                 → EnumAdapter
ref<kind>             → DefinitionReferenceAdapter
resource<class>       → ResourceAdapter
text                  → TextReferenceAdapter
stable_id             → StableIdAdapter
```

Adapter отвечает за создание editor row/widget, чтение current value, формирование нового authoring value, локальную UI validation и optional preview/chooser behavior.

Authoritative validation всё равно выполняет GV2 tooling.

## 8. Schema UI metadata

Существующие `schemas/<name>.ui.json5` используются для editor presentation metadata:

```text
label
description
category
order
widget_hint
```

`widget_hint` может выбирать специализированный Field Adapter, но не меняет gameplay schema semantics.

Например:

```text
integer + widget_hint=slider
    → SliderAdapter

string + widget_hint=multiline
    → MultilineTextAdapter
```

Authoring UI metadata остаются отделены от canonical gameplay content и не влияют на package content hash согласно существующему contract.


## 9. Основной Editor Tab

Plugin регистрирует:

```text
GV2 Content
```

Предлагаемый layout:

```text
┌──────────────────────────────────────────────────────────────┐
│ GV2 Content                                                   │
├────────────────┬────────────────────────────┬────────────────┤
│ Definitions    │ Properties                 │ References     │
│                │                            │                │
│ Actors         │ Actor: Aria                │ Uses           │
│  Aria          │                            │  Tavern         │
│  Cassia        │ Type     NPC               │  AriaPortrait  │
│  Marcus        │ HP       100               │                │
│                │ Gold     50                │ Used by        │
│ Items          │ Home     Tavern            │  quest.main    │
│  Sword         │ Portrait AriaPortrait      │  tavern_intro  │
│                │                            │                │
│ Locations      │ [Open Actor Schema]        │                │
│  Tavern        │                            │                │
├────────────────┴────────────────────────────┴────────────────┤
│ + New   Duplicate   Delete   Validate   Save                  │
├──────────────────────────────────────────────────────────────┤
│ Problems (3)                                                  │
└──────────────────────────────────────────────────────────────┘
```

## 10. Definition Browser

Левая панель показывает definitions по kind.

Источник данных — операция построения индекса библиотеки; та же, что стоит за `gv2-content index`.

Browser не реализует собственную package overlay logic.

Поддерживаются:

```text
filter/search
group by kind
package indicator
Stable ID tooltip/copy
dirty state indication
```

## 11. Definition Selection

При выборе definition plugin:

1. получает canonical identifier;
2. получает schema/field description через `describe`;
3. получает authoring UI metadata;
4. строит field adapters;
5. показывает Properties;
6. получает reference information;
7. отображает incoming/outgoing references.

## 12. References panel

Правая панель разделяется минимум на:

```text
Uses
Used by
```

Пример:

```text
Actor: Aria

Uses:
  Home       → Tavern
  Portrait   → AriaPortrait

Used by:
  quest.main
  tavern_intro
```

Источником остаётся существующее GV2 reference tooling.

Double-click/reference activation переводит Editor к соответствующей definition, если она доступна.

## 13. Property mapping

Базовое отображение:

```text
GV2 type         Editor representation

integer          numeric field
number           numeric field
boolean          checkbox
string           text field
enum             selector
array            collection editor
object           nested/group editor
ref<T>           Definition Reference Picker
resource<T>      Resource Picker
text             Text Reference Picker/Preview
stable_id        Stable ID Viewer
```

Сложные случаи не должны заставлять расширять canonical C++ schema model.

## 14. Definition Reference Picker

Поле:

```text
home : ref<location>
```

отображается не как raw string, а как typed chooser:

```text
Home Location
[Tavern ▼] [Find]
```

Compatible targets берутся из GV2 tooling/index.

В canonical data сохраняется Stable ID/reference в формате, установленном существующим contract.

## 15. Resource Picker

Resource field:

```text
Portrait
[AriaPortrait] [Browse...] [Preview]
```

UE Asset Browser может использоваться как chooser физического asset-а только там, где это соответствует Resource contract.

В gameplay data сохраняется GV2 Resource ID, а не произвольный UE asset path, если canonical contract требует Resource ID.

## 16. Text field behavior

На первом этапе Text support ограничивается:

```text
Text reference selection
preview resolved text
Find/Open source when available
```

Полноценный localization workspace не входит в MVP и остаётся отдельным future feature.

Plugin не меняет существующий Text contract.

## 17. Definition editing и Schema editing — разные workflows

Обычный Definition Editor редактирует значения конкретной definition.

Например:

```text
Actor: Aria

Type        NPC
HP          100
Gold         50
Home        Tavern
```

Он **не должен** показывать обычную кнопку `+ Property`, потому что добавление schema field меняет модель всех compatible definitions.

Вместо этого:

```text
[Open Actor Schema]
```

открывает отдельный Schema Editor.

## 18. Schema Editor

Schema Editor работает отдельно от Definition Editor.

Пример:

```text
Actor Schema

Fields
  runtime_type
  hp
  gold
  home

[+ Field]
```

Schema operations:

```text
Add Field
Edit Field
Rename Field
Set Default
Change Category
Change UI metadata
Find Usages
Remove Field
```

выполняются через существующий Schema Authoring API/tooling.

Arbitrary per-definition properties не вводятся.

## 19. Schema change validation

Изменение schema всегда требует impact analysis.

Например:

```text
reputation: integer
    ↓
reputation: enum
```

UI должен показать:

```text
Affected definitions: 17
Invalid after change: 3
```

и потребовать explicit confirmation для поддерживаемых migration operations.

**Второе число нечем получить.** Количество затронутых определений даёт существующий `refs`; «сколько станет невалидным» — это пробный прогон валидации против изменённой схемы, которого не выполняет ни одна существующая операция.

Поэтому анализ влияния является **предусловием этапа со Schema Editor**, а не следствием уже имеющегося tooling: библиотека авторинга обязана получить операцию пробного применения изменения схемы, возвращающую список определений, которые станут невалидными, без записи на диск.

Unsupported in-place changes следуют существующему schema evolution contract.


## 20. Create Definition

`+ New` открывает:

```text
Kind:     Actor
Package:  rh
Local ID: aria
```

Plugin вызывает операцию создания библиотеки авторинга и затем перечитывает созданную definition через authoritative tooling.

Defaults и placeholders определяются существующим authoring contract, а не Editor-specific logic.

## 21. Duplicate Definition

Duplicate является **одной операцией библиотеки авторинга**, а не оркестрацией в редакторе.

Выражение дублирования серией `set` неприемлемо: сбой посередине оставляет в репозитории наполовину скопированное определение — ровно тот случай, который запрещает раздел об атомарности записи.

Не копируются буквально:

```text
Stable ID
source path identity
computed/derived values
```

Если позднее duplicate semantics станут сложнее, отдельная CLI operation может быть добавлена без изменения Editor UI.

## 22. Delete Definition

Delete:

1. получает incoming references;
2. показывает impact;
3. вызывает canonical delete operation;
4. не допускает silent dangling references;
5. refresh-ит index/reference views;
6. запускает validation.

## 23. Rename Definition

Rename выполняется операцией библиотеки авторинга и соблюдает существующие package/reference/frozen rules.

Editor не реализует собственную rename propagation logic.

## 24. Dirty field model

При изменении Properties plugin отслеживает:

```text
original value
current value
dirty flag
```

Save не сериализует definition заново целиком: библиотека поддерживает точечную правку с сохранением комментариев и форматирования.

Preferred flow:

```text
dirty fields
    ↓
одна атомарная операция авторинга
    ↓
validate
    ↓
reload canonical DTO
```

Это сохраняет comments, formatting и file structure согласно возможностям существующего JSON5 AST rewriter.

## 25. Save

`Save`:

1. собирает dirty fields;
2. выполняет необходимые authoring operations;
3. запускает authoritative validation;
4. при успехе reload-ит canonical data;
5. обновляет references;
6. очищает dirty state.

Набор изменённых полей применяется одной операцией: несколько независимых записей нарушили бы атомарность.

На v1 не требуется создавать новый monolithic JSON5 serializer внутри Editor plugin.

## 26. Auto-save

Auto-save не вводится в первой версии.

Причины:

```text
schema changes may have broad impact
reference edits can fail validation
multi-field edits benefit from explicit commit point
```

Используется explicit:

```text
Save
Revert/Reload
dirty indicator
```

## 27. Validation

Есть два уровня.

### 27.1 UI validation

Field Adapter может сразу обнаружить очевидные ошибки:

```text
wrong numeric format
invalid enum selection
empty required local input
```

### 27.2 Authoritative GV2 validation

Всегда выполняется authoritative validation через `GV2ContentCore` — та же, что исполняет CLI и CI.

UI-level validation не считается достаточной.

Editor не воспроизводит самостоятельно repository/content validation rules.

## 28. Problems / Diagnostics panel

Structured diagnostics отображаются как first-class Editor panel:

```text
Problems (3)

Error   Aria.home       unresolved location "tavrn"
Warning Sword.damage    ...
```

Diagnostic может содержать:

```text
severity
typed error code
message
file
line
column
Stable ID
schema field / JSON pointer
```

При double-click Editor:

```text
selects definition
focuses corresponding field
```

если mapping возможен.

## 29. Raw JSON5 view

Raw JSON5 view не входит в MVP.

Позже возможен:

```text
[Properties] [Raw JSON5]
```

При возврате из Raw mode content должен быть заново parse/validate-нут через canonical tooling.

Raw editor не должен иметь отдельную semantics.

## 30. Bulk editor

Bulk/table mode остаётся future optimization для однотипных scalar fields.

Например:

```text
Actor     HP   Gold   Home
Aria      100  50     Tavern
Cassia     80  20     Market
```

Он использует те же Editor Adapter, Field descriptors, authoring operations и validation и не становится отдельным storage model.

## 31. Понятия, которых ещё нет

Редактор работает с существующими kind-ами: `actor`, `item`, `location`, `action`, `resource`, `screen`, `text`.

Понятие «сценарий» в проекте отсутствует — его нет ни в схемах, ни в определениях, ни в нормативной документации. Поэтому редактор не вводит для него ни отображения, ни терминологии: специализированный визуальный workflow проектируется отдельным предложением **после** того, как соответствующий контентный и рантайм-контракт появится.

То же относится к любому будущему kind: редактор получает его бесплатно, как только у kind есть схема, потому что формы строятся из схемы. Специальный workflow — отдельное решение.

## 33. Source control

Поскольку canonical content остаётся text files, автоматически сохраняются:

```text
Git
diff
merge
external tooling
LLM access
```

Специальная Git integration не входит в v1.

## 34. UE Editor-only module

Plugin находится только в Editor module.

Conceptually:

```text
Plugins/GV2/
Source/
├── GV2Runtime/
├── GV2ContentCore/
└── GV2ContentEditor/
```

или equivalent существующей структуре проекта.

`GV2ContentEditor` может зависеть от Editor APIs, Content tooling adapter и shared value/DTO definitions, но runtime/headless не зависят от Editor module.

## 35. UI technology

Recommended:

```text
Slate
PropertyEditor
DetailsView
FInstancedPropertyBag
custom detail/property rows
```

Editor Utility Widgets допустимы для быстрых prototypes, но production implementation должен быть полноценным Editor plugin.

## 36. Почему не DataAsset

Gameplay definitions не превращаются в `UDataAsset`.

Это сохраняет:

```text
headless compatibility
external modding
text diffs
CLI tooling
LLM access
UE independence
```

`UObject`/PropertyBag допустимы только как temporary/editor representation.

## 37. Почему не DataTable

DataTable может быть полезен только как future bulk view.

Он не заменяет canonical JSON5 representation сложных nested definitions.

## 38. MVP scope

Первая production-версия должна поддерживать:

```text
Definition Browser
Actor Details
schema → field adapters
integer
number
boolean
string
enum
location reference picker
load
edit
dirty state
atomic save through authoring library
validate
Problems panel
incoming/outgoing references
```

Первый vertical slice охватывает три статических игровых kind-а:

```text
actor
item
location
```

Они покрывают принципиально разные случаи и вместе проверяют pipeline целиком:

| Kind | Что проверяет |
|---|---|
| `item` | Плоская схема одного пакета; единственный kind, у которого уже есть файл метаданных представления `.ui.json5` |
| `location` | Массив ссылок на определения (`connected_location_ids`) и ссылка на экран |
| `actor` | Базовая schema из dependency package плюс package-owned extension site выбранного provider-а; foreign namespace должен быть недоступен для записи |

**`world` требует предварительного решения.** Сегодня мир существует только как рантайм-состояние (`game.instances.world()`, `state.world`) и определением не является — редактировать в редакторе нечего. Чтобы он вошёл в срез, `world` сначала должен стать kind-ом со схемой, и это контентное решение (какому слою принадлежит, какие поля), а не задача редактора.

Порядок внутри среза: `item` первым как самый простой, `actor` последним как самый сложный.

## 39. MVP success criteria

MVP успешен, если gameplay designer может:

1. открыть `GV2 Content`;
2. выбрать любое определение из `actor`, `item` или `location`;
3. увидеть schema-driven form;
4. изменить scalar field;
5. выбрать `Home Location` через typed reference picker;
6. сохранить;
7. получить обычный корректно изменённый JSON5;
8. запустить authoritative validation;
9. увидеть structured diagnostics;
10. открыть incoming/outgoing references;
11. не знать внутреннюю реализацию PropertyBag, JSON5 AST rewriter, repository или Stable ID parser.

## 40. Stage 2

После подтверждения MVP:

```text
create
duplicate
delete
rename
resource picker
text reference picker
additional kinds
schema categories/UI metadata polish
```

## 41. Stage 3

После подтверждения реальным gameplay:

```text
separate Schema Editor
schema field operations
impact analysis UI
raw JSON5 view
bulk editor
```

## 42. Stage 4

Только при отдельной необходимости и отдельном design proposal:

```text
scenario-specific visual authoring
conditions/actions editor
scenario graph
localization workspace
live content preview
external mod editor
```

## 43. Не входит в Plugin

Plugin не должен:

```text
владеть canonical state
выполнять gameplay rules
создавать второй repository
изменять save format
создавать gameplay UObject model
делать UE обязательным для mods/headless
обходить authoring library/Content Core validation
самостоятельно интерпретировать package overlay
создавать отдельный schema semantics
```

## 44. Testing

Минимальные integration tests.

### Read

```text
index → browser
describe → form
refs → references
```

### Write

```text
edit one scalar
save
verify JSON5 changed
verify comments/format retained
validate
reload
```

### Reference

```text
choose compatible location
save
validate
find usage
```

### Error

```text
attempt invalid edit
receive structured diagnostic
navigate to field
```

### Headless independence

Shipping/runtime/headless build не получает dependency на `GV2ContentEditor`.

## 45. Architectural invariants

1. JSON5 остаётся source of truth.
2. Библиотека авторинга и Content Core остаются authoritative authoring implementation; CLI и Editor — два её frontend-а.
3. Editor не воспроизводит package/repository rules самостоятельно.
4. Standard renderer не становится gameplay model.
5. Editor хранит UI/dirty state, но не второй canonical content model.
6. Field-specific UI строится через небольшой Field Adapter Registry.
7. Definition editing и Schema editing — разные workflows.
8. Scenario-specific visual tooling не блокирует generic Content Editor.
9. Runtime/Headless не зависят от Editor plugin.
10. Save атомарен и всегда заканчивается authoritative validation.
11. Запись по устаревшему представлению файла отклоняется, а не выполняется молча.

## 46. Ожидаемый результат

Типичный MVP workflow:

```text
GV2 Content
    ↓
Actors
    ↓
Aria
    ↓
schema-driven form
    ↓
edit HP / Home
    ↓
Save
    ↓
atomic authoring operation
    ↓
Validate
    ↓
reload canonical JSON5
```

На диске результатом остаются:

```text
JSON5 definitions
Lua gameplay modules
GV2 schemas
Stable IDs
canonical Commands/Events
```

без создания параллельной Unreal gameplay model.

## 47. Главный принцип

> **GV2 Content Editor Plugin должен быть тонким UE-native frontend к уже существующему GV2 authoring toolchain. Чем меньше canonical content logic живёт внутри Editor module, тем лучше.**
