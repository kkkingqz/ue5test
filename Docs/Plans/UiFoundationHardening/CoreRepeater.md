---
title: Core Repeater Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - README.md
  - ../../UI/WidgetRegistry.md
---

# M1 — Core Repeater

> **Материализует:** разделы 4 и 12 [предложения](../../Proposals/CoreUIBaselineAndScalingProposal.md).
> **Задачи:** UIH-01…04.
> **Результат:** повторяемый UI имеет один Core-owned механизм identity,
> reconciliation и widget reuse.

## Результат этапа

`FGV2KeyedCollection` перестаёт использоваться gameplay/textsystem composites
как собственный ad-hoc механизм. Core предоставляет reusable Repeater surface,
а concrete composite задаёт только модель entry и layout host.

## Задачи

- [ ] **UIH-01 — Сделать `UGV2ListViewWidgetBase` настоящим Repeater**
  - Done: Core component умеет принять ordered keyed entries, проверить
    non-empty/unique keys, создать отсутствующие widgets, переиспользовать
    существующие, удалить исчезнувшие и восстановить заданный order;
    реализация использует один `FGV2KeyedCollection`; layout host остаётся
    отдельной политикой и может быть `VerticalBox`, `HorizontalBox` или
    `WrapBox`; отказ происходит до частичной мутации.
  - Negative: empty key, duplicate key, отсутствующий host/class и failed
    entry apply не оставляют частично reconciled collection.
  - Evidence: `Source/GV2/Public/UI/GV2ListViewWidgetBase.*`,
    `Source/GV2/Private/UI/GV2ListViewWidgetBase.*`,
    `Source/GV2/Private/Tests/`.

- [ ] **UIH-02 — Перевести `CommandPanel` на Core Repeater**
  - Зависимости: UIH-01.
  - Done: `UGV2LocationCommandPanelWidgetBase` больше не содержит собственного
    `ButtonsByKey` и прямого вызова `FGV2KeyedCollection::Reconcile`;
    wrap/reflow команд сохраняется; button key остаётся identity;
    semantic binding и `OnBindingInvoked` не меняют смысл.
  - Evidence: `GV2LocationCompositeWidgetBases.*`,
    `Content/TextSystem/UI/Widgets/WBP_CommandPanel.uasset`,
    automation reuse/reorder test.

- [ ] **UIH-03 — Перевести item/effect collections на Core Repeater**
  - Зависимости: UIH-01.
  - Done: удалён локальный `ApplyIcons`; item и effect остаются двумя
    независимыми presentation collections, но обе используют один Core
    repeated-content mechanism; placeholder image проходит через общий image
    pipeline.
  - Evidence: `GV2LocationCompositeWidgetBases.*`,
    `WBP_PlayerStatusPanel.uasset`, automation.

- [ ] **UIH-04 — Сделать Character Layer повторяемой коллекцией**
  - Зависимости: UIH-01.
  - Done: SceneView не имеет single `Character` leaf, которому передаётся
    только `CharacterResourceIds[0]`; character layer является keyed
    collection через Core Repeater; первый gameplay slice может по-прежнему
    публиковать одного персонажа, но renderer структурно поддерживает `0..N`.
    Identity не выводится из текста или позиции.
  - Contract: если текущая `v1` schema не может выразить стабильный character
    key, вводится новая schema version; существующая `v1` не меняет смысл
    молча.
  - Evidence: field schema/view model, `GV2LocationCompositeWidgetBases.*`,
    `WBP_SceneView.uasset`, positive/negative reconciliation tests.

## Проверка milestone

- [ ] Core Repeater создаёт/reuses/removes/reorders children по key.
- [ ] Duplicate и empty key отклоняются до mutation.
- [ ] CommandPanel не имеет собственного keyed map.
- [ ] Item/effect collections не имеют собственного reconciliation helper.
- [ ] Character layer структурно является `0..N`, а не скрытым `0..1`.
