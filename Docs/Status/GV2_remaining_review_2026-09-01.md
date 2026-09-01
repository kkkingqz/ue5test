---
title: GV2 Remaining UI Pipeline Review 2026-09-01
status: informative
version: 1.0
updated: 2026-09-01
depends_on:
  - ImplementationStatus.md
  - ../ADR/0040-universal-ui-property-pipeline.md
---

# GV2 — ревью оставшихся проблем UI pipeline

> **Показывает:** внешнее ревью остаточных проблем UI pipeline на commit `15ae6c5c6c7097f5c3ff5fb310c194103f34a1f0`, как оно было получено 2026-09-01.
> **Не является нормативным:** подтверждённые находки ведёт план [GenericBoundaryHardening](../Plans/GenericBoundaryHardening/README.md); при расхождении источником считается он.

**Дата:** 2026-09-01  
**Проверенный HEAD:** `15ae6c5c6c7097f5c3ff5fb310c194103f34a1f0`  
**База предыдущего внешнего ревью:** `06b3bd220d67d4ff36e3ede102b6223a0573902f`

## Область ревью

В документ включены **только проблемы, которые остаются после `PipelineClosureCorrection` и `DataDrivenUiComposition`**. Закрытые пункты прошлого ревью (`UPP-R1`, `UPP-R3`, `UPP-R4`, `UPP-R6` и reset-часть `UPP-R7`) здесь не повторяются.

Классификация:

- **Баг реализации** — текущая реализация уже заявленного контракта допускает неправильное наблюдаемое поведение; исправляется без изменения фундаментальной модели.
- **Архитектурная недоработка** — текущей модели или публичному контракту не хватает состояния/механизма, поэтому корректное поведение нельзя гарантировать локальным point-fix.
- **Документационный / teardown debt** — runtime-поведение напрямую не ломается, но нормативный текст или остаточный API расходится с фактической архитектурой.

---

# 1. `DeclaredComposite` проверяет child capability только по `kind`

**Приоритет:** P1  
**Классификация:** **баг реализации с архитектурным корнем**  
**Основной владелец:** `DeclaredComposite` / capability compatibility

## Проблема

`DUC-07` был введён как независимая проверка между:

1. capability, объявленной composite в Designer;
2. реальными capability дочернего widget.

Но текущая проверка `DoesCapabilityTreeSupportKind(...)` сравнивает только:

```cpp
Entry.Value.SupportedKind == DeclaredKind
```

Файл:

```text
Source/GV2/Private/UI/GV2UiCapability.cpp
```

При этом capability содержит больше семантики, чем один `kind`:

- `NumberMin` / `NumberMax`;
- `IntMin` / `IntMax`;
- `TargetKind` для `StableId` / `ref`;
- требования keyed identity для коллекций;
- target/consumer-specific ограничения.

### Подтверждённый пример

`UGV2ProgressBarWidgetBase` объявляет:

```cpp
OutBuilder.AddNumber(
    TEXT("percent"),
    FName(TEXT("ProgressBar")),
    0.0,
    1.0);
```

Файл:

```text
Source/GV2/Private/UI/GV2ProgressBarWidgetBase.cpp
```

Но `UGV2DeclaredCompositeWidgetBase` для Designer-kind `Number` строит:

```cpp
OutBuilder.AddNumber(PropertyName, ChildWidgetName);
```

без диапазона.

Файл:

```text
Source/GV2/Private/UI/GV2DeclaredCompositeWidgetBase.cpp
```

Текущая fixture-schema также объявляет `value` просто как `number`, без `min/max`:

```text
GameData/textsystem/schemas/ui_field_declared_composite_fixture_v1.schema.json5
```

В результате цепочка допускает контракт:

```text
schema:            Number, unbounded
DeclaredComposite: Number, unbounded
ProgressBar child: Number [0..1]
```

`DoesCapabilityTreeSupportKind()` видит только совпадение `Number` и принимает mapping.

Дополнительно `FGV2NumberPropertyConsumer::Prepare()` сейчас не восстанавливает потерянное ограничение child capability: после проверки типа он просто сохраняет значение.

