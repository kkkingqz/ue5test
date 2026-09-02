---
title: GV2RemainingReview2026-09-01 Archive Summary
status: archived
version: 1.0
updated: 2026-09-02
---

# Ревью оставшихся проблем UI pipeline 2026-09-01: итог раунда

> **Показывает:** исторический итог раунда внешнего ревью; документ не является источником правил или задач.

## Охват и метод

Внешнее ревью на commit `15ae6c5c6c7097f5c3ff5fb310c194103f34a1f0` (ветка `main`), полученное после закрытия `PipelineClosureCorrection` и `DataDrivenUiComposition`. Ревью проверяло не наличие отдельных классов/тестов, а границы generic-абстракций — declared capability → реальная capability ребёнка, подготовленная мутация → транзакционный commit живого виджета, схемы на файловой системе → авторитет репозитория — и нашло 7 находок P1…P3 на работе, закрытой с полностью зелёной верификацией.

Каждая находка закрывалась планом [GenericBoundaryHardening](../../Plans/Archive/GenericBoundaryHardening.md) (`GBH-01…11`); для `REM-02` (самой глубокой архитектурной недоработки раунда) отдельно проверено, что закрыт весь класс дефекта, а не один экземпляр — существующая screen-level инъекция отказа (`PCC-07`) проверяла только замену widget instance (V1→V2, target off-tree до коммита), а не mid-Commit failure на уже живом переиспользуемом экземпляре. Новый danger-point test (`GBH-11`) на реально опасной точке подтверждён red-on-revert независимо от старого теста, который остаётся зелёным.

## Счёт

| Категория | Количество |
|---|---|
| Находок в ревью | 7 |
| Закрыто планом `GenericBoundaryHardening` (`GBH-01…11`) | 7 |
| Из них: architecture decision, зафиксированный отдельным ADR | 1 (`REM-02` → `ADR-0041`) |
| Осознанно отложено с явным условием повторного открытия | 1 (`REM-04`) |
| **Итог: без владельца или потерян** | **0** |

## Находки

