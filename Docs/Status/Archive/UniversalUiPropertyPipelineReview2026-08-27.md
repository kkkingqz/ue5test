---
title: UniversalUiPropertyPipelineReview2026-08-27 Archive Summary
status: archived
version: 1.0
updated: 2026-08-31
---

# Ревью Universal UI Property Pipeline 2026-08-27: итог раунда

> **Показывает:** исторический итог раунда внешнего ревью; документ не является источником правил или задач.

## Охват и метод

Внешнее ревью реализации `ADR-0040` на commit `06b3bd220d67d4ff36e3ede102b6223a0573902f` (ветка `main`), полученное сразу после закрытия и архивирования плана [UniversalUiPropertyPipeline](../../Plans/Archive/UniversalUiPropertyPipeline.md). Ревью проверяло не наличие отдельных классов/тестов, а выполнение архитектурных гарантий нового pipeline — и нашло 5 находок уровня P1/P2 на работе, закрытой с полностью зелёной верификацией.

Каждая находка проверялась по текущему коду. Пять из семи находок (`UPP-R1`, `UPP-R2`, `UPP-R3`, `UPP-R6`, `UPP-R7`) закрывались планом [PipelineClosureCorrection](../../Plans/Archive/PipelineClosureCorrection.md) (PCC-01…12); для каждой была отдельно проверена не только починка конкретного экземпляра дефекта, но и то, что закрыт весь класс — попытка воспроизвести дефект той же формы в другой точке pipeline отклоняется гейтом или физически невозможна. Эта проверка нашла один реальный, ранее не закрытый разрыв (см. `UPP-R6` ниже) и устранила его в том же раунде.

## Счёт

| Категория | Количество |
|---|---|
| Находок в ревью | 7 |
| Закрыто планом `PipelineClosureCorrection` (PCC-01…12) | 5 |
| Из них: разрыв «класс vs экземпляр» найден и устранён этой же сверкой | 1 (`UPP-R6`) |
| Активно ведётся другим планом (не входит в границы этого) | 1 (`UPP-R4`) |
| Осознанно отложено с явным условием повторного открытия | 1 (`UPP-R5`) |
| **Итог: без владельца или потерян** | **0** |

## Находки

| ID | Приоритет | Исходная формулировка | Исход |
|---|---|---|---|
| `UPP-R1` | P1 | Collection item schema не проверяется recursively против entry-widget capabilities; возможен silent property loss | Устранён `PCC-01`…`PCC-03`. Схема элемента коллекции приходит из скомпилированной схемы репозитория, а не из capability потребителя; проверка `Schema ⊆ Capabilities` рекурсивно доходит до элементов. Проверено: `GV2.UI.StandardPropertyConsumers`, `GV2.UI.PropertyHostAndCapabilities`. Независимая сверка `PCC-03` (2026-08-31) нашла и устранила разрыв в собственном доказательстве задачи: тест 8e проверял материализацию в обход `ValidateUiFieldValue`/`ProjectMaterializedValue` — добавлен тест 8f через реальную цепочку функций продакшна |
| `UPP-R2` | P1 | Commit и document reconciliation не обеспечивают заявленную atomicity | Устранён `PCC-06`, `PCC-07`. Результат каждого `Prepare*`/`Commit*`/`Attach*`/`Detach*` UI-слоя обязан быть потреблён (`[[nodiscard]]`, гейт компиляции — не point-fix, действует на весь модуль одинаково); `CommitReconcile` документа коммитит все экраны первым шагом, до detach/attach, что делает весь документ атомарным, а не только отдельный экран. Проверено: `GV2.UI.LayeredReconciliationContract` (Step I/J), подтверждено откатом (`git stash` на `GV2LayeredUiReconciler.cpp`) |
| `UPP-R3` | P1 | UE materializer использует второй validator вместо `ValidateUiFieldValue`; теряются min/max/default semantics | Устранён `PCC-04`. Второй валидатор (`WalkFieldValue`) физически удалён, а не оставлен параллельно; материализация значения — тем же переносимым `ValidateUiFieldValue`, что и headless-проверка. Проверено: `GV2.Runtime.Presentation.ScreenFieldUnifiedValidatorPcc04` (проверяет отсутствие исходного кода `WalkFieldValue`) |
| `UPP-R4` | P1 | `screen_fields` объявлен Core-kind, но production materializer его не поддерживает end-to-end | Вне границ `PipelineClosureCorrection` (явно записано в его `README.md` «Границы»). Активно ведётся планом [DataDrivenUiComposition](../../Plans/Archive/DataDrivenUiComposition.md) (материализует `UPP-R4` напрямую; `screen_fields` — задачи `DUC-09`…`DUC-11`, milestone M3 на момент этого раунда не закрыт) |
| `UPP-R5` | P1 | UI schema cache живёт отдельно от active repository/package closure; mod rejection policy не реализована | Осознанно отложена — записано в `README.md` обоих планов (`PipelineClosureCorrection` и `DataDrivenUiComposition`). Условие повторного открытия: становится обязательной, когда UI-блоки начнут поставляться модами (сейчас все блоки — проектные, closure repository не требуется) |
| `UPP-R6` | P2 | `DescribeUiCapabilities` / Prepare могут создавать internal repeaters и мутировать live UObject state | Устранён `PCC-09` (+ `PCC-12`). Wiring/init перенесены на этап инициализации экземпляра (`NativePreConstruct`); `DescribeUiCapabilities` стал read-only. Проверено: `GV2.Runtime.Presentation.LocationCompositeCapabilityQueryIsPure`. **Разрыв «класс vs экземпляр», найденный этой сверкой**: тест изначально проверял только `UGV2LocationPlayerStatusWidgetBase`, хотя фикс тронул 5 мест — `UGV2LocationSceneWidgetBase` и `UGV2LocationCommandPanelWidgetBase` были исправлены в коде, но не доказаны. Тест расширен на оба класса той же схемой в `PCC-12`; откат (временное отключение эагерного `Resolve*()` в `NativePreConstruct`) подтверждён красным для каждого класса отдельно, восстановление — зелёным |
| `UPP-R7` | P2/P3 | Reset path может silently succeed; contracts частично отстали от нового API | Устранён `PCC-08` (reset-инварианты в общих `PrepareUiHostProperties`/`CommitUiHostProperties` — отсутствие target/consumer теперь отказ Prepare, а не пропуск) и `PCC-11` (документация: `WidgetRegistry.md`, `ScreenTemplates.md`, `UIDocumentAndReconciliation.md` приведены к фактическому API и фактическому порядку `CommitReconcile` после `PCC-07`; заявленная, но нереализованная enter/exit-анимация экрана записана как `STATUS-003`, а не оставлена в тексте контракта). Проверено: `GV2.UI.PropertyHostAndCapabilities` (10a/10b) |

