---
title: Self-Contained Payload Tasks
status: active
version: 1.9
updated: 2026-09-09
depends_on:
  - README.md
  - Snapshot.md
  - ../../ADR/0040-universal-ui-property-pipeline.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# M3 — Self-Contained Payload

> **Материализует:** `PAH-R1`, `D3/D4` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md) и необходимую перед module extraction типовую границу.
> **Задачи:** PSC-09A…09B, PSC-10A…10B.
> **Результат:** Prepare и Apply больше не являются методами одного authority-aware объекта; нижний DTO содержит всё необходимое физическому применению — и для видов операций, и для центральной стилизации.

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

- [x] **PSC-09B — Весь production-путь проходит через транзакцию**
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
  - **Реализация (2026-09-08):** Задача выполнена последовательностью из 7 независимо
    верифицированных и закоммиченных под-шагов (`31c2ecf`…`1c9c839`) — по явному
    выбору пользователя разбить крупную задачу на под-коммиты вместо одного большого
    изменения или частичного покрытия.

    **Паттерн разделения физической мутации**, применённый одинаково для всех 11
    `IGV2PropertyConsumer` kinds: если target — plain Engine/UMG/CommonUI тип
    (`UCheckBox`, `UEditableTextBox`, `UProgressBar`, `UCommonTextBlock`,
    `UCommonRichTextBlock`), операция — новый `FPreparedXxxOperation` struct в
    `GV2PresentationApply`, применяемый напрямую `GV2PresentationApply::Apply()` —
    это НЕ adapter, это подлинно нижний код. Если target требует GV2-owned widget/
    interface (`UGV2ImageWidgetBase`, `UGV2PortraitWidgetBase`,
    `UGV2DropdownSelectWidgetBase`, `UGV2InputFieldWidgetBase`,
    `UGV2ProgressBarWidgetBase`, `UGV2TabContainerWidgetBase`, `UGV2TextWidgetBase`,
    `UGV2ButtonWidgetBase`, `UGV2RichTextWidgetBase`, `IGV2UiPropertyHost`,
    `IGV2UiBindingTarget`, `UGV2ListViewWidgetBase`, `UGV2ButtonListWidgetBase`,
    `IGV2UiStyleConsumer`) — которые `GV2PresentationApply.Build.cs`'s denylist делает
    структурно недостижимыми — та же транзакция применяется новым
    `Source/GV2/{Public,Private}/UI/GV2LegacyPresentationApplyAdapter.{h,cpp}`,
    живущим в `GV2` (верхний модуль), выполняющим "no semantic lookup" — чисто
    механический Cast+call dispatch по уже разрешённым данным. Это явно временная
    конструкция (Payload.md M3 intro: "physical Apply ещё может временно вызываться
    через adapters в `GV2`") — не мигрируется, а удаляется в `PSC-11`, когда
    widget-owning UCLASS'ы физически переезжают в `GV2PresentationApply`.

    **Canonical lower types**, заменившие GV2-owned USTRUCT/UENUM(BlueprintType) payload
    (не копии/алиасы, отдельные типы только на Core/Engine/Slate/CommonUI): `FPreparedResolvedImageValue`/
    `EPreparedImageRenderMode` (вместо `FGV2ResolvedImageResource`/`EGV2ImageRenderMode`),
    `FPreparedRichTextHover`/`FPreparedRichTextSpan` (вместо `FGV2RichTextHoverViewModel`/
    `FGV2RichTextSpanViewModel`, `FGV2UiBindingHandle` сведён к plain `FString
    SerializedBinding`), `FPreparedTextValue` (вместо `FGV2TextViewModel`, переиспользован
    внутри `FPreparedTabEntry::Title`), `FPreparedTabEntry` (вместо `FGV2TabItemEntry`).

    **Точные поведенческие асимметрии сохранены явно, не выведены**: оригинальный
    `FGV2TextPropertyConsumer::Commit()` перенаправлял `UGV2RichTextWidgetBase` targets
    на внутренний `RichTextBlock`, `Reset()` — нет; сохранено полем `bool bIsReset` на
    `FPreparedTextOperation`/`FPreparedKeyedCollectionOperation`/
    `FPreparedTabContainerOperation`, читаемым только адаптером.

    **Рекурсивные consumers** (`FGV2KeyedCollectionPropertyConsumer`,
    `FGV2TabContainerTabsPropertyConsumer`): recursive per-item/per-tab
    `CommitUiHostProperties`/`CommitScreenFields` вызовы не изменены — каждое
    вложенное capability уже применяется через собственный мигрированный leaf
    consumer, это не физическая мутация ЭТОГО consumer'а. Мигрирован только финальный
    шаг сборки дерева виджетов (panel reconciliation, `ApplyTabEntries`).

    **`UGV2TextPipeline::Apply/ApplyRichText/ApplyHint`** разделены: Theme-resolution
    (Style/DefaultStyle/Markup, guard'ы, `ResolveEffectiveFontSize`/`ResolveStyle`/
    `ResolveStyleClass`/`NormalizeMarkup`) остаётся upper (удаление Theme-доступа —
    именованная работа `PSC-10A`, не этой задачи); физическая мутация — новые
    `FPreparedPlainTextOperation`/`FPreparedRichTextRenderOperation`/
    `FPreparedTextHintOperation`, применяемые `GV2PresentationApply::Apply()`.
    Публичные сигнатуры не изменены — ни один из 24+ call sites не тронут.
    `UCommonTextStyle` объявлен внутри `CommonTextBlock.h` (отдельного
    `CommonTextStyle.h` не существует) — заголовок транзакции подключает
    `CommonTextBlock.h` напрямую вместо forward declaration, поскольку
    `TSubclassOf<UCommonTextStyle>` как поле требует complete type.

    **Новые gates**: `validate_presentation_apply_field_inventory.py`
    (source-derived: сканирует поля всех struct'ов транзакции, отвергает
    `TFunction`/`PrepareContext`/`Repository`/`Registry`/`Theme`/`Session`/`Snapshot`/
    generic `void*` по типу И по имени поля; allowlist для `TWeakObjectPtr<X>` и
    `TSubclassOf<X>` targets; 7 self-test кейсов) и
    `validate_property_consumer_transaction_coverage.py` (множество из 11
    `IGV2PropertyConsumer` kinds выводится обходом `class GV2_API
    F...PropertyConsumer : public IGV2PropertyConsumer` деклараций в
    `GV2PropertyConsumers.h`, не список в задаче; для каждого требуется вызов
    `GV2PresentationApply::Apply`/`GV2LegacyPresentationApplyAdapter::Apply`; 4
    self-test кейса).

    Red-on-revert проведён для каждого из 7 под-шагов независимо — каждый раз
    временный no-op соответствующей физической мутации проваливал конкретные
    production-тесты (от 3 до 12 тестов на шаг), откат восстанавливал зелёный статус.

    Верификация (финальный прогон): 128/128 UE Automation, 94/94 portable ctest, все
    13 standalone `validate_*.py` + `validate_core_decoupling.py` + `validate_docs.py`
    (185 файлов) — зелёные. Ни один `/Script/GV2` class path не изменён за все 7
    под-коммитов.

- [x] **PSC-10A — Замкнуть resolved payload для всех operation kinds**
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
    - на путях видов операций не остаётся ни одного обращения к `GetConfiguredTheme()`/`GetConfiguredRegistry()` и эквивалентным configured accessors; settings/DataAssets остаются только candidate-builder inputs;
    - **область этого пункта — только пути видов операций.** Центральная стилизация сейчас является отдельным механизмом со своим входом, и её закрывает `PSC-10B`. Физическое удаление самих accessor-функций возможно только когда закрыты обе задачи, и это условие названо в `PSC-10B`, а не подразумевается здесь;
    - unresolvable value даёт typed Prepare failure и не создаёт partial transaction;
    - production test проходит каждый фактический operation kind; actual set выводится из enum/variant, expected behavior — из независимой classification table.
  - Evidence: prepared operation declarations, compiler exhaustive switch/visitor, recursive field inventory, Text/Image/Nested/Collection production tests.
  - **Реализация (2026-09-08):** Задача выполнена последовательностью из 5 независимо
    верифицированных и закоммиченных под-шагов (`d2ffab6`…`a7f683f`), тем же способом
    (несколько под-коммитов внутри задачи), что и `PSC-09B`.

    **Единый enum/variant** (под-шаг 1): `FGV2PreparedPresentationTransaction` хранил 17
    параллельных `TArray`, по одному на operation kind — заменено на единственный
    `TArray<FGV2PreparedOperationVariant>` (`TVariant` по всем 17 struct'ам) плюс
    `EGV2PreparedOperationKind` enum (порядок объявления зеркалит порядок шаблонных
    аргументов variant'а). Типовые `Add*Operation()` методы сохранены как тонкие обёртки
    над одним `AddOperation()` — тот же паттерн, что уже использует
    `FGV2PreparedUiValue::Make*()` поверх собственного `TVariant` в этом кодбейзе, что
    оставило 29 producer call sites нетронутыми. Типовые `Get*Operations()` удалены и
    заменены одним `GetOperations()`; оба потребителя (`GV2PresentationApply::Apply()` и
    `GV2LegacyPresentationApplyAdapter::Apply()`) переписаны на единственный цикл с
    `Visit()` поверх `TOverloaded<...>` (стандартная "overloaded lambda set" идиома,
    deduction guide + `using Ts::operator()...`) — лямбда без кейса для одной из 17
    альтернатив теперь ошибка компиляции, а не тихо пропущенный `default`.

    **Font/scale policy как pure function** (под-шаг 2, самый крупный): `FGV2TextViewModel`
    получил resolved-presentation cache (`bHasResolvedPresentation`, `ResolvedStyleClass`,
    `ResolvedBaseFontSize`/`MinReadableFontSize`/`ReferenceViewportHeight`/`FontScaleCurve`,
    `bHasResolvedDefaultStyle`/`ResolvedDefaultStyle`), заполняемый только
    `UGV2TextPipeline::Resolve()`, когда вызван с новым опциональным PrepareContext —
    Theme читается через `PrepareContext->GetTheme()` (пин из session snapshot), не
    `GetConfiguredTheme()`. `UGV2UiTheme::GetEffectiveFontSize` разделён на публичный
    `ResolveUnscaledFontSize()` (Theme-lookup half) и существующую scale-математику —
    behavior-preserving рефакторинг, переиспользуемый и `Resolve()`, и старым путём.
    `Apply()`/`ApplyRichText()` резолвят `Style`/`ScalePolicy` из `bHasResolvedPresentation`
    когда он true — ни одного обращения к Theme в этом случае; иначе (~24 call site вне
    operation-kind pipeline — собственное `NativePreConstruct`-стилирование виджетов,
    scope `PSC-10B`) поведение не изменилось, `ScalePolicy.bIsAlreadyScaled=true` делает
    финальный `SetFontSize` в нижнем модуле same-value no-op. Новые
    `GV2PresentationApply::FPreparedTextScalePolicy` + pure-function
    `EvaluatePreparedFontSize()`/`ResolveLiveViewportHeight()` зеркалят
    `UGV2UiTheme::EvaluateTextScale`/`GetEffectiveFontSize` и
    `UGV2TextPipeline::GetViewportHeight`'s математику один-в-один, но без Theme/UObject
    зависимости; финальный размер шрифта вычисляется `GV2PresentationApply::Apply()` как
    pure function от policy и LIVE viewport height, считанного в момент применения.
    `PrepareContext` прокинут в production: `GV2ScreenFieldMaterializer::BuildFields`/
    `FMaterializeContext`/`ResolveText` получили опциональный параметр (нулевой churn),
    `GV2SessionCoordinator::PrepareDocumentRequest` строит его тем же
    `TOptional<FGV2PresentationPrepareContext>`-паттерном, что уже использует
    `UGV2RuntimeSubsystem::HandleDocumentRequested` — единственный production call site
    `BuildFields`, резолюция текста для инициального документа и всех document update
    теперь идёт через PrepareContext.

    **RichTextSpans hover-popover** (под-шаг 3): последний прямой вызов
    `GetConfiguredTheme()` в `GV2PropertyConsumers.cpp` — та же PrepareContext-first схема.
    Отдельно ВЕРИФИЦИРОВАНО (не предположено), что `GetConfiguredRegistry()`'s fallback в
    `FGV2TabContainerTabsPropertyConsumer::Prepare()` (PSC-06-era код) уже недостижим ни с
    одного production operation-kind пути: все три production call site
    `PrepareUiHostProperties` либо сами получают PrepareContext параметром, либо строят
    его через `GetContentSnapshotForPrepare()`; единственный другой похожий на production
    call site (`RunUiCapabilityObservabilityHarness`) вызывается только из
    `GV2UiCapabilityObservabilityTests.cpp`.

    **Exhaustive kind-walk test** (под-шаг 4): `GV2.Runtime.Presentation.
    ExhaustiveOperationKindWalk` — множество kinds берётся обходом `TVariantSize_V`, не
    список в тесте; для каждого kind switch без `default` строит одну валидную операцию
    (новый kind без соответствующего case не компилируется здесь тоже); ожидаемое
    поведение (`LowerModuleMutatesDirectly` / `LeftEntirelyForAdapter`) читается из
    независимой `ExpectedRouteFor`-таблицы, написанной по doc-комментариям структур, не
    выведено из тех же лямбд, которые использует сам `Apply()`.

    **Recursive field inventory** (под-шаг 5): новый self-test кейс доказывает, что
    `validate_presentation_apply_field_inventory.py` ловит нарушение, спрятанное внутри
    NESTED struct'а (referenced через `TArray<...>`), не только на верхнем уровне.

    **Payload completeness для остальных kinds** (verified via reading, не новый код):
    image/resource (bullet 3) уже несёт `Brush`/`RenderMode`/`FixedAspectRatio`, а не
    только `resource_id` — установлено `PSC-09B`. screen/nested (bullet 4): `FPreparedTabEntry`
    несёт УЖЕ СКОНСТРУИРОВАННЫЙ `ScreenWidget` (виджет физически построен с resolved
    class ДО того, как Commit строит эту операцию — резолюция класса и валидация
    placement уже произошли в Prepare через `PrepareContext->ResolveScreen`), что
    сильнее, чем нести отдельно `UClass*`. primitive/binding/collection/reset (bullet 5)
    несут resolved value через canonical lower types, установленные `PSC-09B`. Stable
    IDs (bullet 6) везде остаются рядом с resolved payload, не заменяют его. Unresolvable
    value (bullet 11): каждый существующий `Prepare()` возвращает `false` ДО вызова
    `Add*Operation()` на любом failure path — partial transaction структурно невозможна
    без явного нарушения этого уже установленного паттерна.

    Верификация (финальный прогон): 129/129 UE Automation (128 + 1 новый), 94/94 portable
    ctest, все 13 standalone `validate_*.py` + `validate_core_decoupling.py` +
    `validate_docs.py` (185 файлов) — зелёные. Red-on-revert проведён для каждого из 5
    под-шагов независимо (кроме RichTextSpans-ветки под-шага 3 — честно отмечено в
    коммите как непроверенная production-путём: единственный существующий тест на эту
    проверку строит consumer напрямую без `SetPrepareContext`, а GameData не использует
    span hover нигде; построение session/candidate fixture только ради одной проверки
    посчитано непропорциональным, тот же компромисс, что `PSC-08` сделал для
    `GetConfiguredRegistry()`. Зарегистрировано как
    [`STATUS-018`](../../Status/ImplementationStatus.md) — код корректен и не нарушает
    `ADR-0043` D1, но claim «эта ветка red-on-revert-доказана» был бы ложным без этой
    записи; условие закрытия там же).

- [ ] **PSC-10B — Включить central style в transaction и удалить runtime accessors**
  - Зависимости: PSC-10A.
  - `IGV2UiStyleConsumer::ApplyCentralStyle` — второй путь применения, независимый от существующих видов операций. Его фактическое множество выводится из реализаций интерфейса; текущие прямые и косвенные обращения к Theme внутри этих классов являются scope задачи, а не фиксированным ручным списком.
  - Инвариант: ни один путь, ведущий к физической мутации, не разрешает семантику ([ADR-0043](../../ADR/0043-presentation-apply-boundary.md), `INV-P5`, `D3`). Предыдущая формулировка `PSC-10` оставляла central style без владельца: под перевод существующих operation kinds он не подпадал, а содержащие его классы `PSC-12` переносит в модуль, где Theme недостижим по построению.
  - Не считается закрытием: перенос темы в поле виджета, читаемое при стилизации; передача указателя на Theme; отдельный runtime prepare/apply entry point; закрытие только известного списка классов; сохранение no-argument `ApplyCentralStyle` или runtime-вызова из `NativePreConstruct` рядом с транзакцией.
  - Done:
    - множество классов, реализующих `IGV2UiStyleConsumer`, берётся обходом реализаций интерфейса, а не списком в задаче;
    - central style представлен замкнутым kind/variant той же `FGV2PreparedPresentationTransaction`; операция несёт concrete CommonUI/Slate classes, brushes, colors, spacing и scale policies, необходимые target-виджету, без Theme object или token lookup;
    - central-style kind включён в exhaustive visitor и независимую behavior classification, созданные `PSC-10A`; добавление или удаление kind без Prepare/Apply semantics ломает тот же compiler/test gate;
    - runtime style Prepare получает `FGV2PresentationPrepareContext`, добавляет prepared central-style operation в общую transaction, а физическая мутация выполняется только единственной transaction façade;
    - no-argument `IGV2UiStyleConsumer::ApplyCentralStyle` удалён как самостоятельный runtime API; если остаётся локальный target-helper, он принимает только resolved value и все его production call sites структурно принадлежат transaction façade;
    - runtime-ветка `NativePreConstruct` не применяет central style и не читает Theme/settings. Design-time preview может применять только сериализованные Widget/Blueprint defaults через pure value-only helper при `IsDesignTime()`; он не читает configured Theme, не загружает content и не является runtime entry point;
    - `RichTextWidgetBase`'s разрешение run style и interactive style входит в тот же Prepare → prepared operation → transaction Apply path, хотя сейчас не является `ApplyCentralStyle`;
    - **после закрытия `PSC-10A` и этой задачи** символы `GetConfiguredTheme()`/`GetConfiguredRegistry()` физически удалены из production declarations, definitions и call sites; candidate builder читает settings/DataAssets только как explicit bootstrap inputs и публикует их результат исключительно через snapshot;
    - `UGV2UiTheme::GetCoreMinimalTheme()` сохраняется только для UE-native cold-start recovery. Это не исключение к предыдущему пункту: метод не читает configured content, а production call-site inventory допускает его только внутри recovery surface;
    - authority counter даёт ноль вокруг central-style Prepare result application и остаётся secondary evidence к transaction/module boundary;
    - implementation inventory и production call-site inventory отвергают синтетическое обращение к Theme, runtime `NativePreConstruct → style`, вызов target-helper вне transaction façade и `GetCoreMinimalTheme()` вне recovery;
    - в том же change set обновлены [Widget Registry](../../UI/WidgetRegistry.md), [UI Document](../../UI/UIDocumentAndReconciliation.md) и partial-supersession note [ADR-0012](../../ADR/0012-centralized-ui-theme.md): runtime reconstruction и editor preview больше не описываются одним authority-aware путём.
  - Evidence: prepared central-style operation/variant, implementation and call-site inventories, transaction façade production tests, design-time preview test, recovery call-site gate и обновлённые owner contracts.
  - **Реализация (2026-09-09), срез 1 из N — мёртвый gate удалён.** Обход реализаций интерфейса дал 19 классов, но семь из них (`ScrollArea`, `Panel`, `Modal`, `ListView`, `Portrait`, `GameShell`, `TabContainer`) читали `GetConfiguredTheme()` **только ради null-проверки**: полученное значение не использовалось ни разу, ни одного `Theme->` в теле. У `Modal` и `TabContainer` за проверкой шло настоящее тело (применение текста через уже мигрированный `PSC-10A` конвейер и рекурсия по детям), у остальных пяти — `return true`.

    Это седьмой экземпляр семейства «значение получено и отброшено» после `ResourceIcon`, `ApplyOptionalXxx`, `OnBindingInvoked`, `bFatal`, `IsLayerAllowedForEmbedded` и `bSingleton`. Здесь он вдобавок раздувал оценку самой задачи: масштаб «19 классов, у каждого свой набор Theme-полей» на семь классов держался на чтении, которое ничего не читает.

    Удалены семь обращений (41 → 34), поведение сохранено: реализации по-прежнему возвращают `true`, интерфейс не снимается (`GV2.Runtime.UIKit.CentralThemeAndComponents` проверяет `Implements<UGV2UiStyleConsumer>()` у компонентов, и снятие интерфейса было бы отдельным решением, а не следствием). Единственное изменение наблюдаемого — при отсутствующей теме эти семь возвращают `true` вместо `false`; после закрытия задачи отсутствующая настроенная тема и есть нормальное состояние, поскольку тема приходит из снимка.

    Верификация: полный `Automation RunTests GV2` — 129/129 из машинного отчёта.

    Остаётся: 11 классов с фактическими Theme-полями (`ButtonList`, `Button`, `Checkbox`, `DropdownSelect`, `Image`, `InputField`, `LoadingIndicator`, `ProgressBar`, `RichTextPopover`, `RichText`, `Separator`), третий путь `RichTextWidgetBase`'s run/interactive style, design-time ветка, удаление `ApplyCentralStyle` как runtime API и символов accessor'ов.

## Проверка milestone

- [ ] Верхний слой только разрешает semantics и формирует transaction; lower-facing types не могут вызвать authority.
- [ ] Все operation kinds перечисляет enum/variant, а не задача или test list.
- [ ] Ни один payload не требует lookup/load при Apply.
- [ ] Ни один путь к физической мутации не разрешает семантику — ни виды операций, ни центральная стилизация.
- [ ] `GetConfiguredTheme()`/`GetConfiguredRegistry()` отсутствуют в production-коде без исключений; `GetCoreMinimalTheme()` достижим только из UE-native cold-start recovery.
- [ ] `UCLASS` paths ещё не менялись; production path работает через временный delegating adapter.