| ID | Приоритет | Исходная формулировка | Исход |
|---|---|---|---|
| `REM-01` | P1 | `DeclaredComposite` проверяет child capability только по `kind` (`DoesCapabilityTreeSupportKind`), теряя range/`target_kind`/keyed-identity constraints; подтверждённый пример — `ProgressBar.percent[0..1]` против unbounded declared `Number` | Устранён `GBH-06`…`GBH-08`. Declaration несёт `NumberMin`/`NumberMax`/`IntMin`/`IntMax`/`TargetKind`/`ChildCapabilityName`; `DoesCapabilityTreeSupportKind` заменён `ResolveDelegatedChildCapability` + `IsUiCapabilitySubset` — одна нормализованная subset-проверка, общая с schema↔Widget. Проверено: `GV2.UI.DeclaredComposite.ConstraintsAndSelector` (declaration `[0..100]` на реальном `UGV2ProgressBarWidgetBase[0..1]` отклоняется до любой physical mutation) |
| `REM-02` | P1 | `CommitUiHostProperties`/`CommitScreenFields`/`CommitReconcile` не хранят previous physical state и не откатывают уже применённые mutations при mid-Commit failure на reused live screen — логически старая ревизия остаётся физически частично новой | Устранён `GBH-09` (решение — [`ADR-0041`](../../ADR/0041-ui-commit-rollback-model.md): захват `LastCommittedProperties`/структуры перед мутацией, откат в обратном порядке той же Prepare/Commit-машиной), `GBH-10` (реализация на всех шести обнаруженных границах: property, host/screen, документ, Shell attach, keyed collection item, nested screen в табе — включая реальный найденный попутный дефект: элементы keyed collection никогда не отслеживали `LastCommittedProperties`), `GBH-11` (danger-point test). Проверено: `GV2.UI.LayeredReconciliationContract` Step K — единственный тест именно на reused live screen с двумя field-хостами, mid-Commit failure после первой успешной мутации; red-on-revert продемонстрирован отдельно от старого `PCC-07` (Step J), который остаётся зелёным на том же коде — опасная точка подтверждена как действительно новая |
| `REM-03` | P2 | Последовательный `AttachScreenToLayer` в `CommitReconcile` может частично изменить Shell tree, если экран B не присоединяется после успешного присоединения A; `HasHostForLayer` уже существовал, но не использовался в Prepare | Устранён `GBH-01`. Все *предсказуемые* причины отказа attach (null widget, invalid layer, missing authored host) отклоняются в `PrepareReconcile` через `HasHostForLayer`; остаточный invariant-level случай покрыт `GBH-10`'s recovery моделью. Проверено: `GV2.UI.LayeredReconciliationContract` (GBH-01 блок) — layer с отсутствующим host отклоняет `PrepareReconcile` целиком, Shell tree и `ActiveScreens` не тронуты, точное совпадение с regression test, предложенным в самом ревью |
| `REM-04` | P1/P2 | UI schema cache (`GV2ScreenFieldMaterializer`/`FGV2UiSchemaCache`) резолвится статичным сканированием файловых корней, не связан с pinned `GameDataRepository`/package closure/mod accept-reject | Осознанно отложено `GBH-03` (вариант B — defer, зафиксировать честно). `STATUS-008` в `ImplementationStatus.md`; ADR-0040 получил секцию "Implementation caveat"; `ScreenTemplates.md`/`UIDocumentAndReconciliation.md` несут тот же caveat. **Условие повторного открытия:** любой мод начинает поставлять `ui_field`/`ui_value` схему. Regression gate: `Validation.validate_status_008_consistency` (`validate_docs.py`) — docs/status consistency gate вместо фиктивного runtime test отсутствующей фичи, явно разрешённый текстом самой задачи |
| `REM-05` | P2 | `CollectionHost` — публичный Designer-kind, но declaration не несёт `EntryWidgetClass`/item capability/key field contract; Prepare на действительно пустой коллекции возвращает `missing_entry_class` | Устранён `GBH-02A` (временно `UMETA(Hidden)`, разрывает цикл `GBH-02↔GBH-06`) + `GBH-02B` (вариант B — полный contract: `EntryWidgetClass`/`KeyPropertyName` на declaration, item capability читается из CDO `EntryWidgetClass`'s собственного `DescribeUiCapabilities`). Проверено: `GV2.UI.DeclaredComposite.CollectionHostFirstEntry` — первый элемент по-настоящему пустой коллекции создаётся через реальный `WBP_Button`/`UGV2ListViewWidgetBase` без authored dummy child |
| `REM-06` | P3 | `ScreenTemplates.md` описывал `schema_id`/optional host policy на уровне `IGV2ScreenFieldHost`, которых нет в фактическом API (`GetScreenFieldId()` — единственный метод) | Устранён `GBH-04`. `ScreenTemplates.md` переписан под фактическую строгую bijection без optional policy; `ADR-0011` получил caveat. Проверено: `GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure` (пункт 4) — обе стороны top-level bijection (configured host без envelope, envelope без host) отклоняются, не трактуются как optional |
| `REM-07` | P3 | Legacy `ApplyOptionalImageResource`/`ApplyOptionalPortrait` остались в публичном API вопреки `ADR-0040`, ожидавшему их удаления вместе с миграцией | Устранён `GBH-05`. Методы и `FGV2ImagePresentation::ResolveOptionalAndApply` физически удалены. Проверено: расширенный code-audit gate в `GV2PropertyConsumersTests.cpp` рекурсивно сканирует **весь** `Source/GV2` (не только известные call site) на эти три имени |

## Что раунд говорит о методе

Единственная находка, для которой понадобилась отдельная architecture decision (`ADR-0041`), а не просто point-fix — `REM-02` — оказалась воспроизведённой не в одном, а в **шести** независимых местах кода (property/host/screen/document/Shell-attach/keyed-collection/nested-screen), обнаруженных только при систематическом аудите каждой Commit-фазовой последовательности, а не при точечной починке единственного упомянутого в ревью сценария. Закрытие `GBH-10` попутно нашло собственный реальный дефект — ни один код никогда не вызывал `SetLastCommittedProperties` для элемента keyed collection, из-за чего построенный для него `RollbackPlan` был бы бессмысленным all-Reset, а не настоящим восстановлением — воспроизводя тот же урок предыдущих раундов: закрытие дефекта, тронувшего N мест одной формы, обязано покрыть все N, а не то одно, что назвало ревью.

Отдельно `GBH-11` подтвердило, что существовавший до этого regression test (`PCC-07`) доказывал более узкое свойство, чем заявлял: он проверял атомарность документа при **замене** экрана (off-tree candidate, безопасно по конструкции), а не при mid-Commit failure на уже живом **переиспользуемом** экземпляре — том самом сценарии, который `REM-02` называет напрямую. Явное сравнение "старый тест остаётся зелёным, новый — красный на том же откате" сделало разницу между двумя классами failure наблюдаемой, а не предполагаемой.

## Актуальные нормативные источники

- [UI Document and Reconciliation](../../UI/UIDocumentAndReconciliation.md)
- [Screen Templates](../../UI/ScreenTemplates.md)
- [Widget Registry](../../UI/WidgetRegistry.md)
- [ADR-0041: UI Commit Rollback Model](../../ADR/0041-ui-commit-rollback-model.md)
- [Confirmed Contract Gaps](../ImplementationStatus.md)
- [GenericBoundaryHardening](../../Plans/Archive/GenericBoundaryHardening.md)

## Полная история

`source_commit`: [7a76a69076222a44c3f66615fac62fb20e7e2951](https://github.com/kkkingqz/ue5test/commit/7a76a69076222a44c3f66615fac62fb20e7e2951)

[Полный текст ревью на source commit](https://github.com/kkkingqz/ue5test/blob/7a76a69076222a44c3f66615fac62fb20e7e2951/Docs/Status/GV2_remaining_review_2026-09-01.md) содержит исходные формулировки, обоснования и предложенные regression cases для всех семи находок.
