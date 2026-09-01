---
title: Declaration Constraints Tasks
status: active
version: 1.1
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

- [ ] **GBH-06 — Объявление несёт параметры своего вида**
  - Зависимости: `GBH-02`, только часть A (неполные structural kinds уже не selectable).
  - Тройка `(имя свойства, имя дочернего виджета, вид)` достаточна для видов без структуры и недостаточна для остальных. `REM-01` и `REM-05` — два следствия одного упрощения.
  - Done: объявление получает параметры, зависящие от вида: диапазон для числа и целого, `target_kind` для ссылки, для коллекции — `EntryWidgetClass`, `KeyPropertyName` и item contract; **каждое delegating declaration явно указывает `ChildCapabilityName` (или эквивалентный стабильный selector), а не только ChildWidget+Kind**, поэтому ребёнок с двумя capability одного вида не создаёт неоднозначности; параметры редактируются в Designer и скрываются для видов, к которым не относятся; **форма схемы, видимая автору контента, не меняется** — обогащается объявление, а не schema; предусмотрен способ наследовать ограничения от выбранной child capability без ручного дублирования либо документированно доказано, почему явное дублирование необходимо и как consistency gate предотвращает drift.
  - Evidence: `Source/GV2/Public/UI/GV2DeclaredCompositeWidgetBase.h`, `Content/`, `Docs/UI/ScreenTemplates.md`.

- [ ] **GBH-07 — Consumer применяет объявленное ограничение**
  - Зависимости: GBH-06.
  - `FGV2NumberPropertyConsumer::Prepare` проверяет вид значения и сохраняет его; `NumberMin`/`NumberMax` объявленной capability не участвуют. То же следует проверить для целого и для `TargetKind` ссылки.
  - Done: значение, выходящее за объявленное ограничение capability, отклоняется в Prepare типизированной диагностикой с полным `property_path`, а не обрезается виджетом при отрисовке; проверены **все** consumers, чьи capability несут ограничения, а не только числовой; для каждого — отрицательный тест на значение вне границ; обрезание на стороне виджета остаётся как защита последней инстанции, но перестаёт быть местом, где ограничение впервые применяется.
  - Evidence: `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, `Source/GV2/Private/Tests/`.

- [ ] **GBH-08 — Сверка сравнивает capability целиком**
  - Зависимости: GBH-07.
  - `DoesCapabilityTreeSupportKind` реализует условие «тот же вид» вместо «capability композита ⊆ выбранная capability ребёнка».
  - Done: объявление указывает конкретную child capability, поэтому сверка однозначна и при нескольких capability одного вида. Вместо второй реализации «тех же правил» вводится **одна общая функция subset-совместимости** (например `IsUiCapabilitySubset(Required, Provided, OutDiagnostics)` над нормализованными capability descriptors), которой пользуются и schema→Widget compatibility, и DeclaredComposite→child compatibility. Она рекурсивно сравнивает как минимум: kind; integer/number min/max; `target_kind`; keyed identity flag; `KeyPropertyName`; collection item contract/entry capability; binding/input contract, если он представлен capability model; и все будущие constraint fields через completeness gate. Объявление шире выбранной child capability отклоняется до `Ready` с различимым кодом; более узкое принимается. Сужение DUC-07 только до `RendererControl` пересмотрено: structural target либо проходит ту же нормализованную subset-модель, либо остаётся Hidden по `GBH-02`. Существующие объявления мигрированы и покрыты тестом на неизменность поведения production Location screen.
  - Evidence: `Source/GV2/Private/UI/GV2UiCapability.cpp`, `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, `Content/TextSystem/UI/Widgets/`.

## Проверка milestone

- [ ] Объявление Number `[0..100]` на выбранной child capability `[0..1]` отклоняется; `[0..0.5]` принимается.
- [ ] Значение вне объявленного диапазона отклоняется в Prepare, а не впервые обрезается widget renderer.
- [ ] Ребёнок с двумя capability одного вида не создаёт неоднозначности: selector указывает конкретную capability.
- [ ] Mutation test на schema→Widget и declaration→child краснеет при отключении **одной и той же** subset helper, доказывая отсутствие двух расходящихся реализаций.
- [ ] Collection item/key/entry constraints входят в ту же subset-модель; `CollectionHost` после этого проходит `GBH-02B` либо остаётся Hidden.
- [ ] Форма схемы для автора контента не изменилась.