Следствие: значение вроде `5.0` может пройти schema/preflight, хотя физический `ProgressBar` способен представить только `[0..1]` и позднее clamp-нет значение.

## Почему это баг

Архитектурный принцип уже принят: `SchemaContract ⊆ WidgetCapabilities`. `DUC-07` должен был обеспечить независимую проверку этого свойства для declared composite.

Проверка существует, но реализует более слабое условие:

```text
same kind
```

вместо:

```text
composite capability ⊆ selected child capability
```

То есть это прежде всего дефект реализации принятого контракта.

## Архитектурный корень

У declaration сейчас нет явного понятия **какую именно capability ребёнка делегирует property**. Хранятся только:

```text
PropertyName
ChildWidgetName
Kind
```

Поэтому полная сверка child semantics становится неоднозначной, если child объявляет несколько capability одного kind.

## Рекомендация

Заменить `DoesCapabilityTreeSupportKind()` на полную совместимость capability.

Предпочтительная модель declaration:

```text
PropertyName
ChildWidgetName
ChildCapabilityName
DeclaredKind / declared constraints
```

После чего проверять:

```text
DeclaredCapability ⊆ ChildCapability
```

как минимум по:

- kind;
- integer/number range;
- ref `target_kind`;
- keyed identity;
- другим обязательным constraint-полям capability.

Важно: **не выводить declaration автоматически из child capability**, иначе снова появится проблема `UPP-R1` — контракт будет проверяться против самого себя.

## Обязательный regression test

```text
DeclaredComposite Number
    -> ProgressBar.percent [0..1]

schema Number [0..100]
```

Ожидается:

```text
Prepare == false
```

до любой physical mutation.

Также нужен отдельный positive case:

```text
schema Number [0..1]
-> accepted
```

---

# 2. Commit reused live screen остаётся неатомарным при mid-Commit failure

**Приоритет:** P1  
**Классификация:** **архитектурная недоработка**  
**Связь с прошлым ревью:** остаток `UPP-R2`

## Что уже исправлено

`FGV2LayeredUiReconciler::CommitReconcile()` теперь сначала commit-ит mutation plans всех экранов и только после этого начинает менять Shell tree и `ActiveScreens`.

Файл:

```text
Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp
```

Это устранило прежнюю document-level проблему, когда один слой уже заменялся физически до отказа commit другого слоя.

## Оставшаяся проблема

`CommitUiHostProperties()` последовательно выполняет mutations live widget:

```text
mutation A -> Commit
mutation B -> Commit
mutation C -> Commit
```

При failure на `B` или `C` функция возвращает `false`, но **undo/rollback уже выполненных mutations отсутствует**.

Файл:

```text
Source/GV2/Private/UI/GV2UiMutationPlan.cpp
```

`UGV2ScreenWidgetBase::CommitScreenFields()` также не имеет compensating state и прямо исходит из предположения, что commit prepared plan должен быть infallible-style.

Файл:

```text
Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp
```

Для нового off-tree candidate это обычно безопасно: частично изменённый widget ещё не опубликован.

Но при **reuse существующего screen instance** target widget уже является live previous revision.

Сценарий:

```text
previous revision active

A.Commit() -> success, live renderer изменён
B.Commit() -> injected / engine-level failure

CommitScreenFields() -> false
CommitReconcile() -> false
ActiveScreens -> остаётся previous revision
```

Однако physical state `A` уже относится к новой revision.

Получается логически старая revision с физически частично новым UI.

## Почему это архитектурная недоработка

Локальной проверки перед `Commit()` недостаточно: сам класс failure существует именно **после начала необратимого применения**.

Чтобы реально гарантировать contract:

> previous revision remains active after unexpected Commit failure

нужен один из архитектурных механизмов:

1. rollback/undo каждой mutation;
2. commit в shadow/off-tree presentation state с последующим swap;
3. другой механизм транзакционного commit.

Текущий mutation plan не содержит предыдущего physical state и не предоставляет `Rollback()`.

