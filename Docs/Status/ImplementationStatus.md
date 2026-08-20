---
title: Confirmed Contract Gaps
status: informative
version: 2.0
updated: 2026-08-20
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

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