## Что раунд говорит о методе

Единственный найденный этим раундом самостоятельный разрыв (`UPP-R6`) — не новый дефект, а неполное доказательство уже сделанной починки: код был исправлен в пяти местах, тест доказывал ровно одно. Это тот же урок, что вывел предыдущий раунд ([ExternalReview2026-08-23](ExternalReview2026-08-23.md) — «зелёный тест, доказывающий более слабое свойство, чем заявлено»), только на этот раз — не слабее заявленного *свойства*, а у́же заявленного *охвата класса*. Отсюда практическое правило, применённое в `PCC-12`: закрытие инстанс-дефекта, тронувшего N call site одной формы, обязано либо тестировать все N, либо явно объяснить, почему часть из них не нуждается в отдельном доказательстве (как в случае `UGV2LocationTopBarWidgetBase`, у которого просто нет internal repeater — не пропуск, а архитектурное отсутствие цели).

Остальные четыре находки этого плана (`UPP-R1`, `UPP-R2`, `UPP-R3`, `UPP-R7`) закрылись способами, которые по построению исключают point-fix: физическое удаление второго пути (`UPP-R3`), компиляционный гейт на уровне объявления функции, а не call site (`UPP-R2`), общая функция, используемая каждым host/schema без исключения (`UPP-R7`), схема из репозитория вместо вывода из потребителя (`UPP-R1`) — для них отдельная проверка «класс, не экземпляр» была не нужна, эта гарантия следует из формы самого решения.

## Актуальные нормативные источники

- [Widget Registry](../../UI/WidgetRegistry.md)
- [Screen Templates](../../UI/ScreenTemplates.md)
- [UI Document and Reconciliation](../../UI/UIDocumentAndReconciliation.md)
- [Confirmed Contract Gaps](../ImplementationStatus.md)
- [PipelineClosureCorrection](../../Plans/Archive/PipelineClosureCorrection.md)
- [DataDrivenUiComposition](../../Plans/Archive/DataDrivenUiComposition.md)

## Полная история

`source_commit`: [8d5ec1961988e8607fe097236e91ca372e9a1006](https://github.com/kkkingqz/ue5test/commit/8d5ec1961988e8607fe097236e91ca372e9a1006)

[Полный текст ревью на source commit](https://github.com/kkkingqz/ue5test/blob/8d5ec1961988e8607fe097236e91ca372e9a1006/Docs/Status/GV2_Universal_UI_Property_Pipeline_Review_2026-08-27.md) содержит исходные формулировки, обоснования и предложенные regression cases для всех семи находок.