Это отсутствующая возможность модели, а не отдельный неправильный `if`.

## Рекомендация

Минимально инвазивный вариант:

```text
FGV2UiPropertyMutation
    PreparedNewValue
    PreviousCommittedValue / rollback payload
    Commit()
    Rollback()
```

`CommitUiHostProperties()`:

```text
committed = []

for mutation:
    if Commit fails:
        rollback(committed in reverse order)
        return false
    committed.push(mutation)
```

Rollback должен восстанавливать **physical renderer state**, а не только `LastCommittedProperties`.

## Обязательный regression test

Нужен тест именно на **reused live screen**, а не replacement widget:

```text
revision 1:
    property A = old-A
    property B = old-B

revision 2:
    property A = new-A
    property B = new-B

inject failure on B after A committed
```

После отказа проверить:

```text
physical A == old-A
physical B == old-B
LastCommittedProperties == revision 1
ActiveScreens == revision 1
bindings/revision == revision 1
```

Существующие PCC/DUC tests такого общего свойства не доказывают.

---

# 3. Attach нескольких screens всё ещё может частично изменить Shell tree

**Приоритет:** P2  
**Классификация:** **баг реализации**  
**Основной владелец:** `FGV2LayeredUiReconciler`

## Проблема

После успешного commit всех screen plans `CommitReconcile()` последовательно делает:

```text
Attach screen A
Attach screen B
Attach screen C
```

Если `A` успешно attached, а `B` возвращает `false`, функция завершится с ошибкой, не публикуя новый `ActiveScreens`, но `A` уже физически находится в Shell tree.

Этот residual risk прямо отмечен комментарием в текущем `GV2LayeredUiReconciler.cpp`.

Основная причина `AttachScreenToLayer()` failure — structural misconfiguration Shell, например отсутствующий authored layer host.

При этом `UGV2GameShellWidgetBase` уже имеет API:

```cpp
bool HasHostForLayer(FName Layer) const;
```

Файл:

```text
Source/GV2/Public/UI/GV2GameShellWidgetBase.h
```

## Почему это баг, а не архитектурная недоработка

Необходимая модель уже существует:

- список incoming layers известен в `PrepareReconcile()`;
- Shell известен;
- наличие host можно проверить без mutation;
- failure предсказуем до Commit.

То есть проблема устраняется нормальным preflight и не требует нового transactional abstraction.

## Рекомендация

В `PrepareReconcile()` до создания publish plan проверить для каждого incoming screen:

```text
if Shell != nullptr:
    IsValidLayerName(layer)
    HasHostForLayer(layer)
```

Missing host должен давать typed Prepare error.

После successful Prepare `AttachScreenToLayer()` должен считаться invariant-level operation, а не обычным content-driven failure.

Дополнительно желательно проверить, что `AddChild()` реально вернул slot, если этот failure path возможен.

## Regression test

Shell fixture:

```text
layer A host exists
layer B host missing
```

Document содержит A+B.

Ожидание:

```text
PrepareReconcile == false
Shell tree полностью прежний
CommitReconcile не запускается
```

---

# 4. UI schema authority всё ещё отделена от active repository/package closure

**Приоритет:** P1 архитектурно / P2 для текущего content set  
**Классификация:** **архитектурная недоработка**  
**Связь с прошлым ревью:** `UPP-R5`

## Проблема

`GV2ScreenFieldMaterializer` использует собственный static schema cache с filesystem roots:

```text
GameData/core
GameData/textsystem
GameData/rh
GameData/sample
```

Файл:

```text
Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp
```

Таким образом UI schema resolution не является projection текущего:

```text
active GameDataRepository
pinned package closure
accepted/rejected mod set
session repository revision
```

Следствия после появления UI-схем в модах:

- схема физически присутствует на диске, но package может быть неактивен;
- схема rejected mod всё ещё потенциально видима отдельному cache;
- UI resolver и gameplay repository могут иметь разные представления package closure;
- невозможно строго реализовать правило «bad mod schema rejects mod, not session» через один authoritative repository build.

