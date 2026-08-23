---
title: Confirmed Contract Gaps
status: informative
version: 2.1
updated: 2026-08-23
depends_on:
  - ../README.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../UI/PresentationSnapshotAndEffects.md
---

# Подтверждённые расхождения contract и реализации

> **Показывает:** только проверенные незакрытые gaps между нормативным contract и текущим кодом/tests.
> **Не является нормативным:** целевое поведение задаёт linked owner contract; Plans и Proposals определяют будущую работу.
> **Обновляется:** gap добавляется при подтверждённом расхождении и удаляется тем же change set, который его полностью закрывает.

Отсутствие строки не доказывает полноту реализации. Реализованные возможности здесь не перечисляются: их evidence находится в коде и tests. Roadmap, приоритеты и идеи в этот документ не входят.

## Состояния

- `missing` — обязательная contract surface отсутствует.
- `partial` — существует только часть обязательного lifecycle/behavior.
- `known_nonconformance` — реализация существует, но наблюдаемо нарушает конкретное правило.

## Открытые gaps

| ID | Состояние | Нормативное требование | Точное расхождение | Evidence |
|---|---|---|---|---|
| `STATUS-001` | `partial` | [Bootstrap and Session Lifecycle § Session states](../Architecture/BootstrapAndSessionLifecycle.md#session-states), [§ Replacement sequences](../Architecture/BootstrapAndSessionLifecycle.md#replacement-sequences) | Реализован cold start `NewGame`/`LoadSave`, но coordinator не проводит session через `Registering`, `BuildingState`, `RestoringInstances`, `Starting`, `PreparingPresentation`; отсутствуют active-session preflight, cancellation и Menu↔Game/load-another-save/content-reload replacement flow. | `FGV2SessionCoordinator::StartSession` в `Source/GV2/Private/Application/GV2SessionCoordinator.cpp` выставляет `Creating`, затем сразу `Ready`; `FRuntimeSession::StartFromSave` покрыт `GV2ColdStartLoadConformance`, но production entry point replacement operation отсутствует. |
| `STATUS-002` | `missing` | [Presentation Snapshot and Effects § Effect](../UI/PresentationSnapshotAndEffects.md#effect), [§ ordering](../UI/PresentationSnapshotAndEffects.md#snapshoteffect-ordering) | UI document/reconciliation реализованы, но public one-shot effect DTO/queue/apply path, stale target handling и effect non-persistence tests отсутствуют. | В `Scripts/`, `Source/` и `Tests/` нет production `publish_effect`/effect queue consumer; `Scripts/authoring/presentation.lua` публикует только desired UI document. |
| `STATUS-003` | `known_nonconformance` | [UI README § Слои владения](../UI/README.md), [ADR-0035](../ADR/0035-ui-foundation-and-composition.md) | Contract требует, чтобы отсутствие объявления политики масштабирования примитива отклонялось. Выразить это нельзя: в `EGV2PrimitiveScalePolicy` нет значения «не задано», а поле объявлено как `ScalePolicy = EGV2PrimitiveScalePolicy::PreserveAspect`. Забывший объявить политику молча получает `PreserveAspect`; отклонять нечего, потому что отсутствовать нечему. | `Source/GV2/Public/UI/GV2ImageWidgetBase.h`, `Source/GV2/Public/UI/GV2ImageResourceCatalog.h`; введение sentinel-значения меняет default behavior существующих ассетов и требует ADR. |
| `STATUS-004` | `partial` | [Screen Templates § Apply lifecycle](../UI/ScreenTemplates.md) | Safety-часть контракта выполнена: `ApplyScreenFields` захватывает предыдущее значение каждого элемента и при отказе восстанавливает его, поэтому partial visual state наружу не выходит. Предиктивная часть не выполнена: публичный pure-preflight `CanApplyScreenFields` не может предсказать отказ глубокого ребёнка композита, так как `CanApplyScreenField` проверяет идентификатор поля и схему и детей не опрашивает. | `Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp`, `Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp`; поведение отката покрыто `GV2.Runtime.UI.CompositeRollbackContract`, предсказуемость отказа — ничем. |

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
