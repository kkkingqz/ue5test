---
title: GV2 Generic Boundary Hardening Follow-up Review 2026-09-02
status: informative
version: 1.0
updated: 2026-09-02
depends_on:
  - ImplementationStatus.md
  - ../ADR/0041-ui-commit-rollback-model.md
---

# GV2 Generic Boundary Hardening — Follow-up Review

> **Показывает:** внешнее ревью остаточных проблем транзакционной модели UI на commit `d9280908a348b5a41b58284bac98a48ce46dfaeb`, как оно было получено 2026-09-02.
> **Не является нормативным:** подтверждённые находки ведёт план [GenericUiTransactionFollowUp](../Plans/GenericUiTransactionFollowUp/README.md); при расхождении источником считается он.

## 1. Scope

Проверено актуальное состояние `main` после реализации и архивирования `GenericBoundaryHardening`.

Проверяемый HEAD:

```text
d9280908a348b5a41b58284bac98a48ce46dfaeb
```

Проверка направлена не на повторную проверку уже закрытых `REM-01…REM-07`, а на то, действительно ли новые механизмы выполняют заявленные invariants и не создали новый класс false-success / partial-revision ошибок.

Проверены в частности:

- `Source/GV2/Private/UI/GV2GameShellWidgetBase.cpp`
- `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`
- `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`
- `Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp`
- `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`
- `Source/GV2/Private/UI/GV2UiCapability.cpp`
- соответствующие public headers;
- `GV2.UI.PrepareCommitAndFailureInjection`;
- `GV2.UI.LayeredReconciliationContract`;
- `GV2.UI.StandardPropertyConsumers`;
- `GV2.UI.DeclaredComposite.*`;
- `ADR-0040`, `ADR-0041`, `ScreenTemplates`, `UIDocumentAndReconciliation`, `ImplementationStatus`.

## 2. Previous review status

Предыдущий corrective round в целом был полезным и большинство исходных проблем действительно закрыто.

| Previous finding | Current status |
|---|---|
| `REM-01` weak declaration→child capability check | закрыт основным механизмом; найден отдельный новый `keyed_by` projection defect ниже |
| `REM-02` mid-Commit reused-screen atomicity | реализован настоящий rollback, но follow-up audit выявил новые defects rollback model |
| `REM-03` partial Shell attach | predictable failure перенесён в Prepare; новый false-success в самом `AttachScreenToLayer` ниже |
| `REM-04` UI schema authority outside repository | сознательно deferred и теперь явно записан как `STATUS-008` |
| `REM-05` unusable `CollectionHost` | закрыт; `CollectionHost` получает `EntryWidgetClass`/`KeyPropertyName` и имеет empty-first-item test |
| `REM-06` Screen Field host docs drift | закрыт |
| `REM-07` legacy `ApplyOptional*` | закрыт |

`STATUS-008` не является finding этого review. Он остаётся отдельным зафиксированным known nonconformance и не должен случайно втягиваться в corrective work ниже.

## 3. Classification

**Implementation bug** — архитектурное правило уже определено правильно, но конкретный production code нарушает его.

**Architectural implementation gap** — текущая форма механизма не может надёжно обеспечить заявленный invariant без изменения самого механизма/API, а не одной условной ветки.

**Verification architecture defect** — runtime сегодня может работать правильно, но заявленный gate не способен гарантировать свойство, которое ему приписано; future regression может пройти gate.

## 4. Findings summary

| ID | Severity | Type | Summary |
|---|---:|---|---|
| `GBH-R1` | **P1** | Implementation bug | `AttachScreenToLayer()` возвращает success, если `UPanelWidget::AddChild()` вернул `nullptr` |
| `GBH-R2` | **P1** | Architectural implementation gap | rollback строится по current schema и неверно восстанавливает schema-switch |
| `GBH-R3` | **P1** | Implementation bug | higher-level rollback восстанавливает physical state, но не `LastCommittedProperties` |
| `GBH-R4` | **P2** | Architectural implementation gap | невозможность подготовить rollback только логируется и не блокирует transaction |
| `GBH-R5` | **P2** | Architectural lifecycle gap | `OnScreenFieldsApplied()` вызывается до окончательной публикации document transaction |
| `GBH-R6` | **P2** | Implementation bug | schema `keyed_by` не проецируется в `KeyPropertyName` capability descriptor |
| `GBH-R7` | **P2/P3** | Verification architecture defect | `sizeof(FGV2UiPropertyCapability)` не является completeness gate |

