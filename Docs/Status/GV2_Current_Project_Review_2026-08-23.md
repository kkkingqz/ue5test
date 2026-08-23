---
title: GV2 Current Project Review 2026-08-23
status: informative
version: 1.0
updated: 2026-08-23
depends_on:
  - ImplementationStatus.md
---

# GV2 — повторное ревью актуального состояния

> **Показывает:** внешнее ревью состояния репозитория на commit `93bc1e1a11890189188c4b554174f3d39f3795a7` от 2026-08-23, как оно было получено.
> **Не является нормативным:** проверенные находки перенесены в [Implementation Status](ImplementationStatus.md) и [Boundary and Apply Integrity](../Plans/BoundaryAndApplyIntegrity/README.md); при расхождении источником считаются они.

## 1. Резюме

Проверен актуальный `main` репозитория `kkkingqz/ue5test`.

Текущий HEAD:

```text
93bc1e1a11890189188c4b554174f3d39f3795a7
```

Это **тот же commit, который был HEAD во время предыдущего ревью**. После
предыдущего ревью новых commits в `main` нет.

Следовательно:

- новых регрессий, **внесённых после предыдущего ревью**, обнаружиться не могло;
- перечисленные ниже `NEW-*` — это **дополнительные дефекты, которые существовали
  уже на том же HEAD, но не были отмечены в предыдущем проходе**;
- ранее найденные и всё ещё актуальные проблемы перечислены отдельно как
  `OPEN-*`;
- известные архитектурные gaps из status-документов перечислены отдельно и не
  смешиваются с newly discovered bugs.

### Итог по важности

На текущем HEAD наиболее важны четыре проблемы:

1. `OPEN-01` — Core Repeater всё ещё не transactional по состоянию reused
   widgets.
2. `NEW-02` — Location composites могут сообщить успешный apply при
   отсутствующем widget class и фактически не отрисовать repeated content.
3. `NEW-03` — PlayerStatus items/effects заявлены выполненными, но не проходят
   через portable Lua→C++ Screen Field adapter.
4. `OPEN-02` — test, который заявлен как actual-consumer font verification,
   всё ещё проверяет central resolver, а не фактически настроенные widgets.

Дополнительно найдены validation/test/documentation расхождения.

---

# 2. Метод проверки и ограничения

Проверялись:

- последние commits и текущий HEAD;
- Core Repeater:
  - `FGV2KeyedCollection`;
  - `UGV2ListViewWidgetBase`;
  - реальные LocationScreen consumers;
- LocationScreen composites:
  - PlayerStatus;
  - Scene;
  - CommandPanel;
  - screen-level apply/rollback;
- `FGV2ScreenFieldAdapterRegistry`;
- current Screen Field / repeated identity contracts;
- viewport и typography verification tests;
- `AuditFindings.md`;
- `ImplementationStatus.md`;
- архивы завершённых планов.

Проверка выполнена по текущему содержимому GitHub repository.

## Ограничение verification

На HEAD `93bc1e1a...` GitHub не показывает status checks / workflow runs.
В документации репозитория записан успешный локальный прогон:

```text
66/66 portable CTest
81/81 GV2.* UE automation
gv2-headless --self-test
gv2-headless --check-scripts
validate_docs.py
clean GV2Editor build
```

Но в рамках этого ревью эти команды **не запускались заново локально**.
Поэтому документ отличает доказуемый из source дефект от утверждения о свежем
полном test/build gate.

---

# 3. Новые проблемы, найденные этим проходом

## NEW-01 — `BlueprintPure` getters имеют скрытые side effects

**Приоритет:** P2 / medium  
**Тип:** API semantics / test reliability / lifecycle

### Где

`Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`

Методы объявлены как `BlueprintPure`:

```cpp
UGV2ListViewWidgetBase* GetItemRepeater() { return ResolveItemRepeater(); }
UGV2ListViewWidgetBase* GetEffectRepeater() { return ResolveEffectRepeater(); }
UGV2ListViewWidgetBase* GetMeterRepeater() { return ResolveMeterRepeater(); }
UGV2ListViewWidgetBase* GetCharacterRepeater() { return ResolveCharacterRepeater(); }
UGV2ListViewWidgetBase* GetRepeater() { return ResolveRepeater(); }
```

Но соответствующие `Resolve*()` в
`Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp` могут:

```cpp
Internal...Repeater = NewObject<UGV2ListViewWidgetBase>(this);
Internal...Repeater->SetContainerPanel(...);
```

То есть read-like / pure accessor:

- создаёт UObject;
- меняет internal state;
- привязывает container.

### Почему это проблема

`BlueprintPure` воспринимается как чтение значения без observable mutation.
Blueprint/UMG может вычислять pure expressions повторно и не в том lifecycle
порядке, который ожидает автор кода.

Diagnostic/test code, который хочет только проверить существующий Repeater,
может **сам создать Repeater и изменить проверяемый объект**.

### Ожидаемая коррекция

Разделить:

```text
GetExistingRepeater() const      // только чтение
EnsureRepeater()/ResolveRepeater // допускает создание
```

или убрать `BlueprintPure` с lazy initializer и выполнять initialization в
явном lifecycle/apply path.

---

## NEW-02 — repeated content может быть принят без реального widget class

**Приоритет:** P1 / high  
**Тип:** runtime correctness / silent presentation loss

### PlayerStatus / Scene

В `Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp` повторяется
pattern:

```cpp
if (UGV2ListViewWidgetBase* MeterRep = ResolveMeterRepeater())
{
    const TSubclassOf<UGV2ProgressBarWidgetBase> Class =
        ResolveMeterWidgetClass();

    if (Class != nullptr)
    {
        // reconcile
    }
}

Applied = Candidate;
return true;
```

Для item/effect icons и Scene characters логика аналогична.

Если host существует, candidate непустой, но visual child class не удалось
resolve/load, reconciliation просто пропускается, после чего composite сохраняет
`Applied = Candidate` и возвращает `true`.

### CommandPanel

При отсутствующем `ButtonWidgetClass` creator делает:

```cpp
if (Class == nullptr)
{
    return NewObject<UGV2ButtonWidgetBase>(this);
}
```

Вместо явного отказа создаётся bare native base widget без гарантии authored
WidgetTree/label/layout.

### Риск

Ошибка cook, rename asset, broken path или неполная конфигурация может дать:

```text
presentation apply == success
```

при отсутствующем или неполноценном визуальном содержимом.

### Ожидаемая коррекция

Для non-empty repeated content отсутствие корректного child class должно быть
validation/apply failure. Bare native visual fallback для обязательного
renderer не использовать.

---

## NEW-03 — PlayerStatus items/effects не подключены к portable Screen Field boundary

**Приоритет:** P1 / high  
**Тип:** incomplete implementation / contract drift

### Что заявлено выполненным

Исторический LocationScreen plan:

- `GLS-06`: PlayerStatus показывает portrait, name, meters, active items и
  effects.
- `GLS-03`: commands, item icons и effect icons являются repeated elements и
  подчиняются общему key contract.

### Что есть в C++

`FGV2LocationPlayerStatusViewModel`:

```cpp
TArray<FGV2LocationMeterEntry> Meters;
TArray<FString> ItemIconResourceIds;
TArray<FString> EffectIconResourceIds;
```

`UGV2LocationPlayerStatusWidgetBase` имеет item/effect repeaters.

### Чего нет в adapter

В текущем
`Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp`
`PrepareLocationPlayerStatus` / `BuildLocationPlayerStatus` обрабатывают:

- `name`;
- optional portrait;
- meters.

В source нет обработки items/effects или
`ItemIconResourceIds` / `EffectIconResourceIds`.

