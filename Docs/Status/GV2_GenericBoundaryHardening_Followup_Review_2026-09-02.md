---
title: GV2 Generic Boundary Hardening Follow-up Review 2026-09-02
status: informative
version: 1.1
updated: 2026-09-03
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

# 9. GBF-08 independent closure audit

> **Показывает:** независимую сверку закрытий на `453eb30a1fb126bbd4f2424b6f4d9c85353fc6c8`, проведённую 2026-09-03. Исходный отчёт и полный вывод команд находятся в `.superpowers/sdd/LifecycleAndClosure/task-1-report.md` и не являются источником нормативных правил.

Все outcomes ниже закрывают finding как класс, а не единственный fixture. Формулировка «assertion станет red при revert» означает проверяемую связь current production path с уже выполненным UE-тестом; независимая сверка **не** подменяет это фиктивной перекомпиляцией временно откаченного C++. Отдельно выполненные negative self-test структурных Python gates действительно подставляют дефектную форму и требуют её rejection.

## GBH-R1 — устранён

`UGV2GameShellWidgetBase::AttachScreenToLayer` теперь возвращает `false` для `Host->AddChild(...) == nullptr` (`GV2GameShellWidgetBase.cpp`). `GV2.UI.LayeredReconciliationContract` проходит production `CommitReconcile` через занятый `USizeBox` и проверяет failed commit, tree, previous screen metadata и неизменённый `ActiveScreens`; возврат старой success-ветки делает его `TestFalse` red. `validate_shell_attach_failure_consumption.py` перечисляет каждый `->AddChild(...)` в body `AttachScreenToLayer`; его executed self-test удаляет null-propagation и discard-ит result, и gate их отклоняет. Текущее множество содержит один вызов `Host->AddChild`.

## GBH-R2 — устранён

Committed tuple теперь включает value, compiled schema и schema ID; `PrepareUiHostRollbackPlan` строит inverse от previous schema/value либо reset candidate-only property (`GV2ScreenWidgetBase.cpp`, `GV2UiMutationPlan.cpp`, reused collection item в `GV2PropertyConsumers.cpp`). `GV2.UI.LayeredReconciliationContract` делает schema A→B switch, вводит failure после первой mutation и проверяет возврат обоих значений формы A; откат к current-schema preparation делает эти assertions red. `GetUiMutationKindsRequiringInverse()` перечисляет direct mutation kinds, а `GV2.UI.PrepareCommitAndFailureInjection` удаляет inverse по очереди для каждого kind. Screen host и reused keyed item — оба production builders generic inverse; nested tab screen делегирует `RollbackFieldPlans`.

## GBH-R3 — устранён

`RollbackFieldPlans` replay-ит inverse и только после physical success вызывает `RestoreCommittedSnapshot`; document, keyed collection и nested-tab boundaries используют тот же путь (`GV2ScreenWidgetBase.cpp`, `GV2LayeredUiReconciler.cpp`, `GV2PropertyConsumers.cpp`). `GV2.UI.LayeredReconciliationContract` проверяет reused two-screen rollback, schema ID, absent candidate-only property и next Prepare; он также проверяет nested child. `GV2.UI.StandardPropertyConsumers` проверяет reused keyed item. Удаление restore accounting делает эти metadata/next-Prepare assertions red. Перечислитель rollback boundaries содержит property, screen fields, document, keyed collection, nested tabs и Shell attach; Shell использует тот же `RollbackFieldPlans` recovery.

## GBH-R4 — устранён

Prepare отклоняет non-empty committed value без schema snapshot, inverse preparation failure и plan mismatch; та же hard-failure логика есть у reused collection item. `GV2.UI.LayeredReconciliationContract` ожидает `core:diagnostic.ui_rollback.missing_committed_schema`; `GV2.UI.PrepareCommitAndFailureInjection` перебирает `GetUiMutationKindsRequiringInverse()` и ожидает `core:diagnostic.ui_rollback.plan_mismatch` после удаления inverse. Возврат warning-and-continue делает эти assertions red. Два production inverse builders — screen host и reused keyed item — найдены source search; единственный UI call without rollback plan является one-property test-only observability probe, не live multi-mutation transaction.