## Почему это архитектурная недоработка

Проблема не в конкретном пути поиска файла. Нужен единый owner schema lifetime и package closure.

Текущий archive `DataDrivenUiComposition` сознательно отложил этот пункт до момента, когда UI-блоки начнут поставляться модами. Для текущего проектного content это допустимый временный scope decision, но архитектурная гарантия ещё не реализована.

## Дополнительное расхождение contract/status

Нормативные документы уже утверждают, что:

- repository владеет UI schemas;
- мод может поставлять schema стандартных kinds;
- несовместимая schema мода отбраковывает мод, а session продолжает работу.

Но `Docs/Status/ImplementationStatus.md` этот gap сейчас не показывает.

То есть deferred capability одновременно выглядит как уже реализованный normative invariant.

## Рекомендация

Целевое решение:

```text
GameDataRepository build
    -> compiled UI schemas входят в repository snapshot
    -> materializer получает schema resolver/read handle из текущего repository
```

Никакого второго filesystem-owned schema authority в runtime presentation path.

До реализации нужно как минимум сделать contract честным:

- добавить отдельный `STATUS-*`;
- явно отметить, что mod-owned UI schema closure ещё не поддерживается production runtime.

## Regression tests после реализации

1. schema из inactive package не разрешается;
2. schema rejected mod не видна materializer;
3. valid mod schema доступна только при active package;
4. core/session продолжают запуск при rejection только плохого mod UI package;
5. repository revision и schema resolver используют один pinned snapshot.

---

# 5. `DeclaredComposite.CollectionHost` объявлен как поддерживаемый Designer-kind, но declaration недостаточно для создания первого элемента

**Приоритет:** P2  
**Классификация:** **архитектурная недоработка публичной surface**

## Проблема

`EGV2DeclaredUiCapabilityKind` публично предлагает:

```text
CollectionHost
```

Файл:

```text
Source/GV2/Public/UI/GV2DeclaredCompositeWidgetBase.h
```

Однако одна declaration содержит только:

```text
PropertyName
ChildWidgetName
Kind
```

Для `CollectionHost` composite создаёт generic capability без:

- `EntryWidgetClass`;
- item capability tree;
- явного key field contract.

Файл:

```text
Source/GV2/Private/UI/GV2DeclaredCompositeWidgetBase.cpp
```

`FGV2KeyedCollectionPropertyConsumer` для нового item должен получить class элемента.

Если `Capability.EntryWidgetClass` отсутствует, consumer пытается вывести class из:

1. уже существующих active entries;
2. первого existing child в panel.

На действительно пустой коллекции оба источника отсутствуют, и Prepare возвращает:

```text
core:diagnostic.ui_consumer.missing_entry_class
```

Сам `UGV2ListViewWidgetBase` отдельного Designer `EntryWidgetClass` сейчас не хранит.

## Почему это архитектурная недоработка

Публичный declaration format просто не содержит всей информации, необходимой для generic collection creation.

Добавлением локального `if` это не исправить: нужно решить, **кому принадлежит class item presentation**.

## Рекомендация

До принятия полного контракта один из двух вариантов.

### Вариант A — убрать неподдерживаемую surface

Удалить `CollectionHost` из `EGV2DeclaredUiCapabilityKind` до отдельной задачи generic declared collections.

Это наиболее чистый вариант, если текущая цель declared composite — плоская композиция существующих leaf controls.

### Вариант B — расширить declaration

Например:

```text
Kind = CollectionHost
ChildWidgetName
EntryWidgetClass
KeyPropertyName
```

При этом item schema должна по-прежнему приходить из repository compiled schema, а `EntryWidgetClass` лишь задаёт presentation implementation.

После этого обязательно применять recursive `Schema.Items ⊆ EntryWidgetCapabilities`.

## Regression test

Declared composite с пустым ListView и одним incoming item должен успешно создать **первый** entry без предварительно authored dummy child.

---

# 6. Normative contract всё ещё описывает top-level Screen Field policy, которой нет в текущем API