Следовательно normal path:

```text
Lua / portable FScreenField
    ->
FGV2ScreenFieldAdapterRegistry
    ->
FGV2LocationPlayerStatusViewModel
```

не способен заполнить эти коллекции.

### Identity тоже остаётся неправильной

При ручном заполнении C++ model Repeater использует `ResourceId` как key:

```cpp
[](const FString& ResourceId)
{
    return FName(*ResourceId);
}
```

Это не разделяет stable element identity и текущий visual resource.

### Contract drift

Сейчас существуют четыре разных описания поля:

```text
completed plan            -> name + portrait + meters + items + effects
C++ model/composite       -> name + portrait + meters + items + effects
portable adapter          -> name + portrait + meters
ScreenTemplates summary   -> name + portrait
```

### Вывод

Либо items/effects нужно довести до полного keyed boundary:

```text
{ key, resource_id }
```

либо официально снять их из реализованного baseline и удалить dead/overclaimed
paths.

---

## NEW-04 — repeated-key grammar противоречит собственным примерам и реализации

**Приоритет:** P2 / medium  
**Тип:** contract ambiguity / validation drift

`Docs/UI/UIDocumentAndReconciliation.md` формально требует:

```text
[a-z0-9_.-]+
```

Но тот же раздел разрешает источники key:

```text
rh:item.weapon.iron_sword
actor@42
```

Первый требует `:`, второй — `@`.

`IsValidRepeatedElementKey()` в
`GV2ScreenFieldAdapterRegistry.cpp` разрешает оба символа:

```text
_
-
.
@
:
```

Поэтому формальная grammar и фактическая реализация расходятся.

Нужно выбрать одну grammar и использовать её одновременно в contract, C++,
Lua validation и conformance tests.

---

## NEW-05 — 720p verification слабее заявленного acceptance

**Приоритет:** P2 / medium  
**Тип:** verification gap

`GV2.Runtime.UI.LocationScreenViewportMatrix` теперь действительно использует
arranged geometry, но:

### Короткий fixture

Кнопки имеют текст:

```cpp
"Command #%d"
```

То есть реальный long-text / pseudolocale stress case отсутствует.

### Bounds проверены не полностью

Для 1280×720 проверяется:

```text
BtnSize.X > 0
BtnSize.Y > 0
BtnLocalPos.Y + BtnSize.Y <= 720
```

Не проверяется:

```text
x >= 0
y >= 0
x + width <= 1280
button rect внутри CommandPanel rect
```

Horizontal overflow может остаться зелёным.

### Вывод

Проблема старого `DesiredSize`-test исправлена, но claim «mandatory controls не
обрезаются на 720p» защищён не полностью.

---

## NEW-06 — `AuditFindings.md` содержит устаревший current-state paragraph

**Приоритет:** P3 / low  
**Тип:** documentation/status correctness

Текущий `Docs/Status/AuditFindings.md` всё ещё говорит:

```text
CriticalCorrectiveHardening объявлен закрытым при неподтверждённом DoD.
План остаётся в каталоге активных.
```

Но на текущем HEAD:

- top-level DoD синхронизирован SVC-12;
- plan архивирован;
- `Docs/Plans/README.md` говорит, что активных планов нет.

Paragraph нужно удалить либо явно пометить историческим/закрытым.

---

# 4. Проблемы, оставшиеся с предыдущего ревью

## OPEN-01 — Core Repeater не transactional по состоянию reused widgets

**Приоритет:** P1 / high  
**Статус:** остаётся

В `Source/GV2/Public/UI/GV2KeyedCollection.h` после key/preflight validation
`ApplyItem()` вызывается прямо на live reused widgets:

```cpp
for (int32 Index = 0; Index < Models.Num(); ++Index)
{
    if (!ApplyItem(*TempOrderedWidgets[Index], Models[Index]))
    {
        return false;
    }
}
```

Сценарий:

```text
A reused -> Apply(new A) success
B        -> Apply failure
```

возвращает `false`, но `A` уже содержит `new A`.

Container structure может остаться прежней, но widget state уже изменён.

### Почему текущий test это пропускает

`CompositeRollbackContract` создаёт item_1/item_2 update + item_3 failure, но
после `ReconcileEntries == false` проверяет только число children.

Значения первых reused widgets после failure не проверяются.

### Screen-level rollback не отменяет дефект generic Repeater

`UGV2ScreenWidgetBase::ApplyScreenFields()` имеет compensating rollback, поэтому
LocationScreen часто восстанавливается.

Но generic `ReconcileEntries(false)` всё равно нарушает собственный
transactional contract для любого caller, не окружённого screen-level rollback.

---

## OPEN-02 — typography conformance не проверяет actual widget consumers

**Приоритет:** P2 / medium  
**Статус:** остаётся

`Source/GV2/Private/Tests/GV2WidgetSemanticFontSizeContractTests.cpp` подключает
классы Text/RichText/Button/InputField/DropdownSelect, но не создаёт их.

Для всех пяти «consumer» вызывается:

```cpp
UGV2TextPipeline::ResolveStyleForHeight(...)
```

и сравнивается `FTextBlockStyle.Font.Size`.

Тест доказывает parity helper-а с самим собой, но не фактический font size
внутренних renderer controls.

Регрессия в одном `ApplyCentralStyle()` может не сломать этот test.

---

## OPEN-03 — `CanApplyScreenFields()` не является predictive deep preflight

**Приоритет:** P2 / medium  
**Статус:** остаётся

Существующий regression scenario фактически показывает:

```text
Scene содержит nonexistent resource
CanApplyScreenFields(candidate) == true
ApplyScreenFields(candidate) == false
```

Screen-level rollback делает failure безопаснее, но `CanApply` не предсказывает
все глубокие apply failures.

---

## OPEN-04 — legacy single-widget properties физически остаются

**Приоритет:** P3 / low  
**Статус:** остаётся

Основные `[0]` fallback paths удалены, но C++ header всё ещё содержит:

```text
StaminaMeter
Character
```

и lifecycle продолжает обращаться к ним.

Это уже не старый функциональный bug, а obsolete compatibility surface и
расхождение со слишком сильным archive claim «legacy path удалён».

---

# 5. Ранее известные open findings из самого репозитория

## KNOWN-01 — нет regression gate против whole-frame uniform scaling

**Источник:** `UIF-AF-02`  
**Приоритет:** P2

Сегодня whole-frame scaling не найден, но нет negative regression gate,
гарантированно запрещающего его возврат.

---

## KNOWN-02 — невозможно выразить «ScalePolicy не объявлен»

**Источник:** `UIF-AF-05`  
**Приоритет:** P2 / требует решения contract

`EGV2PrimitiveScalePolicy` не имеет `Unset`, а default равен
`PreserveAspect`. Забытое authoring declaration неотличимо от сознательного
выбора `PreserveAspect`.

---

## KNOWN-03 — Content Editor tree search/expansion не имеет portable conformance

**Источник:** `CEH-AF-02`  
**Приоритет:** P3

Поведение покрыто UE Editor automation, но не portable conformance. Это test
portability debt, а не доказанный production bug.

---

# 6. Подтверждённые implementation gaps вне corrective UI review

## STATUS-001 — session lifecycle / replacement flow partial

Cold start существует, но полный contract lifecycle и replacement flow не
реализованы:

```text
Registering
BuildingState
RestoringInstances
Starting
PreparingPresentation
active-session preflight
cancellation
Menu <-> Game
load-another-save
content reload replacement
```

Это известный архитектурный gap.

---

## STATUS-002 — Presentation Effects отсутствуют

Нет production:

```text
one-shot effect DTO
effect queue
effect apply path
stale target handling
effect non-persistence tests
```