## GBH-R5 — устранён

`OnScreenFieldsApplied`, `OnTabModelApplied`, `OnTabSelectionUpdated` и `OnTabChanged` удалены из screen/tab API и Commit path. `GV2.UI.LayeredReconciliationContract` перечисляет filesystem `.uasset` под `Content`, сверяет их с Asset Registry и инспектирует generated class каждого Widget Blueprint; он требует zero implementers и null reflected base callback surface. Возврат UFUNCTION/callback делает reflection assertions red; новый Blueprint implementation не может скрыться вне ручного списка assets. Это полный enumerator применимых content assets, а не проверка известных трёх Blueprint.

## GBH-R6 — устранён

`ProjectSchemaFieldToCapability` переносит actual `FieldSpec.KeyedBy` в `KeyPropertyName`, а shared subset rule сравнивает это имя (`GV2UiCapability.cpp`). `GV2.UI.PropertyHostAndCapabilities` подаёт `keyed_by: "id"` против widget `key` без items, ожидает `key_property_mismatch` и обе строки в diagnostic; flag-only projection делает `TestFalse` red. Единый schema-side projection перечисляет array keyed identity, а independent member inventory требует classification `KeyPropertyName` как `KeyPropertyMismatch`.

## GBH-R7 — устранён

`sizeof(FGV2UiPropertyCapability)` больше не является proof. CTest gates `ui_capability_member_inventory_contract` и `_negative_contract` извлекают каждый data member public struct и сравнивают с independent `CLASSIFIED_MEMBERS`. Executed self-test добавляет unknown member и удаляет `PropertyName`, требуя failure в обоих случаях. Это enumerates all current 15 members и не зависит от ABI padding.

## Общая проверка модели отката

`validate_ui_rollback_boundaries.py` выполняет source-derived inventory definitions `Commit*`/`AttachScreenToLayer`, требует marker, one-to-one `EGV2UiRollbackBoundary` и executable recovery classification; executed self-test вставляет synthetic root/leaf, удаляет real marker и recovery case. По построению он не распознаёт mutation path вне этой naming grammar и не доказывает тело recovery; новый такой путь обязан расширить gate и получить runtime fault-injection test в том же change set.

Ни одна repair не сужает [ADR-0041](../ADR/0041-ui-commit-rollback-model.md): Decision 7 по-прежнему запрещает partial state. Physical recovery остаётся replay обычного Prepare/Commit через generic `PrepareUiHostRollbackPlan` и `RollbackFieldPlans`; schema-specific rollback DTO/consumer/framework не введён. `ui_pipeline_legacy_gate_contract` и его executed negative self-test запрещают новый schema-specific Prepare/Build, payload или DTO surface.

## STATUS-008 semantic closure check

`STATUS-008` не является finding этого review и остаётся `known_nonconformance`. Параллельные commits `fe3cfdf`/`ade64db` изменили только evidence и linked DCA closure tasks: ID, state, normative requirement и verbatim reopening condition не менялись. Current `GameData` содержит 12 `ui_field` schemas в `core`, 5 в `textsystem`, ноль `ui_value` и ноль UI schemas в `rh`/`sample`; mod-owned `ui_field`/`ui_value` schema не появилась. Поэтому reopening condition не наступил по semantic interpretation controller.

## Executed audit gates

`ctest --test-dir build --output-on-failure` завершился 74/74; `gv2-headless --check-scripts` завершился `ok=true`, `modules_checked=42`; `validate_docs.py` прошёл. В текущем `libUnrealEditor-GV2.so` `Automation RunTests GV2.UI` обнаружил и завершил success все 16 tests, включая `LayeredReconciliationContract`, `PrepareCommitAndFailureInjection`, `PropertyHostAndCapabilities` и `StandardPropertyConsumers`. Отдельно прошли current и negative-self-test варианты shell-attach, capability-member-inventory, rollback-boundary-inventory и UI legacy gates.