# 5. Findings

## GBH-R1 — P1 — Implementation bug

### `AttachScreenToLayer()` silently succeeds when `AddChild()` fails

Файл:

```text
Source/GV2/Private/UI/GV2GameShellWidgetBase.cpp
```

Символ:

```cpp
UGV2GameShellWidgetBase::AttachScreenToLayer
```

Текущая форма:

```cpp
UPanelSlot* NewSlot = Host->AddChild(ScreenWidget);
if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(NewSlot))
{
    OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
    OverlaySlot->SetVerticalAlignment(VAlign_Fill);
}
return true;
```

`UPanelWidget::AddChild()` может вернуть `nullptr`. В этом случае `AttachScreenToLayer()` всё равно возвращает `true`.

Это особенно критично после `GBH-10`: `CommitReconcile()` уже содержит recovery branch для:

```cpp
if (!Shell->AttachScreenToLayer(...))
{
    rollback...
}
```

и прямо рассматривает `AddChild` rejection как пример unpredictable engine-level attach failure. Но этот branch не сработает, если `AttachScreenToLayer()` скрывает сам failure.

### Failure shape

```text
old / unrelated Shell state
        ↓
Host->AddChild(NewScreen) == nullptr
        ↓
AttachScreenToLayer() == true
        ↓
CommitReconcile считает attach успешным
        ↓
ActiveScreens публикует NewScreen
        ↓
NewScreen физически отсутствует в Shell
```

Это тот же класс `logical success / physical absence`, который generic pipeline должен запрещать.

### Required outcome

- `AddChild() == nullptr` → `AttachScreenToLayer() == false`;
- `CommitReconcile()` получает failure и выполняет ADR-0041 recovery;
- `ActiveScreens`, prior widgets, prior metadata остаются previous revision;
- regression test должен проходить через production `CommitReconcile`, а не только напрямую вызывать helper.

---

## GBH-R2 — P1 — Architectural implementation gap

### Rollback is prepared using the new/current schema

Файлы:

```text
Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp
Source/GV2/Private/UI/GV2UiMutationPlan.cpp
Source/GV2/Public/UI/GV2UiPropertyHost.h
```

В `PrepareScreenFieldPlans()` previous value берётся из `LastCommittedProperties`, но rollback plan строится с:

```cpp
*Value.CompiledSchema
Value.SchemaId
```

то есть со schema **новой revision**.

Это неверно для schema-switch. Актуальный Screen Field contract специально разрешает `schema_id` приходить из runtime envelope per revision. Host хранит только `field_id`; schema не является частью Designer placement.

### Concrete failure

Previous revision:

```text
schema A owns:
  text
  percent
  enabled

previous values:
  text    = Old
  percent = 0.50
  enabled = true
```

Candidate revision:

```text
schema B owns:
  text
```

Forward plan правильно делает:

```text
Apply text = New
Reset percent
Reset enabled
```

Если после части этих mutations происходит Commit failure, rollback строится из previous values, но по schema B.

Для `percent` и `enabled` новая schema уже не является owner, поэтому обычный `PrepareUiHostProperties()` интерпретирует старое значение не как «restore previous», а как property, которую надо снова reset.

То есть current rollback algorithm путает две разные операции:

```text
apply previous revision
```

и:

```text
prepare previous value under current ownership
```

Они эквивалентны только если schema ownership не менялась.

### Root cause

Inverse operation должна определяться **forward mutation + previous committed state**, а не current schema.

Для каждой forward mutation:

```text
previous property exists:
    inverse = Apply(previous value)
previous property absent:
    inverse = Reset
```

Для `Apply(previous value)` сложных kinds (`CollectionHost`, `NestedScreen`) нужен previous compiled field spec. Следовательно committed host state должен сохранять не только property object, но и schema metadata previous revision.

### Required outcome

Нужен единый committed snapshot:

```text
previous prepared properties
previous compiled schema
previous schema_id
```

и inverse mutations должны строиться 1:1 с forward mutations.

Не должно быть второго schema-specific rollback framework.

---

## GBH-R3 — P1 — Implementation bug

### Higher-level rollback does not restore committed metadata

Файлы:

```text
Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp
Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp
```

Внутри одного `CommitScreenFields()` metadata продвигается только после успешного Commit всех field hosts. Поэтому нынешний Step K корректно проверяет:

```text
field A commits
field B fails
→ field A physical state restored
→ LastCommittedProperties remains OLD
```

Но document-level rollback имеет другую последовательность:

```text
Screen A CommitScreenFields() succeeds completely
    ↓
Screen A LastCommittedProperties = NEW
    ↓
Screen B CommitScreenFields() fails
    ↓
CommitReconcile calls RollbackFieldPlans(Screen A)
```

`RollbackFieldPlans()` физически replay-ит old values, но не возвращает `FGV2UiPropertyHostState::LastCommittedProperties`.

Получается:

```text
Screen A physical presentation = OLD
Screen A LastCommittedProperties = NEW
ActiveScreens = OLD
```

Это прямо нарушает ADR-0041: partial third state снова существует, просто теперь между physical presentation и committed metadata.

То же относится к rollback после attach failure, rollback уже полностью committed sibling screen и эквивалентным nested-screen границам.

### Required outcome

Rollback одного fully committed field plan обязан атомарно восстановить:

```text
physical properties
committed property snapshot
committed schema metadata
```

Тест должен использовать **два reused screens**, где A полностью commits, B падает, а не replacement/off-tree fixture.

---

## GBH-R4 — P2 — Architectural implementation gap

### Rollback preparation is optional even for a transaction that promises atomicity

Файлы:

```text
Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp
Source/GV2/Private/UI/GV2PropertyConsumers.cpp
```

При failure подготовки rollback plan current code пишет warning вида:

```text
a Commit failure ... will not be able to restore its previous state
```

и продолжает считать forward Prepare successful.

То же допущение есть для reused keyed collection item.

Это несовместимо с ADR-0041.

Если transaction заранее знает:

```text
forward valid
rollback unavailable
```

он не имеет права входить в live Commit phase, которая нормативно обещает:

```text
previous revision
OR
new revision
but never partial revision
```

### Required outcome

Rollback capability должна быть частью validity самого prepared plan.

Самый безопасный API-level вариант:

```text
FGV2UiHostMutationPlan
  ForwardMutations
  InverseMutations
```

вместо внешнего optional `RollbackPlan*`.

Так caller физически не сможет «забыть» rollback.

---

## GBH-R5 — P2 — Architectural lifecycle gap

### `OnScreenFieldsApplied()` fires inside a transaction that can still roll back

Файлы:

```text
Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp
Source/GV2/Public/UI/GV2ScreenWidgetBase.h
```

`CommitScreenFields()` после успешного per-screen commit вызывает:

```cpp
OnScreenFieldsApplied();
```

Но top-level document transaction ещё не обязательно опубликована.

После этого может произойти:

```text
Screen A Commit
    ↓
OnScreenFieldsApplied(A)
    ↓
Screen B Commit failure
    ↓
rollback Screen A properties
```

Blueprint event является arbitrary UE-side callback и его side effects не входят в rollback model.

Он может изменить focus, local visibility, animation, другое UE-local state или secondary presentation behavior.

### Required outcome

До реализации нужно через Unreal MCP проверить, существует ли хотя бы одна Blueprint implementation `OnScreenFieldsApplied`.

Допустимы два исхода.

**Если implementations отсутствуют:** удалить неиспользуемый event и call site; не вводить новую lifecycle phase без потребности.

**Если implementations существуют:**

- `CommitScreenFields()` больше не вызывает event;
- добавить explicit non-fallible post-publication finalize phase;
- top-level `CommitReconcile()` вызывает finalize только после успешной публикации всего document;
- nested screen finalize рекурсивно проходит через prepared consumers;
- standalone `ApplyScreenFields()` делает `Prepare → Commit → Finalize`.

Нельзя просто документировать, что Blueprint event «не должен иметь side effects»: это невозможно надёжно проверить.

