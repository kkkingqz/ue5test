---
title: Confirmed Contract Gaps
status: informative
version: 2.8
updated: 2026-09-03
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
| `STATUS-003` | `missing` | [UI Document and Reconciliation § Reconciliation](../UI/UIDocumentAndReconciliation.md#reconciliation), [UI README](../UI/README.md) | Contract описывал optional enter/exit анимацию реконсиляции (removed/stale screen блокирует input "до завершения exit animation"), но ни `FGV2LayeredUiReconciler`, ни вызывающий `UGV2RuntimeSubsystem`/`FGV2SessionCoordinator` не содержат animation-гейтинга: detach/attach выполняются синхронно, без ожидания. | `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp` (`CommitReconcile` не запускает и не ждёт анимаций), `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`; PCC-11 (2026-08-31) обнаружено при аудите фактического порядка шагов `CommitReconcile` против контракта. |
| `STATUS-008` | `known_nonconformance` | [ADR-0040 § Decision](../ADR/0040-universal-ui-property-pipeline.md#decision) (пункты 6, 7), [ADR-0040 § Implementation caveat](../ADR/0040-universal-ui-property-pipeline.md#implementation-caveat-ui-schema-authority-status-008) | Decision 6 (владение schema ID по namespace) и Decision 7 (политика отказа зависит от владельца схемы) сформулированы без оговорки, но `FGV2UiSchemaCache` резолвит `ui_field`/`ui_value` схемы статичным сканированием фиксированного списка файловых корней (`GameData/core`, `GameData/textsystem`, `GameData/rh`, `GameData/sample`), никогда не связанным с pinned `GameDataRepository`/package closure текущей сессии. Схема регистрируется по собственному полю `id`, без проверки, что её namespace совпадает с пакетом-источником, и независимо от того, принят или отклонён этот пакет как мод. Сегодня в проекте нет UI-схем, поставляемых модом (только `core`/`textsystem`/`rh`/`sample`), поэтому Decision 7 никогда не наблюдалась в действии для UI-схем. **Условие повторного открытия:** любой мод начинает поставлять `ui_field`/`ui_value` схему — тогда до реализации repository-owned closure несовместимая или чужого namespace схема мода не отклоняет мод отдельно от сессии, как обещает Decision 7, а её принятие полностью зависит от того, что физически лежит в просканированных директориях. | `Source/GV2/Private/UI/GV2UiSchemaCache.h` (doc comment: "self-identifying by their own `id` field, not by a manifest binding ... never bound to a DefinitionType the way RepositoryBuilder's `FSchemaRegistry` binds them"), `Source/GV2/Private/UI/GV2UiSchemaCache.cpp` (`EnsureDiscovered` — фиксированный `PackageRoots`, ноль namespace/package-acceptance проверок); GBH-03 (2026-09-01), REM-04. |
| `STATUS-009` | `known_nonconformance` | [ADR-0035 § Decision](../ADR/0035-ui-foundation-and-composition.md) (отказ от масштабирования кадра, раскладка распределяет доступное пространство viewport) | Порог переноса кнопок команд задан константой, не зависящей от выделенной ширины панели: `WBP_ListView_WrapButtons` имеет `bExplicitWrapSize=true`, `WrapSize=1200`. Значение равно ширине `CommandPanel` на 1280×720 за вычетом отступов; на 4K, QHD и ultrawide кнопки переносятся заметно раньше доступного места. Требование `ADR-0035` не имеет перечислителя: строка `ADR-0035` не встречается ни в одном тесте, а `WrapSize`/`bExplicitWrapSize` — нигде в `Source/`, `Tools/`, `Docs/`; все гейты проекта — текстовые сканы по `Source/` и `Docs/` и ассеты не читают. Сплошная проверка проекта (2026-09-03) дала ещё четыре места того же класса, задающие абсолютный размер из C++: `GV2RecoveryScreenWidget.cpp:42,53` (`SetSize(800×400)`, `SetPadding` — голые литералы), `GV2RichTextPopoverWidgetBase.cpp:69-70` (`SetMaxDesiredWidth(360)`/`Height(480)` из темы), `GV2DropdownSelectWidgetBase.cpp:216` (`SetMaxDesiredHeight(200)` из темы), `GV2SeparatorWidgetBase.cpp:25,29` (`SeparatorThickness=1.0`, вероятное исключение). Для popover и dropdown это прямое противоречие внутри `ADR-0035`: типографика масштабируется кривой `0.85 → 1.60` по высоте viewport, вмещающая её коробка — нет. **Условие закрытия:** `DCA-15`. | `Content/UI/Widgets/WBP_ListView_WrapButtons.uasset`; `Source/GV2/Private/UI/GV2RecoveryScreenWidget.cpp`, `Source/GV2/Private/UI/GV2RichTextPopoverWidgetBase.cpp`, `Source/GV2/Private/UI/GV2DropdownSelectWidgetBase.cpp`; `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp` (`FGV2LocationScreenViewportMatrixTest`, строгая проверка только при 1280×720); введено `c2526ae` (SVC-05, 2026-08-23) одним change set с добавлением этой проверки; разбор — [GV2_CommandPanelWrapSizeFixedNonScaling_2026-09-03.md](GV2_CommandPanelWrapSizeFixedNonScaling_2026-09-03.md). |

## Правило изменения

- Новый contract и полностью соответствующая реализация не создают строку.
- Частичная реализация создаёт или уточняет строку со ссылкой на точное правило и code/test evidence.
- Полное закрытие удаляет строку; история остаётся в commit и выполненном Plan summary.
- Предположение без проверки кода/tests сюда не добавляется.
