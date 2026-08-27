---
title: GV2 Universal UI Property Pipeline Implementation Review 2026-08-27
status: informative
version: 1.0
updated: 2026-08-27
depends_on:
  - ImplementationStatus.md
  - ../ADR/0040-universal-ui-property-pipeline.md
---

# GV2 Universal UI Property Pipeline — Implementation Review

> **Показывает:** внешнее ревью реализации ADR-0040 на commit `06b3bd220d67d4ff36e3ede102b6223a0573902f` (ветка `main`), как оно было получено 2026-08-27.
> **Не является нормативным:** подтверждённые находки ведутся планами [PipelineClosureCorrection](../Plans/PipelineClosureCorrection/README.md) и [DataDrivenUiComposition](../Plans/DataDrivenUiComposition/README.md); при расхождении источником считаются они.

## 1. Scope

Проверено актуальное состояние `main` после завершения и архивирования плана **UniversalUiPropertyPipeline**.

Проверяемый HEAD:

```text
06b3bd220d67d4ff36e3ede102b6223a0573902f
```

Коммит переводит proposal в состояние `implemented` и архивирует его как завершённую реализацию ADR-0040.

Цель ревью — проверить не наличие отдельных классов/тестов, а выполнение архитектурных гарантий нового pipeline:

- data-driven UI schemas;
- generic prepared value tree;
- `IGV2UiPropertyHost`;
- `SchemaContract ⊆ WidgetCapabilities`;
- observability capability;
- Prepare без мутации live UI;
- Commit как infallible-style operation;
- document/screen atomicity;
- единый portable schema validator;
- nested Screen Fields;
- mod schema isolation;
- удаление schema-specific transport и parallel legacy surface.

---

# 2. Общий вывод

Основное архитектурное направление реализовано правильно.

Новый pipeline действительно заменил старую schema-specific цепочку:

```text
PrepareXxx
→ BuildXxx
→ FGV2XxxViewModel
→ ApplyXxx
```

на generic модель:

```text
portable value
→ compiled UI schema
→ FGV2PreparedUiValue
→ widget capabilities
→ property consumer
→ renderer/control
```

В частности:

- `FGV2ScreenFieldValue` больше не содержит schema-specific payload union;
- появился recursive `FGV2PreparedUiValue`;
- UI schemas вынесены в `GameData/*/schemas`;
- виджеты публикуют capabilities через `IGV2UiPropertyHost`;
- существуют standard property consumers;
- `ScalePolicy::Unset` реализован;
- observability harness расширен до всех host-классов и уже обнаружил реальный новый defect (`UGV2ModalWidgetBase.key`);
- предыдущий schema-specific adapter registry фактически снят.

Это подтверждает, что **само решение ADR-0040 было правильным**.

Однако текущую реализацию пока нельзя считать полностью замкнутой и окончательно завершённой.

Обнаружены несколько архитектурных gaps, пять из которых относятся к уровню **P1**. Наиболее важный из них позволяет снова получить исходный класс ошибки:

> schema принимает property, pipeline сообщает success, но значение не доходит до widget.

Только теперь этот defect находится внутри repeated collections.

---

# 3. Summary of findings

| ID | Severity | Summary | Recommendation |
|---|---|---|---|
| `UPP-R1` | **P1** | Collection item schema не проверяется recursively против entry-widget capabilities; возможен silent property loss | Передавать actual compiled item schema в child Prepare и рекурсивно проверять `Schema ⊆ Capabilities` |
| `UPP-R2` | **P1** | Commit и document reconciliation не обеспечивают заявленную atomicity | Сделать commit reversible/staged и обязательно проверять все commit/attach results |
| `UPP-R3` | **P1** | UE materializer использует второй validator вместо `ValidateUiFieldValue`; теряются min/max/default semantics | Использовать один portable validator как единственный source of value validation/materialization |
| `UPP-R4` | **P1** | `screen_fields` объявлен Core-kind, но production materializer его не поддерживает end-to-end | Реализовать nested Screen Fields через тот же normal screen field pipeline |
| `UPP-R5` | **P1** | UI schema cache живёт отдельно от active repository/package closure; mod rejection policy не реализована | UI schemas должны принадлежать pinned repository/active package graph |
| `UPP-R6` | **P2** | `DescribeUiCapabilities` / Prepare могут создавать internal repeaters и мутировать live UObject state | Выполнять wiring/init заранее; capability discovery сделать read-only |
| `UPP-R7` | **P2/P3** | Reset path может silently succeed; contracts частично отстали от нового API | Усилить reset validation и синхронизировать нормативную документацию |