Это известный архитектурный gap, а не regression UI hardening.

---

# 7. Несоответствия status/archives текущему source

## CriticalCorrectiveHardening archive

Archive claim:

```text
failed reconciliation не оставляет partial state
```

сильнее текущего поведения generic `FGV2KeyedCollection` (`OPEN-01`).

Claim:

```text
verification измеряет фактический font size
```

сильнее фактического `WidgetSemanticFontSizeContract` (`OPEN-02`).

## LocationScreen historical DoD

Исторический план заявляет active items/effects в PlayerStatus, но portable
adapter их не передаёт (`NEW-03`).

## AuditFindings

Документ содержит поздние annotations о закрытии SVC-задач и одновременно
устаревший paragraph о том, что CriticalCorrectiveHardening всё ещё активен
(`NEW-06`).

---

# 8. Приоритет исправления

## P1

1. `OPEN-01` — Repeater widget-state atomicity.
2. `NEW-02` — missing child widget class must fail.
3. `NEW-03` — PlayerStatus items/effects boundary/contract.

## P2

1. `OPEN-02` — actual consumer font verification.
2. `OPEN-03` — predictive deep preflight.
3. `NEW-01` — side-effecting `BlueprintPure` getters.
4. `NEW-04` — единая repeated-key grammar.
5. `NEW-05` — полноценный 720p + long/pseudolocale geometry gate.
6. `KNOWN-01` — gate против whole-frame scaling.
7. `KNOWN-02` — explicit/unset ScalePolicy.

## P3

1. `OPEN-04` — obsolete Character/StaminaMeter properties.
2. `NEW-06` — stale status paragraph.
3. `KNOWN-03` — portable Content Editor tree conformance, если оправдано
   стоимостью.

---

# 9. Минимальные regression cases

## Repeater

```text
baseline:
A = oldA
B = oldB

candidate:
A = newA -> success
B = invalid -> failure

expected:
result == false
A pointer unchanged
B pointer unchanged
A state == oldA
B state == oldB
container order unchanged
container child count unchanged
```

## Missing child class

```text
valid host
missing child class
non-empty candidate

expected:
CanApply/Apply == false
Applied unchanged
visual state unchanged
```

## PlayerStatus boundary

Если items/effects остаются частью baseline:

```text
meters
items/equipment
effects
```

должны проходить portable adapter, а item/effect entry должен иметь минимум:

```text
key
resource_id
```

## Font consumers

На instantiated WBP Text/RichText/Button/InputField/Dropdown для `body` на:

```text
720
1080
1440
2160
```

читать font size из фактического inner renderer/control, а expected — из
central pipeline.

## 720p

Использовать long/pseudolocale strings и проверять полный rectangle:

```text
left >= viewport.left
top >= viewport.top
right <= viewport.right
bottom <= viewport.bottom
```

плюс containment в предназначенной CommandPanel/scroll/wrap области.

---

# 10. Финальная оценка

После corrective cycle проект существенно сильнее исходного состояния:

- actual arranged geometry действительно измеряется;
- Tavern->Market test выполняет реальное semantic действие;
- Scene characters переведены на keyed collection;
- `[0]` character/meter rendering fallback удалён;
- `RenderMode -> ScalePolicy` inference удалён;
- NineSlice проверяется по resulting brush;
- screen-level rollback существует и покрыт;
- Stable ID browser использует canonical parser.

Но текущий status «все corrective problems закрыты» остаётся слишком сильным.

На HEAD `93bc1e1a...` остаются три high-priority corrective problems, несколько
medium validation/verification проблем, low-priority cleanup/status issues и два
явно известных архитектурных implementation gap.

Главное уточнение этого ревью: **после предыдущего ревью код не менялся**.
Поэтому `NEW-*` — не новые регрессии, а дополнительные дефекты, обнаруженные
более глубоким повторным чтением того же состояния.