---

## GBH-R6 — P2 — Implementation bug

### Schema `keyed_by` name is lost when projected to capability descriptor

Файл:

```text
Source/GV2/Private/UI/GV2UiCapability.cpp
```

Функция:

```cpp
ProjectSchemaFieldToCapability
```

Current projection для Array делает только:

```cpp
Required.bRequiresKeyedIdentity = FieldSpec.KeyedBy.has_value();
```

но не переносит:

```text
FieldSpec.KeyedBy
→
Required.KeyPropertyName
```

При этом default `FGV2UiPropertyCapability::KeyPropertyName` равен `"key"`.

### Failure shape

Schema:

```json5
{
  kind: "array",
  keyed_by: "id",
  items: {
    kind: "object",
    fields: {
      id: { kind: "key", required: true },
    },
  },
}
```

Widget capability:

```text
bRequiresKeyedIdentity = true
KeyPropertyName = "key"
```

Static compatibility ошибочно может принять этот contract. Для non-empty value failure проявится позже внутри collection consumer; для empty array несовместимость может вообще не быть замечена.

### Required outcome

Projection обязана делать:

```text
Required.bRequiresKeyedIdentity = true
Required.KeyPropertyName = actual schema keyed_by
```

Regression test должен использовать mismatch `"id"` vs `"key"` и static compatibility path, без зависимости от runtime items.

---

## GBH-R7 — P2/P3 — Verification architecture defect

### `sizeof(FGV2UiPropertyCapability)` is not a completeness gate

Файл:

```text
Source/GV2/Private/UI/GV2UiCapability.cpp
```

Current gate:

```cpp
static_assert(
    sizeof(FGV2UiPropertyCapability) == 192,
    "... classify the new/changed field ...");
```

Рядом находится ручная classification table.

Memory layout не доказывает completeness. Новый маленький field может попасть в padding и не изменить `sizeof`; field может быть заменён другим field того же размера; ABI/layout может измениться без изменения semantic contract.

### Current runtime impact

Немедленный runtime bug из этого сам по себе не следует: известные current constraints сейчас перечислены и сравниваются.

Проблема — false confidence и future regression protection.

### Required outcome

Удалить утверждение, что memory layout является completeness proof.

Заменить его механическим source-level inventory gate, который:

1. извлекает список data members `FGV2UiPropertyCapability`;
2. сравнивает его с explicit classification table;
3. падает на любом новом/удалённом member без classification;
4. имеет negative self-test, добавляющий synthetic unknown member;
5. выполняется через CTest.

Не нужно превращать `FGV2UiPropertyCapability` в новый reflection/runtime framework только ради этого gate.

# 6. Non-scope: STATUS-008

`STATUS-008` остаётся действующим known nonconformance:

```text
FGV2UiSchemaCache
!=
pinned repository/package closure
```

Corrective plan этого review **не должен** попутно переносить UI schema authority в repository.

Если во время работ появляется mod-owned UI schema, выполнение текущего plan надо остановить и открыть `STATUS-008` как отдельный проект.

# 7. Verification status of reviewed HEAD

Archive summary `GenericBoundaryHardening` фиксирует локальный прогон:

```text
106/106 UE Automation
68/68 portable CTest
validate_docs.py: 168 files
```

Это recorded evidence implementation round.

На проверенном HEAD GitHub не показывает combined status checks или workflow runs, поэтому это review не утверждает наличие independently verified CI для HEAD `d9280908...`.

# 8. Required closure criteria

Раунд нельзя считать закрытым, пока не доказаны все свойства ниже.

```text
1. AddChild rejection cannot become Attach success.
2. Schema-switch rollback restores previous ownership and values.
3. Rollback restores physical state AND committed metadata/schema.
4. A prepared live transaction cannot exist without inverse mutations.
5. Pre-publication Blueprint callbacks cannot escape rollback.
6. keyed_by name participates in static Schema ⊆ Capability check.
7. Adding a future capability member without classification turns a gate red.
```

Для `1…6` нужен red-on-old-code → green-on-fix regression.

Для `7` нужен negative self-test самого gate.

Полный green suite без доказательства этих danger points недостаточен.