---

# 4. Что реализовано корректно

## 4.1 Generic prepared value tree

`FGV2PreparedUiValue` поддерживает:

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

`Key` отделён от `String`, что соответствует принятому правилу semantic identity и предотвращает возвращение к display-text/position-derived identity.

`FGV2PreparedUiObject` и `FGV2PreparedUiArray` являются immutable prepared containers.

Это соответствует ADR-0040.

---

## 4.2 Data-driven schemas

UI schemas физически находятся в данных:

```text
GameData/core/schemas/
```

и включают, среди прочего:

```text
ui_field_button_v1
ui_field_button_list_v2
ui_field_checkbox_v1
ui_field_dropdown_select_v1
ui_field_image_v1
ui_field_input_field_v1
ui_field_modal_v1
ui_field_portrait_v1
ui_field_progress_bar_v1
ui_field_tab_container_v1
...
```

Schema compiler поддерживает standard UI kinds и `schema_ref`.

---

## 4.3 Capability model

Виджеты объявляют renderer-facing properties через:

```cpp
IGV2UiPropertyHost::DescribeUiCapabilities(...)
```

Существует generic compatibility check:

```text
SchemaContract ⊆ WidgetCapabilities
```

с проверками:

- kind;
- ref target kind;
- numeric capability ranges;
- keyed collection identity.

---

## 4.4 Observability harness

Observability infrastructure является сильной частью реализации.

Последний substantive implementation commit расширил sweep до всех 18 классов `IGV2UiPropertyHost`.

При этом harness действительно обнаружил новый live defect:

```text
UGV2ModalWidgetBase
capability: key
consumer: accepted
observable widget state: unchanged
```

Причина была в отсутствии modal branch внутри `FGV2KeyPropertyConsumer::Commit`.

Это хороший empirical proof, что observability gate является load-bearing и способен ловить именно тот класс дефектов, ради которого был создан UPP.

---

# 5. Findings

# UPP-R1 — P1
## Repeated collection item schema не входит в полный `Schema ⊆ Capabilities` check

### Проблема

Top-level object schema проверяется против capability tree.

Для collection schema проверяется в основном наличие `keyed_by`, но **actual `items` schema не проверяется recursively против capabilities entry widget**.

После этого `FGV2KeyedCollectionPropertyConsumer` создаёт child schema не из исходного compiled `Spec.Items`, а обратно из capabilities фактического entry widget.

Упрощённо текущий поток:

```text
real collection schema
        |
        | only array/keyed_by compatibility
        v
prepared item
        |
        v
entry widget capabilities
        |
        v
synthetic child schema
        |
        v
PrepareUiHostProperties()
```

Вместо требуемого:

```text
real Spec.Items
        |
        +------ recursive compatibility ------+
        |                                     |
        v                                     v
prepared item                         entry capabilities
        |                                     |
        +--------------- Prepare --------------+
```

### Failure scenario

Schema:

```json5
items: {
  kind: "array",
  keyed_by: "key",
  items: {
    kind: "object",
    fields: {
      key:     { kind: "key" },
      text:    { kind: "text" },
      binding: { kind: "binding" },
      foo:     { kind: "string" },
    },
  },
}
```

Entry widget capabilities:

```text
key
text
binding
```

Property `foo` отсутствует.

Top-level compatibility видит:

```text
items = Array
keyed_by exists
```

и принимает schema.

Materializer также создаёт prepared `foo`.

Но collection consumer строит child schema из widget capabilities:

```text
key
text
binding
```

`foo` в эту synthetic schema не входит.

В результате:

```text
foo пересёк Lua → UE boundary
foo прошёл schema materialization
foo не был применён
parent returns success
```

Это непосредственное возвращение исходного класса:

> accepted and silently dropped

### Дополнительная проблема

Обратное также возможно.

Если entry widget объявляет capability, которого нет в реальной item schema, synthetic schema начинает считать эту capability schema-owned.

Таким образом реальные data schema и child apply contract перестают быть одним и тем же объектом.

### Impact

**Высокий.**

Repeated collections используются для:

- buttons;
- dropdown options;
- meters;
- inventory/effect icons;
- scene characters;
- tabs;
- потенциальных будущих generic lists.

Поэтому defect находится в масштабируемой части pipeline.

### Recommendation

Не создавать schema из capabilities.

`FGV2KeyedCollectionPropertyConsumer::Prepare()` должен получить actual compiled `Spec.Items`.

Необходимо recursive правило:

```text
Array schema
  └── Items schema
        └── SchemaContract ⊆ EntryWidgetCapabilities
```

Каждый key в prepared item должен быть либо:

1. поддержан child capability;
2. structural-only property, явно принадлежащий collection consumer;
3. либо candidate должен быть rejected.

Добавить отрицательный test:

```text
schema item contains unsupported property
→ Prepare parent collection == false
```

и mutation test:

```text
remove recursive item compatibility check
→ test must turn red
```

---

# UPP-R2 — P1
## Commit и document reconciliation не являются atomic

### Contract

ADR-0040 требует:

```text
Prepare:
    fallible
    no live mutation

Commit:
    infallible-style
```

При неожиданном Commit failure:

```text
candidate revision не публикуется
previous revision остаётся active
property_path диагностируется
```

### Current host-level behavior

`CommitUiHostProperties()` выполняет mutations последовательно:

```text
Mutation A → Commit
Mutation B → Commit
Mutation C → Fail
return false
```

Rollback предыдущих mutations отсутствует.

Следовательно:

```text
A уже применена
B уже применена
C упала

→ screen physical state частично candidate
```

### Current screen-level behavior

`UGV2ScreenWidgetBase::CommitScreenFields()` также выполняет prepared field plans последовательно.

При failure он пишет diagnostic и возвращает `false`.

Сам код прямо отмечает:

```text
there is nothing to roll back to
(no compensating capture exists any more)
```

То есть заявленная ADR guarantee фактически отсутствует.

### Current document-level behavior

`FGV2LayeredUiReconciler::CommitReconcile()` выполняет:

```text
1. detach replaced old screens
2. attach new screens
3. CommitScreenFields()
4. detach removed screens
5. ActiveScreens = candidate
6. update interactivity
7. return true
```

При этом return values:

```text
AttachScreenToLayer(...)
CommitScreenFields(...)
```

не участвуют в результате commit.

Особенно опасен replacement path:

```text
detach old
attach new
commit new
```

Если commit нового Screen неожиданно падает, старый Screen уже снят.

### Почему текущий failure-injection test недостаточен

Текущий test создаёт mutation plan, где первые mutations не содержат реального consumer/target.

Failure injector срабатывает на последующей property.

Поэтому test доказывает:

```text
Commit returns false
property_path reported
```

но не доказывает:

```text
first physical renderer mutation happened
second mutation failed
first mutation was undone
```

Именно это является необходимой atomicity guarantee.

### Impact

**Высокий.**

Unexpected engine-level failure может оставить:

- partial screen state;
- detached previous widget;
- candidate active map;
- несогласованность между visual tree и binding revision.

### Recommendation

Выбрать один из двух подходов.

### Variant A — reversible mutation plan

Каждая mutation хранит:

```text
PreparedNewValue
PreviousCommittedValue / undo operation
```

Commit:

```text
commit A
commit B
fail C
rollback B
rollback A
```

### Variant B — staged presentation swap

Все новые/replacement screens и их fully committed child presentation строятся off-tree.

После полного успешного staging выполняется один final publication step:

```text
swap old tree → new tree
publish ActiveScreens
publish bindings
```

Для reused widgets всё равно понадобится reversible mutation или shadow state.

Дополнительно `CommitReconcile()` обязан проверять:

```cpp
if (!AttachScreenToLayer(...))
    return false;

if (!CommitScreenFields(...))
    return false;
```

Нужны реальные failure-injection tests:

```text
real property A commits
property B injected failure
A physically restored
previous screen remains attached
ActiveScreens unchanged
bindings revision unchanged
```

---

# UPP-R3 — P1
## Production UE materializer использует второй schema validator

### Архитектурная проблема

В `GV2ContentCore` уже существует canonical portable validator:

```cpp
ValidateUiFieldValue(...)
```

Он знает:

- scalar types;
- nullable;
- min/max;
- defaults;
- closed object semantics;
- nested object validation;
- array limits;
- keyed collections;
- TextSpec/Binding shapes;
- refs.

Однако UE runtime materializer выполняет отдельную самостоятельную валидацию через:

```cpp
WalkFieldValue(...)
```

Таким образом существуют две реализации schema semantics:

```text
portable:
    ValidateUiFieldValue

UE:
    WalkFieldValue
```

### Concrete mismatch: numeric constraints

Schema:

```json5
percent: {
  kind: "number",
  min: 0.0,
  max: 1.0,
  required: true,
}
```

Portable validator вызывает scalar validation и должен отвергнуть:

```text
percent = 1.5
```

Но `WalkFieldValue()` для Number проверяет только тип:

```text
double → accepted
int64  → converted to double
```

`min/max` там не применяются.

`FGV2NumberPropertyConsumer::Prepare()` также только читает `AsNumber()` и не проверяет capability range.

### Concrete mismatch: defaults

Portable `ValidateUiFieldValue()` materializes scalar defaults для отсутствующего optional field.

`WalkFieldValue()` при отсутствующем optional child делает:

```text
continue
```

и default не материализуется.

### Consequence for mod schemas

Widget capability:

```text
0.0 .. 1.0
```

Mod schema:

```text
0.0 .. 0.5
```

Такая schema является допустимым subset capability.

Candidate:

```text
0.8
```

должен быть rejected самой schema.

Но UE materializer может его принять, потому что проверяет только тип, а widget capability допускает до `1.0`.

### Impact

**Высокий.**

Data-driven schema перестаёт быть единственным contract of value.

Headless/content tooling и actual UE session могут принимать разные данные.

Это напрямую противоречит цели:

```text
same portable validation
same gameplay/presentation contract
```

### Recommendation

Удалить schema-validation responsibility из `WalkFieldValue`.

Pipeline должен быть:

```text
raw FValue
    ↓
ValidateUiFieldValue()
    ↓
materialized validated FValue
    ↓
UE semantic conversion
    ├── Text → FGV2TextViewModel
    ├── Binding → prepared handle
    ├── Ref → FGV2PreparedUiStableId
    └── primitive/object/array → prepared value
```

UE walker после portable validation не должен повторно решать:

- min/max;
- default;
- required;
- closed schema;
- key grammar;
- array bounds.

Нужны parity tests:

```text
for every production ui schema:
    portable validator result
    ==
    UE materializer acceptance result
```

для:

```text
valid value
bad type
below min
above max
missing required
defaulted optional
unknown property
duplicate key
```

---

# UPP-R4 — P1
## `screen_fields` не реализован end-to-end

### Contract/schema

Core schema vocabulary содержит:

```text
screen_fields
```

`ui_field_tab_container_v1` реально использует:

```json5
fields: {
  kind: "screen_fields",
  required: false,
}
```

