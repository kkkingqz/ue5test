---
title: Declaration Constraints Tasks
status: active
version: 1.4
updated: 2026-09-01
depends_on:
  - README.md
  - DeclaredSurface.md
  - ../Archive/DataDrivenUiComposition.md
---

# M2 — Declaration Constraints

> **Материализует:** `REM-01`.
> **Задачи:** GBH-06…08.
> **Результат:** объявление несёт свои ограничения, и сверка с ребёнком сравнивает capability целиком.

## Результат этапа

`DUC-07` вводился как независимая сверка объявления композита с умениями ребёнка и эту роль выполняет: дерево строится только из объявления, умения читаются из `DescribeUiCapabilities` ребёнка — источники действительно разные. Но сравнивается только вид.

Capability несёт больше: диапазон числа, границы целого, `TargetKind` для ссылки, требования идентичности коллекции. Подтверждённое следствие: `UGV2ProgressBarWidgetBase` объявляет `percent` как число `[0..1]`, объявляемый композит строит `AddNumber` без диапазона, `FGV2NumberPropertyConsumer::Prepare` проверяет только тип — значение `5.0` доходит до виджета, который обрезает его при отрисовке. Контракт `Schema ⊆ Capabilities` нарушен в измерении диапазона.

Архитектурный корень глубже отсутствующей проверки: объявление хранит `PropertyName`, `ChildWidgetName`, `Kind` и **не указывает, какую именно capability ребёнка делегирует**. Пока ребёнок объявляет одну capability каждого вида, сверка однозначна; при двух — нет.

**Порядок задач строгий.** Сначала `GBH-02A` убирает неполный `CollectionHost` из selectable surface. Затем объявление получает полный contract (`GBH-06`), consumers начинают применять ограничения (`GBH-07`), а `GBH-08` подключает одну общую subset-проверку для schema→Widget и declaration→child. Только после этого `GBH-02B` может вернуть `CollectionHost` в Designer. Так разрывается прежний цикл зависимостей.

## Задачи

