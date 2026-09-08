---
title: Self-Contained Payload Tasks
status: active
version: 1.3
updated: 2026-09-08
depends_on:
  - README.md
  - Snapshot.md
  - ../../ADR/0040-universal-ui-property-pipeline.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# M3 — Self-Contained Payload

> **Материализует:** `PAH-R1`, `D3/D4` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md) и необходимую перед module extraction типовую границу.
> **Задачи:** PSC-09A…09B, PSC-10.
> **Результат:** Prepare и Apply больше не являются методами одного authority-aware объекта; нижний DTO содержит всё необходимое физическому применению.

## Почему этап идёт до module extraction

Сейчас `IGV2PropertyConsumer` совмещает `Prepare` и `Commit`, а `UGV2TextPipeline` — text/theme resolution и widget mutation. Такой тип нельзя перенести в lower module без обратной зависимости на `GV2`.

Сначала верхний слой формирует immutable transaction из resolved operations. На этом этапе physical Apply ещё может временно вызываться через adapters в `GV2`; окончательную гарантию даёт следующий milestone, когда реализация Apply физически переезжает в `GV2PresentationApply`.

## Задачи

- [x] **PSC-09A — Типовая граница: контекст подготовки, каркас модуля и транзакция**
  - Зависимости: PSC-06, PSC-08.
  - Инвариант: объект, способный обратиться к snapshot/registry/Theme, не исполняет physical mutation; объект применения не имеет authority capability.
  - Не считается закрытием: перенос существующего `IGV2PropertyConsumer` целиком; callback из lower operation в upper resolver; `void*`/generic service locator; временная обратная module dependency; изменение `UCLASS` paths.
  - Done:
    - `FGV2PresentationPrepareContext` принадлежит верхнему `GV2` и является единственным типовым входом к snapshot resolution;
    - создаётся DTO-only каркас `Source/GV2PresentationApply/` с окончательным dependency allowlist; `GV2` зависит от него, обратное ребро отсутствует;
    - `FGV2PreparedPresentationTransaction` сразу объявлен в `GV2PresentationApply/Public`, состоит из immutable resolved operations и не включает PrepareContext;
    - `IGV2PropertyConsumer` разделён: upper preparers создают operations, lower applicator/operations выполняют Commit/Reset/Rollback;
    - public lower DTO использует только value/resolved UE types из разрешённого dependency set;
    - task не меняет ни одного `/Script/GV2` class path;
    - явно записано, какие виды операций ещё не проходят через транзакцию на конец задачи, — это состояние закрывает `PSC-09B`, и оно названо, а не подразумевается.
  - Evidence: `Source/GV2PresentationApply/GV2PresentationApply.Build.cs`, public prepared transaction/operation declarations, разделённые property interfaces, module graph gate.
  - **Реализация (2026-09-08):** Новый Unreal module `Source/GV2PresentationApply/` — `GV2PresentationApply.Build.cs` объявляет ТОЛЬКО `Core, CoreUObject, Engine, UMG, CommonUI, Slate, SlateCore` (никакой UHT-рефлексии — plain C++ структуры/namespace-функции, без `USTRUCT`/`UCLASS`, без `Module.cpp`, тот же "чисто C++ dependency-модуль" паттерн, что уже используют `GV2ContentHostSupport`/`GV2RuntimeCore`; НЕ добавлен в `.uproject`'s "Modules"/Target.cs `ExtraModuleNames` — линкуется только транзитивно через `GV2.Build.cs`'s `PublicDependencyModuleNames`, как и они). `GV2` объявляет единственное разрешённое прямое ребро (`GV2.Build.cs` → `"GV2PresentationApply"`); обратного ребра нет физически, поскольку `GV2PresentationApply.Build.cs` его не может объявить (не список имён, а то, что UBT способен слинковать).

    **`FGV2PreparedPresentationTransaction`** (`GV2PresentationApply/Public/GV2PresentationApply/PreparedPresentationTransaction.h`, namespace `GV2PresentationApply`) — immutable контейнер `TArray<FPreparedImageResourceOperation>`; `FPreparedImageResourceOperation{TWeakObjectPtr<UImage> TargetWidget; FSlateBrush Brush}` — Brush уже полностью финализирован (Tiling/DrawAs уже выставлены под scale policy), PrepareContext отсутствует, никакой authority-тип физически недостижим (denylist — граф сборки). Единственная public entry point — `GV2PresentationApply::Apply(transaction, OutError)`: для каждой image-операции либо `SetBrush`+`SetDesiredSizeOverride`, либо typed-ошибка на невалидный (GC'd) target — decision-логики внутри нет, только физическая мутация.

    **`IGV2PropertyConsumer` разделён** новым virtual `BuildPreparedOperation(TargetWidget, OutTransaction, OutError) const` (default `return false` — "не мигрировано"). Единственный override — `FGV2ImageResourcePropertyConsumer`, и только для plain-`UImage` target (тот самый case, что раньше напрямую вызывал `FGV2ImagePresentation::ApplyResolved`). `Commit()`/`Reset()` для этого одного case теперь строят transaction и вызывают `GV2PresentationApply::Apply(...)` — реальная production-wiring, не демо в отрыве от боевого пути. Оба `UGV2ImageWidgetBase`/`UGV2PortraitWidgetBase` target case НЕ мигрированы — `Commit()` по-прежнему вызывает их собственные `ApplyResolvedImageResource`/`ApplyResolvedPortrait` UFUNCTION напрямую (эти классы — `/Script/GV2` UCLASS, менять их пути явно запрещено этой задачей).

    **Явно названный остаток на конец задачи** (Done-bullet "явно записано, какие виды операций ещё не проходят через транзакцию"): НЕ мигрированы — Text, Boolean, Integer, Number, String, Key, Binding, KeyedCollection, RichTextSpans, TabContainer (все 10 остальных consumer kinds) целиком; Image-consumer's собственные `UGV2ImageWidgetBase`/`UGV2PortraitWidgetBase` target cases (2 из 3 веток `Commit()`); `FGV2ImagePresentation::ApplyResolved`/`ResolveAndApply`, `FGV2ResolvedImageResource`, `EGV2ImageRenderMode`, `EGV2PrimitiveScalePolicy` остаются в `GV2` нетронутыми (используются как `UFUNCTION(BlueprintCallable)` параметры на `UGV2ImageWidgetBase`/`UGV2PortraitWidgetBase` — физический перенос вниз потребовал бы UHT-рефлексии в новом модуле, сознательно вынесено за рамки этой задачи). Закрытие всего перечисленного — `PSC-09B`.

    **Новый gate** `validate_presentation_apply_module_graph.py` (declaration-derived): парсит `GV2PresentationApply.Build.cs`'s `Public/PrivateDependencyModuleNames`, требует каждую запись быть в allowlist (denylist-записи называются по имени явно), плюс проверяет, что `GV2.Build.cs` объявляет `"GV2PresentationApply"`. Негативный self-test: synthetic Build.cs с `GV2ContentCore` (denylist) и с неизвестным именем (`SomeFutureModule`) оба корректно отклоняются; синтетический `GV2.Build.cs` без forward-edge отклоняется.

    **Новые тесты**: `GV2.Runtime.Presentation.PresentationApplyImageOperation` — изолированный unit-тест самого `GV2PresentationApply::Apply()` (без property consumer/session вообще): валидная operation мутирует widget корректно (brush/DrawAs/ImageSize), operation со stale/unassigned target даёт typed failure, не crash. `GV2.UI.StandardPropertyConsumers`'s секция 3 расширена (3e): `InnerIcon` — raw `UImage`, переданный НАПРЯМУЮ как `TargetWidget` (не через `UGV2ImageWidgetBase`/`UGV2PortraitWidgetBase` wrapper), поэтому `Commit()`/`Reset()` реально проходят через мигрированную ветку — `GetBrush().GetResourceObject()` проверяется до/после, доказывая физическую мутацию через новый модуль, а не только компиляцию. Существующий source-audit тест (STATUS-012 pipeline compliance) обновлён: искал `ConsumerSource.Contains("FGV2ImagePresentation::")`, который теперь совпадал бы только случайно с текстом комментария (реальный код в `GV2PropertyConsumers.cpp` больше не пишет эту строку напрямую) — заменён на явную проверку обеих реальных форм маршрутизации (`ApplyResolvedImageResource(PreparedResource`/`ApplyResolvedPortrait(PreparedResource` для host-веток, `GV2PresentationApply::Apply` для мигрированной).

    Red-on-revert: временный no-op `GV2PresentationApply::Apply` (всегда `return true` без мутации) немедленно провалил ОБА зависимых теста (`PresentationApplyImageOperation`, `StandardPropertyConsumers`). Откат применён обратно.

    Верификация: 128/128 UE Automation (127 существующих + 1 новый), 90/90 portable ctest (+2 новых gate-теста), все 13 standalone `validate_*.py`, `validate_core_decoupling.py`, `validate_docs.py` (185 файлов) — зелёные. Ни один `/Script/GV2` class path не изменён.

- [ ] **PSC-09B — Весь production-путь проходит через транзакцию**
  - Зависимости: PSC-09A.
  - Разделение вынесено из `PSC-09A` не по размеру, а потому что у крупной задачи нет промежуточной точки, с которой видно, что пошло не так: `PSC-09A` устанавливает границу типов, `PSC-09B` заводит через неё всё. Та же причина, по которой разделены `PSC-11` и `PSC-12`.
  - Инвариант: транзакция является единственным протоколом между разрешением и применением; второго пути, минующего её, не существует ни для одного вида операции.
  - Не считается закрытием: перевод части видов при сохранении прежнего пути для остальных; `UGV2TextPipeline`, оставшийся вторым полным pipeline; дубли типов и compatibility alias вместо переноса; callback из нижней операции наверх.
  - Done:
    - `UGV2TextPipeline` разделён на upper text/theme preparation и lower widget application; прежний тип не остаётся вторым полным pipeline;
    - все authority-free value/interface types, необходимые lower DTO и будущим physical widget bases, перемещаются вниз либо заменяются одним canonical lower type; дубли и compatibility aliases не остаются;
    - keyed collections, nested screens и screen replacement выражены тем же transaction protocol, без callback в upper module;
    - существующий production path уже строит новую transaction; временный `GV2` adapter только делегирует её применение и не выполняет semantic lookup;
    - source-derived field inventory отвергает function/callback, PrepareContext, repository/package/registry/theme source pointers и generic service handles; у инвентаря есть отрицательный самотест;
    - множество видов операций, обязанных проходить через транзакцию, берётся обходом перечисления видов, а не списком в задаче;
    - task не меняет ни одного `/Script/GV2` class path.
  - Evidence: разделённые text interfaces, production initial-screen test, payload inventory/self-test, exhaustive kind walk.

- [ ] **PSC-10 — Замкнуть resolved payload для всех operation kinds**
  - Зависимости: PSC-09B.
  - Инвариант: если semantic ID/token был проверен в Prepare, Apply использует именно полученный resolved payload и не повторяет решение.
  - Не считается закрытием: покрытие только Text/Image; ID без соседнего payload; `TSoftObjectPtr` с поздним разыменованием; pointer на resolver/context; switch с `default` либо ручной список kinds.
  - Done:
    - operation kinds образуют один enum/variant; compiler exhaustive visitor без `default` отвергает новый kind без Apply semantics;
    - text operation несёт resolved localized text/markup, concrete style/renderer class и font/scale policy;
    - image/resource operation несёт loaded object/brush/render policy, а не только `resource_id`;
    - screen/nested operation несёт resolved loaded class и validated placement descriptor;
    - primitive, binding, collection, reset, rollback и projection-recovery operations содержат достаточные value payloads;
    - Stable IDs остаются только identity/diagnostic metadata рядом с resolved payload;
    - soft references, resolver/context pointers и callable callbacks отсутствуют во всех nested public payload fields; recursive declaration inventory имеет negative self-test;
    - viewport-dependent font/layout size вычисляется Apply как pure function от prepared policy и текущей geometry, без Theme/config lookup;
    - legacy runtime `GetConfiguredTheme()`/`GetConfiguredRegistry()` и эквивалентные configured accessors удалены после перевода всех operation kinds; settings/DataAssets остаются только candidate-builder inputs;
    - unresolvable value даёт typed Prepare failure и не создаёт partial transaction;
    - production test проходит каждый фактический operation kind; actual set выводится из enum/variant, expected behavior — из независимой classification table.
  - Evidence: prepared operation declarations, compiler exhaustive switch/visitor, recursive field inventory, Text/Image/Nested/Collection production tests.

## Проверка milestone

- [ ] Верхний слой только разрешает semantics и формирует transaction; lower-facing types не могут вызвать authority.
- [ ] Все operation kinds перечисляет enum/variant, а не задача или test list.
- [ ] Ни один payload не требует lookup/load при Apply.
- [ ] `UCLASS` paths ещё не менялись; production path работает через временный delegating adapter.