### Production materializer

`WalkFieldValue()` не реализует `EUiFieldKind::ScreenFields`.

Этот case попадает в:

```text
field kind is not supported by the screen field materializer
```

Следовательно:

```text
tab without nested fields
→ может пройти

tab with nested fields
→ materialization failure
```

### Consumer-level mismatch

`FGV2TabContainerTabsPropertyConsumer` уже имеет отдельную nested-screen логику.

Но `fields` там ожидается как generic Object.

Далее child schema снова синтезируется из child capabilities, а не используется normal screen-field envelope:

```text
field_id
schema_id
value
```

То есть nested Screen фактически использует другой protocol.

### Registry fallback

Tab consumer начинает с:

```cpp
TargetWidgetClass = UGV2ScreenWidgetBase::StaticClass();
```

и только если Screen Registry существует, выполняется реальное resolution.

При отсутствии registry это создаёт возможность fallback к generic base Screen вместо typed rejection.

### Impact

**Высокий** для extensibility.

`TabContainer` был ключевым примером recursive/nested composition.

Пока nested `fields` не проходят тот же normal Screen Field lifecycle, pipeline нельзя считать полноценно recursive.

### Recommendation

`screen_fields` должен материализоваться в dedicated prepared structural value, содержащий normal field envelopes:

```text
PreparedScreenFields
[
    {
        field_id,
        schema_id,
        compiled_schema,
        prepared_value
    }
]
```

Nested Screen должен затем использовать:

```text
UGV2ScreenWidgetBase::PrepareScreenFields
UGV2ScreenWidgetBase::CommitScreenFields
```

а не synthetic object-schema из capabilities.

Screen Registry должен быть mandatory dependency nested-screen consumer:

```text
registry missing
→ Prepare failure
```

без base-class fallback.

---

# UPP-R5 — P1
## UI schemas не принадлежат active repository/package closure

### Current design

`FGV2UiSchemaCache` самостоятельно сканирует filesystem:

```text
GameData/core
GameData/textsystem
GameData/rh
GameData/sample
```

и ищет:

```text
*.schema.json5
```

Cache не является частью pinned `GameDataRepository`.

### Runtime packages имеют другой source of truth

`FGV2SessionCoordinator` получает реальный:

```text
RuntimePackageRoots
```

и на их основе загружает package scripts/content.

UI schema cache этот список не получает.

Следовательно существуют две package views:

```text
RuntimeSession package closure
```

и:

```text
UiSchemaCache filesystem roots
```

### Consequences

#### Arbitrary mod

```text
GameData/my_mod/schemas/foo.schema.json5
```

может быть частью active package graph, но fixed UI schema roots его не увидят.

#### Inactive built-in package

Schema из:

```text
sample
```

или:

```text
rh
```

может попасть в cache даже тогда, когда package не является active.

### Mod failure policy

ADR требует:

```text
bad core/textsystem/rh schema
→ project/session startup failure

bad mod schema
→ reject mod
→ continue session without mod
```

Но actual compatibility layer только маркирует mod diagnostics как nonfatal:

```text
Diag.bFatal = false
```

при этом сама compatibility function всё равно возвращает failure.

`PrepareUiHostProperties()` при failure отвергает candidate.

На initial document path это может закончиться:

```text
InitialPresentationInvalid
InitialPresentationApplyFailed
```

и session переходит в Failed.

То есть:

```text
reject only offending mod
```

не реализовано как package-level operation.

### Impact

**Высокий**, потому что это boundary между extensible content architecture и UI pipeline.

Сторонний мод всё ещё потенциально способен воздействовать на session startup не тем способом, который зафиксирован ADR.

### Recommendation

Compiled UI schemas должны находиться в immutable repository snapshot.

Предлагаемый authority:

```text
Package discovery
      ↓
dependency closure
      ↓
schema ownership validation
      ↓
compile UI schemas
      ↓
reject incompatible mod package if needed
      ↓
publish immutable GameDataRepository
      ↓
session pins repository
      ↓
UI materializer resolves schema only from pinned repository
```