- [x] **GBH-06 — Объявление несёт параметры своего вида**
  - Зависимости: `GBH-02`, только часть A (неполные structural kinds уже не selectable).
  - Тройка `(имя свойства, имя дочернего виджета, вид)` достаточна для видов без структуры и недостаточна для остальных. `REM-01` и `REM-05` — два следствия одного упрощения.
  - Done: объявление получает параметры, зависящие от вида: диапазон для числа и целого, `target_kind` для ссылки, для коллекции — `EntryWidgetClass`, `KeyPropertyName` и item contract; **каждое delegating declaration явно указывает `ChildCapabilityName` (или эквивалентный стабильный selector), а не только ChildWidget+Kind**, поэтому ребёнок с двумя capability одного вида не создаёт неоднозначности; параметры редактируются в Designer и скрываются для видов, к которым не относятся; **форма схемы, видимая автору контента, не меняется** — обогащается объявление, а не schema; предусмотрен способ наследовать ограничения от выбранной child capability без ручного дублирования либо документированно доказано, почему явное дублирование необходимо и как consistency gate предотвращает drift.
  - Evidence: `Source/GV2/Public/UI/GV2DeclaredCompositeWidgetBase.h`, `Content/`, `Docs/UI/ScreenTemplates.md`.
  - **Реализация (2026-09-01):**
    - `FGV2DeclaredUiCapability` получил `ChildCapabilityName` (`FName`, универсальный селектор), `NumberMin`/`NumberMax` (`double`, default `[0..1]`), `IntMin`/`IntMax` (`int64`, default `[0..100]`), `TargetKind` (`FString`, default `"resource"`) — все с `EditConditionHides` по `Kind`; и `EntryWidgetClass`/`KeyPropertyName` для `CollectionHost` — не подключены в `DescribeUiCapabilities` (вид остаётся `Hidden`, `GBH-02A`), присутствуют только чтобы `GBH-02B` не потребовал новой миграции структуры.
    - `NumberMin`/`NumberMax` default `[0..1]` — сознательный выбор (не универсально-безопасный "unbounded"): совпадает с `UGV2ProgressBarWidgetBase`'s собственным диапазоном, поэтому существующий `WBP_DeclaredCompositeFixture`/`FlatFixture` (`value → ValueBar`) получает верное ограничение из одной лишь UE-версионной десериализации новых полей структуры — без правки ассета через unreal-mcp (недоступен в этой сессии).
    - `DescribeUiCapabilities` (`GV2DeclaredCompositeWidgetBase.cpp`) передаёt эти поля в уже существующие параметризованные перегрузки `AddNumber`/`AddInteger`/`AddImage` — билдер их уже поддерживал (использовались `UGV2ProgressBarWidgetBase` и другими native виджетами), не хватало только реального значения на composite-стороне.
    - Дублирование значений (а не автовывод из capability ребёнка) — сознательное решение: автовывод воспроизвёл бы `UPP-R1` (сверка против самой себя). Consistency gate против дрейфа явно делегирован общей subset-функции `GBH-08` — не реализован здесь.
    - Новая `ChildCapabilityName` + `FGV2UiCapabilityBuilder::SetChildCapabilityName` + `ResolveDelegatedChildCapability` (`GV2UiCapability.h/.cpp`) заменяют прежний `DoesCapabilityTreeSupportKind`: явный selector резолвит по имени; иначе — по виду (ровно один кандидат резолвится однозначно); два и более без селектора пробуют `FallbackNameHint` (top-level `PropertyName` самого composite-свойства) — единственный источник обратной совместимости для уже существующего контента (`WBP_Duc10TabsHost`'s `default_tab_key` → `UGV2TabContainerWidgetBase`, объявляющего оба `default_tab_key` и `key` как `Key`, резолвится этим путём без миграции); не помогло — типизированный `core:diagnostic.ui_consumer.ambiguous_child_capability`, отдельный от `target_kind_mismatch`.
    - Побочный эффект, закрывающий подтверждённый пример `REM-01` **без изменений в `GBH-07`/`GBH-08`**: существующая, не изменённая `CheckUiSchemaCapabilityCompatibility` уже сравнивала диапазоны, но `Cap.NumberMin/Max` были не установлены — теперь установлены, и она сама отклоняет schema `[0..100]` против declared `[0..1]`. Фикстур-схема `GameData/textsystem/schemas/ui_field_declared_composite_fixture_v1.schema.json5` получила `min: 0.0, max: 1.0` на `value` (реальное ограничение существующего kind, не новая форма schema); три теста, submit'ившие `value: 5.0`/`7.0` для этой схемы, обновлены на значения внутри `[0..1]`.
    - Новый тест `GV2.UI.DeclaredComposite.ConstraintsAndSelector`: (1) declared `[0..1]` на реальном `UGV2ProgressBarWidgetBase` отклоняет schema `[0..100]` и принимает `[0..0.5]` — точно milestone-проверка REM-01; (2) неоднозначный `Key` на `UGV2TabContainerWidgetBase` без селектора и без совпадения по имени отклоняется; (3) тот же случай с явным `ChildCapabilityName = "key"` — принимается.
    - Red→green продемонстрирован дважды отдельно: диапазон (`AddNumber` временно без параметров — падение с assertion на `TOptional::GetValue()`, доказывающим реальное исчезновение диапазона) и disambiguation (`CandidateCount == 1` временно ослаблен до `>= 1` — упали ровно 2 ожидаемые assertion'а); оба восстановления — снова чисто.
    - Верификация: 104/104 UE Automation (`GV2.*`, headless `-nullrhi`), 68/68 `ctest`.

- [x] **GBH-07 — Consumer применяет объявленное ограничение**
  - Зависимости: GBH-06.
  - `FGV2NumberPropertyConsumer::Prepare` проверяет вид значения и сохраняет его; `NumberMin`/`NumberMax` объявленной capability не участвуют. То же следует проверить для целого и для `TargetKind` ссылки.
  - Done: значение, выходящее за объявленное ограничение capability, отклоняется в Prepare типизированной диагностикой с полным `property_path`, а не обрезается виджетом при отрисовке; проверены **все** consumers, чьи capability несут ограничения, а не только числовой; для каждого — отрицательный тест на значение вне границ; обрезание на стороне виджета остаётся как защита последней инстанции, но перестаёт быть местом, где ограничение впервые применяется.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-09-01):**
    - `FGV2NumberPropertyConsumer::Prepare` и `FGV2IntegerPropertyConsumer::Prepare`: кандидат теперь проверяется против `Capability.NumberMin`/`NumberMax` (соотв. `IntMin`/`IntMax`) до сохранения; вне границ — `core:diagnostic.ui_consumer.value_out_of_range`, называющий значение и превышенную границу. `property_path` добавляется на уровне `PrepareUiHostProperties` (уже существующий механизм, не требовал изменений).
    - `FGV2ImageResourcePropertyConsumer::Prepare`: сравнение `target_kind` входящего `StableId` с хардкодом `"resource"` заменено на сравнение с `Capability.TargetKind` — объявленным ограничением (`GBH-06`), а не литералом в коде consumer'а. Поведение для существующего контента не меняется (капабилити по умолчанию тоже `"resource"`), но проверка перестаёт быть завязанной на константу, игнорирующую declaration.
    - Полный аудит подтвердил: это единственные три consumer'а, чья capability несёт ограничение (диапазон/`target_kind`) — `bRequiresKeyedIdentity`/`KeyPropertyName`/`EntryWidgetClass` (`CollectionHost`) вне scope, вид остаётся `Hidden` до `GBH-02B`.
    - Widget-side clamp (`UProgressBar::SetPercent`, и т.п.) сохранён как защита последней инстанции — не удалён, но перестал быть местом первого применения ограничения.
    - Три новых отрицательных теста в `GV2.UI.StandardPropertyConsumers`: `ProgressBar` отклоняет `5.0`/`-0.5` при declared `[0..1]` без мутации физического `Percent`; `EditableTextBox`'s `max_length` отклоняет `50` при declared `[0..10]` без мутации `MaxLength`; `Portrait`'s resource consumer отклоняет `StableId(target_kind="item")` при declared `target_kind="resource"`, называя объявленный `target_kind` в диагностике.
    - Red→green: все три проверки одновременно обойдены недоказуемым компилятором `false`-условием — упали ровно 8 ожидаемых assertion'ов (по всем трём механизмам); восстановление — снова чисто.
    - Верификация: 104/104 UE Automation (`GV2.*`, headless `-nullrhi`), 68/68 `ctest`.

- [x] **GBH-08 — Сверка сравнивает capability целиком**
  - Зависимости: GBH-07.
  - `DoesCapabilityTreeSupportKind` реализует условие «тот же вид» вместо «capability композита ⊆ выбранная capability ребёнка».
  - Done: объявление указывает конкретную child capability, поэтому сверка однозначна и при нескольких capability одного вида. Вместо второй реализации «тех же правил» вводится **одна общая функция subset-совместимости** (например `IsUiCapabilitySubset(Required, Provided, OutDiagnostics)` над нормализованными capability descriptors), которой пользуются и schema→Widget compatibility, и DeclaredComposite→child compatibility. Она рекурсивно сравнивает как минимум: kind; integer/number min/max; `target_kind`; keyed identity flag; `KeyPropertyName`; collection item contract/entry capability; binding/input contract, если он представлен capability model; и все будущие constraint fields через completeness gate. Объявление шире выбранной child capability отклоняется до `Ready` с различимым кодом; более узкое принимается. Сужение DUC-07 только до `RendererControl` пересмотрено: structural target либо проходит ту же нормализованную subset-модель, либо остаётся Hidden по `GBH-02`. Существующие объявления мигрированы и покрыты тестом на неизменность поведения production Location screen.
  - Evidence: `Source/GV2/Private/UI/GV2UiCapability.cpp`, `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, `Content/TextSystem/UI/Widgets/`.
  - **Реализация (2026-09-01):**
    - `IsUiCapabilitySubset(Required, Provided, OutMismatch, OutDetail)` (`GV2UiCapability.h/.cpp`) — одна функция, сравнивающая kind, `TargetKind` (когда обе стороны его объявляют), `IntMin`/`IntMax`, `NumberMin`/`NumberMax`, `bRequiresKeyedIdentity` и рекурсивно один уровень `ItemCapability`, если он есть на обеих сторонах. Возвращает структурированную причину (`EGV2UiCapabilitySubsetMismatch`) вместо диагностического кода напрямую — каждый вызывающий маппит её в свой уже существующий, различный namespace кодов (`ui_capability.*` для schema↔Widget, `ui_consumer.*` для declaration↔child); unifying namespace'ов кодов не входило в Done и не делалось.
    - `CheckUiSchemaCapabilityCompatibility`: leaf-level (kind/target_kind/диапазон/keyed identity) проверки заменены на `ProjectSchemaFieldToCapability` (schema-поле → тот же `FGV2UiPropertyCapability` descriptor) + один вызов `IsUiCapabilitySubset`. Собственная рекурсия по вложенным `Object`-полям schema (`Items`) сохранена как есть — это структура schema field tree, а не capability-к-capability сравнение, и не сводится к тому же descriptor'у.
    - `GV2UiMutationPlan.cpp` (все три apply/reset места): после `ResolveDelegatedChildCapability` добавлен вызов той же `IsUiCapabilitySubset(Cap, *ResolvedChildCap, ...)` — declaration→child сверка перестаёт заканчиваться на кинде.
    - **Побочная находка — реальный use-after-free:** `ResolveDelegatedChildCapability(ChildBuilder.Build(), ...)` передавал temporary `FGV2UiCapabilityTree` (возврат `Build()` по значению) напрямую; `ResolvedChildCap` указывал внутрь него, а temporary разрушался по завершении вызова. До этой задачи никто не разыменовывал `ResolvedChildCap` после возврата, поэтому баг был latent; собственный subset-check стал первым кодом, читающим через указатель, и сразу проявил его как мусорные байты в `Provided.TargetKind` (сломало `PlayerStatus`/`Scene`/`RhStartOpensLocationScreen` и другие тесты на реальном content). Исправлено: дерево сохраняется в именованную локальную переменную (`const FGV2UiCapabilityTree ChildCapabilityTree = ChildBuilder.Build();`) на всё время жизни резолвленного указателя, во всех трёх местах.
    - **Сознательно не реализовано, задокументировано:** сравнение `KeyPropertyName` (актуально только для `CollectionHost`, который остаётся `Hidden` до `GBH-02B` — сравнивать сейчас нечего, поскольку ни одна selectable declaration не может объявить эту constraint) и completeness-гейт, гарантирующий, что каждое новое constraint-поле `FGV2UiPropertyCapability` обязательно попадёт в `IsUiCapabilitySubset` (симметрично `FGV2DesignerCapabilityKindGate`/PCC-05). Оба — прямое расширение существующей функции, не архитектурный редизайн; откладываются как задача без открытого риска сегодня, а не молча опускаются.
    - `DUC-07`'s сужение до `TargetType == RendererControl` пересмотрено и оставлено: структурная причина (CollectionHost/NestedScreen/CustomControl target — repeater/слот, а не значение того же смысла, что адресующая его capability) не зависит от глубины сверки и остаётся верной независимо от `GBH-08`.
    - Новый regression-тест (расширение `GV2.UI.DeclaredComposite.ConstraintsAndSelector`, секция 1c): declaration `[0..100]` на реальном `UGV2ProgressBarWidgetBase[0..1]`, со schema, специально построенной **под то же широкое объявление** (чтобы schema↔declaration сверка прошла чисто) — отклоняется через `PrepareUiHostProperties` с `core:diagnostic.ui_consumer.range_unsupported`; до `GBH-08` эта комбинация проходила бы (сверка заканчивалась на кинде).
    - **Mutation test** (Done milestone требование): единственная `IsUiCapabilitySubset` временно отключена (возврат `true` через недоказуемый компилятором guard) — упали ОБА assertion'а одновременно: GBH-06's schema↔Widget (`[0..100]` против `[0..1]`) и GBH-08's declaration↔child (та же пара) — доказательство единственной реализации правила, а не двух расходящихся. Восстановление — снова чисто.
    - Неизменность поведения production Location screen подтверждена существующим набором (`RhStartOpensLocationScreen`, `LocationScreenTransitionContract`, `CapabilityObservabilityCompositeSweep`), использующим реальный `WBP_LocationScreen`/`WBP_PlayerStatusPanel`/`WBP_SceneView` — все остаются зелёными без миграции контента (миграции и не требовалось: production объявления уже были unambiguous и in-range).
    - Верификация: 104/104 UE Automation (`GV2.*`, headless `-nullrhi`), 68/68 `ctest`.

## Проверка milestone

- [x] Объявление Number `[0..100]` на выбранной child capability `[0..1]` отклоняется; `[0..0.5]` принимается.
- [x] Значение вне объявленного диапазона отклоняется в Prepare, а не впервые обрезается widget renderer.
- [x] Ребёнок с двумя capability одного вида не создаёт неоднозначности: selector указывает конкретную capability.
- [x] Mutation test на schema→Widget и declaration→child краснеет при отключении **одной и той же** subset helper, доказывая отсутствие двух расходящихся реализаций.
- [ ] Collection item/key/entry constraints входят в ту же subset-модель; `CollectionHost` после этого проходит `GBH-02B` либо остаётся Hidden. (`KeyPropertyName` сравнение сознательно отложено — см. Реализацию `GBH-08`; `CollectionHost` остаётся `Hidden`.)
- [x] Форма схемы для автора контента не изменилась.
