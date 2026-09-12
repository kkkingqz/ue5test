---
title: Confirmed Contract Gaps
status: informative
version: 2.21
updated: 2026-09-12
depends_on:
  - ../README.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../UI/PresentationSnapshotAndEffects.md
  - ../ADR/0043-presentation-apply-boundary.md
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
| `STATUS-001` | `partial` | [Bootstrap and Session Lifecycle § Session states](../Architecture/BootstrapAndSessionLifecycle.md#session-states), [§ Replacement sequences](../Architecture/BootstrapAndSessionLifecycle.md#replacement-sequences) | В portable `FRuntimeSession` и `FGV2SessionCoordinator` реализованы discrete lifecycle phases (`Registering`, `BuildingState`, `RestoringInstances`, `Starting`, `PreparingPresentation`), phase callback, cancellation checkpoints, transition policy (join, supersede, last-wins, shutdown priority) и reverse module teardown (`stop`, `unregister`) (CFC-07). Отсутствует active-session preflight с captured bytes для load-another-save replacement flow (закрывается в `CFC-10`). | `FSessionStartDescriptor`, `RequestSession`, `CancelSessionRequest`, `FGV2SessionTransitionPolicy` в `Source/GV2/Private/Application/GV2SessionCoordinator.cpp` и `GV2SessionTransition.cpp`; automation тесты `GV2.Session.Transition.*`; preflight с captured bytes ожидает `CFC-10`. |
| `STATUS-002` | `missing` | [Presentation Snapshot and Effects § Effect](../UI/PresentationSnapshotAndEffects.md#effect), [§ ordering](../UI/PresentationSnapshotAndEffects.md#snapshoteffect-ordering) | UI document/reconciliation реализованы, но public one-shot effect DTO/queue/apply path, stale target handling и effect non-persistence tests отсутствуют. | В `Scripts/`, `Source/` и `Tests/` нет production `publish_effect`/effect queue consumer; `Scripts/authoring/presentation.lua` публикует только desired UI document. |
| `STATUS-003` | `missing` | [UI Document and Reconciliation § Reconciliation](../UI/UIDocumentAndReconciliation.md#reconciliation), [UI README](../UI/README.md) | Contract описывал optional enter/exit анимацию реконсиляции (removed/stale screen блокирует input "до завершения exit animation"), но ни `FGV2LayeredUiReconciler`, ни вызывающий `UGV2RuntimeSubsystem`/`FGV2SessionCoordinator` не содержат animation-гейтинга: detach/attach выполняются синхронно, без ожидания. | `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp` (`CommitReconcile` не запускает и не ждёт анимаций), `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`; PCC-11 (2026-08-31) обнаружено при аудите фактического порядка шагов `CommitReconcile` против контракта. |
| `STATUS-011` | `known_nonconformance` | [Universal UI Property Pipeline](../ADR/0040-universal-ui-property-pipeline.md), [UI Document and Reconciliation § Screen Fields](../UI/UIDocumentAndReconciliation.md) | `textsystem:schema.ui_field.location_scene.v1` объявляет `required: false` у **всех** полей верхнего уровня — `background_tile_resource_id`, `background_resource_id`, `context_text`, `characters`, `key`. Схема поля сцены не требует ничего, поэтому ревизия, в которой не пришло ни одного значения, проходит любую проверку обязательности этой схемы, и регрессия, при которой сцена перестаёт публиковаться, не отклоняется на границе. Для сравнения, `location_player_status.v1` и `location_commands.v1` объявляют обязательным по одному полю верхнего уровня. Обнаружено при закрытии проверки milestone M2 плана DeclaredCompositeAdoption: утверждение «каждое схемно-обязательное свойство пришло» для сцены оказалось пустым множеством. Обойдено в тесте вторым утверждением («композит получил хотя бы одно объявленное значение»), но это свойство теста, а не контракта. **Условие закрытия:** решено, какие поля сцены обязательны, и это записано в схеме; либо зафиксировано, почему сцена целиком необязательна, и чем тогда гарантируется её присутствие. | `GameData/textsystem/schemas/ui_field_location_scene_v1.schema.json5`; `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp` (`GV2.Runtime.Presentation.RhStartOpensLocationScreen`, метка `M2`); для контраста `ui_field_location_player_status_v1.schema.json5:10`, `ui_field_location_commands_v1.schema.json5:10`. |
| `STATUS-018` | `missing` | [Overview § Vertical slice acceptance](../Architecture/Overview.md#vertical-slice-acceptance), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md): product save/load через host storage | Storage и cold-start load реализованы в portable library, но игровая UE composition не вызывает `SetSaveSlotStorage`/`StartFromSave`. Сохранение при достижении storage получает `SaveWriteFailed:unavailable`; UE-пути загрузки слота нет. Общий replacement lifecycle отдельно ведётся в `STATUS-001`. | Production coordinator вызывает только `RuntimeSession.Start`; UE-вызов `SetSaveSlotStorage` найден только в `GV2LuaSpecRunnerHostTests.cpp`; native unavailable branch `GV2RuntimeSession.cpp:399`; [SAV-AF-01](AuditFindings.md#sav-af-01-p1-saveload-библиотека-не-подключена-к-игровой-ue-сессии). |
| `STATUS-019` | `missing` | [ADR-0021 § Decision](../ADR/0021-opaque-save-container.md#decision), [Canonical State and Save § Export boundary](../Architecture/CanonicalStateAndSave.md#export-boundary): сохранение предыдущей копии | `FFilesystemSaveSlotStorage::WriteSlot` атомарно заменяет слот, но после успешной замены backup предыдущих bytes отсутствует. Сохранность старых bytes при неуспешной записи является отдельным реализованным свойством. | Две успешные записи через реальный storage в temporary directory: один файл с текущими bytes, предыдущей копии нет; `GV2SaveSlotStorage.cpp:114`; [SAV-AF-02](AuditFindings.md#sav-af-02-p2-успешная-перезапись-слота-не-сохраняет-предыдущую-копию). |

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