Filesystem `FGV2UiSchemaCache` не должен быть отдельным runtime authority.

---

# UPP-R6 — P2
## Prepare/capability discovery мутирует Location composite host

### Contract

ADR:

```text
Prepare не изменяет live presentation state
```

### Current Location implementation

Location composites используют lazy internal adapters:

```cpp
ResolveMeterRepeater()
ResolveItemRepeater()
ResolveEffectRepeater()
ResolveCharacterRepeater()
ResolveRepeater()
```

При отсутствии authored repeater они выполняют:

```cpp
NewObject<UGV2ListViewWidgetBase>(this)
SetContainerPanel(...)
```

### Capability discovery

`DescribeUiCapabilities()` вызывает эти `Resolve*()` через `const_cast`.

То есть даже простой запрос:

```text
host.DescribeUiCapabilities(...)
```

может изменить UObject state.

Generic `PrepareUiHostProperties()` также использует lazy getters при target resolution.

### Почему это важно

Даже rejected candidate может оставить после себя:

```text
InternalMeterRepeater != null
InternalItemRepeater != null
...
```

Хотя pixels могли не измениться, live host state уже отличается от initial state.

Это нарушает сильную формулировку Prepare purity.

### Recommendation

Все internal renderer adapters должны быть созданы в отдельной deterministic wiring phase:

```text
NativeConstruct / InitializeRendererBindings
```

До первого Prepare.

После initialization:

```text
DescribeUiCapabilities()
ResolveTarget()
Prepare()
```

должны быть logically `const`.

Добавить test:

```text
capture host UObject graph/state
Prepare(valid)
state unchanged

capture
Prepare(invalid)
state unchanged
```

не только renderer pixels.

---

# UPP-R7 — P2/P3
## Reset path и normative docs требуют доведения

## 7.1 Reset может silently succeed

Для present property `PrepareUiHostProperties()` проверяет:

```text
TargetWidget exists
Consumer exists
Consumer.Prepare succeeds
```

Для reset mutation consumer/target создаются, но отсутствие target или consumer не обязательно отклоняет Prepare.

Commit делает:

```cpp
if (Mutation.Consumer.IsValid() && Mutation.TargetWidget.IsValid())
{
    Mutation.Consumer->Reset(...);
}
```

Иначе просто продолжает.

Таким образом reset property потенциально может быть:

```text
required reset
target missing
reset skipped
Commit success
```

Это снова разновидность:

> accepted but not consumed.

### Recommendation

Reset mutation должна иметь такие же invariants, как Apply mutation.

Перед добавлением reset plan:

```text
consumer must exist
target must resolve
reset support must be explicit
```

или reset должен быть intrinsic operation самого property consumer/capability.

---

## 7.2 Documentation drift

Нормативный `WidgetRegistry.md` всё ещё перечисляет многие native classes как реализующие:

```text
IGV2ScreenFieldHost
```

хотя после UPP часть leaf widgets реализует только:

```text
IGV2UiPropertyHost
```

и screen-field ownership вынесен в configured host placement/composites.

Также `ScreenTemplates.md` всё ещё описывает configured Screen Field element как имеющий:

```text
schema_id
required/optional policy
```

в самом host contract, тогда как actual `IGV2ScreenFieldHost` по новой модели фактически предоставляет только:

```text
field_id
```

Schema ID приходит из runtime field envelope.

### Recommendation

После исправления runtime findings синхронизировать:

```text
Docs/UI/WidgetRegistry.md
Docs/UI/ScreenTemplates.md
ADR-0040 consequences/closure notes
ImplementationStatus.md
```

---

# 6. Verification status

Последний substantive implementation commit перед архивированием сообщает локальную проверку:

```text
94/94 UE automation
68/68 portable ctest
validate_docs: 167 files
gv2-headless --self-test: green
```

Эти результаты являются полезным evidence implementation commit.