**Приоритет:** P3  
**Классификация:** **документационный debt**

## Проблема

`Docs/UI/ScreenTemplates.md` всё ещё утверждает, что каждый configured element объявляет:

```text
schema_id
required/optional policy
```

Фактический `IGV2ScreenFieldHost` содержит только:

```cpp
virtual FName GetScreenFieldId() const = 0;
```

Файл:

```text
Source/GV2/Public/UI/GV2ScreenFieldHost.h
```

`schema_id` приходит из runtime envelope, а `PrepareScreenFieldPlans()` сейчас требует value для каждого configured host.

Отдельной top-level `optional host` policy в интерфейсе нет.

## Рекомендация

Если top-level Screen Field фактически всегда required — переписать normative wording под текущую bijection:

```text
every configured host must have one incoming field envelope
```

Если optional top-level host действительно нужен — тогда это уже отдельная архитектурная задача, а не docs fix.

---

# 7. Остались legacy optional apply API, которые ADR-0040 объявлял удаляемыми

**Приоритет:** P3  
**Классификация:** **teardown debt**

В публичных widget API всё ещё присутствуют:

```cpp
UGV2ImageWidgetBase::ApplyOptionalImageResource(...)
UGV2PortraitWidgetBase::ApplyOptionalPortrait(...)
```

Файлы:

```text
Source/GV2/Public/UI/GV2ImageWidgetBase.h
Source/GV2/Public/UI/GV2PortraitWidgetBase.h
```

ADR-0040 определял placeholder/fallback policy как свойство schema/presentation contract и ожидал удаления параллельных optional apply paths.

Если production generic pipeline их не использует, это не runtime defect, но лишняя публичная поверхность повышает риск появления второго presentation path в будущем.

## Рекомендация

Проверить call sites и, если production authority отсутствует, удалить эти методы вместе с тестами/документацией старого пути.

---

# Итоговая таблица

| ID | Приоритет | Тип | Кратко |
|---|---:|---|---|
| `REM-01` | P1 | **Баг реализации + архитектурный корень** | DeclaredComposite проверяет child только по `kind`, теряя range/target constraints |
| `REM-02` | P1 | **Архитектурная недоработка** | Нет rollback для уже committed mutations reused live screen |
| `REM-03` | P2 | **Баг реализации** | Attach нескольких screens может частично изменить Shell до failure |
| `REM-04` | P1/P2 | **Архитектурная недоработка** | UI schema cache не является частью active repository/package closure (`UPP-R5`) |
| `REM-05` | P2 | **Архитектурная недоработка** | Declared `CollectionHost` не содержит entry-class/item presentation contract |
| `REM-06` | P3 | **Документационный debt** | ScreenTemplates описывает schema_id/optional host policy, которой нет в host API |
| `REM-07` | P3 | **Teardown debt** | Остались legacy `ApplyOptional*` presentation APIs |

## Что исправлять первым

Порядок с точки зрения сохранения инвариантов:

1. `REM-01` — не допускать ложную capability compatibility в новом declared mechanism.
2. `REM-02` — довести Commit failure semantics до реально атомарного поведения на reused screen.
3. `REM-03` — перевести structural Shell failures в Prepare.
4. `REM-04` — до появления mod-owned UI blocks либо реализовать repository-owned schemas, либо официально зафиксировать gap.
5. `REM-05` — убрать или завершить declared collection surface.
6. `REM-06` / `REM-07` — синхронизация contract и teardown.

## Общая оценка

Основной generic pipeline после corrective work существенно сильнее исходного: прежние повторяющиеся silent-loss paths в collection materialization, второй validator, `screen_fields` и capability-query mutation действительно устранены на уровне механизма.

Оставшиеся критичные риски теперь сосредоточены не в schema-specific адаптерах, а в **границах generic abstractions**:

```text
Declared capability -> actual child capability
Prepared mutation   -> transactional live commit
Filesystem schemas  -> repository/package authority
```

Именно эти три границы стоит считать следующим архитектурным hardening round.