Однако на проверяемом HEAD:

```text
06b3bd220d67d4ff36e3ede102b6223a0573902f
```

GitHub не показывает:

```text
combined status checks
workflow runs
```

Поэтому данное ревью **не утверждает**, что full current HEAD независимо подтверждён CI.

---

# 7. Recommended corrective plan

## Phase 1 — Close silent-loss paths

Приоритет:

```text
UPP-R1
UPP-R7 reset
```

Цель:

> Ни одна property, признанная schema-owned, не может исчезнуть без typed rejection.

Обязательные negative tests:

```text
unsupported collection item property
unsupported reset target
unknown collection child capability
```

---

## Phase 2 — Make schema validation single-source

Исправить `UPP-R3`.

Цель:

```text
ValidateUiFieldValue()
```

становится единственным portable validator/materializer.

UE-specific layer отвечает только за semantic conversion и renderer preparation.

После этого добавить parity test matrix для всех production UI schemas.

---

## Phase 3 — Complete recursive Screen composition

Исправить `UPP-R4`.

Цель:

```text
screen_fields
```

использует тот же normal Screen Field envelope и тот же:

```text
PrepareScreenFields / CommitScreenFields
```

для nested Screen.

Удалить synthetic child object schema.

---

## Phase 4 — Fix transactionality

Исправить `UPP-R2`.

Обязательная гарантия:

```text
failed candidate
→ exact previous physical presentation
→ exact previous ActiveScreens
→ exact previous binding revision
```

Failure injection должна происходить **после реальной successful mutation**, а не до неё.

---

## Phase 5 — Move UI schemas into repository authority

Исправить `UPP-R5`.

Убрать fixed filesystem schema discovery из runtime authority.

Schema lookup должен идти через pinned repository package closure.

Bad mod schema должна приводить к:

```text
mod rejected
dependency closure recomputed / package omitted
session continues
```

а не к initial presentation/session failure.

---

## Phase 6 — Purity and documentation cleanup

Исправить:

```text
UPP-R6
UPP-R7 docs
```

После чего повторно выполнить:

```text
portable ctest
full UE automation
headless self-test
validate_docs
observability sweep
mutation tests
```

---

# 8. Suggested status changes

Пока findings не закрыты, рекомендуется не считать Universal UI Property Pipeline полностью завершённым implementation state.

Сам proposal можно оставить в Archive как historical rationale, но `ImplementationStatus.md` стоит дополнить новыми подтверждёнными gaps.

Предлагаемые IDs:

```text
STATUS-006 — collection item schema/capability recursive coverage
STATUS-007 — UI Commit/document atomicity
STATUS-008 — portable/UE UI schema validation divergence
STATUS-009 — nested screen_fields incomplete
STATUS-010 — UI schema repository/mod package integration
```

`UPP-R6` можно либо оформить отдельным STATUS, либо закрыть как corrective implementation defect вместе с `STATUS-006/007`.

---

# 9. Final assessment

Архитектуру откатывать или возвращаться к schema-specific DTO/adapters не требуется.

Наоборот, текущий UPP уже показал преимущества:

- уменьшение schema-specific C++ surface;
- единый capability vocabulary;
- generic property consumers;
- data-driven schemas;
- `Key` как отдельный semantic type;
- observability harness действительно обнаруживает ошибки;
- дальнейшее расширение UI стало потенциально дешевле.

Но pipeline ещё не полностью выполняет собственные strongest invariants.

Особенно критичны:

```text
1. collection item property coverage;
2. Commit/document transactionality;
3. single portable schema validator;
4. nested screen_fields;
5. package-owned mod schemas.
```

До их закрытия наиболее точная характеристика текущего состояния:

> **Universal UI Property Pipeline архитектурно внедрён и является основным production path, но closure/transactionality и recursive schema guarantees ещё не полностью реализованы.**

После исправления `UPP-R1…R5` решение уже можно будет считать действительно замкнутым на уровне архитектурного контракта.
