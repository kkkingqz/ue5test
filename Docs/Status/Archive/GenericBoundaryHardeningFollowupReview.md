---
title: Generic Boundary Hardening Follow-up Review Archive
status: archived
version: 1.0
updated: 2026-09-03
depends_on:
  - ../ImplementationStatus.md
decisions:
  - ../../ADR/0041-ui-commit-rollback-model.md
---

# Generic Boundary Hardening Follow-up Review Archive

> **Показывает:** итог независимой сверки follow-up review транзакционной модели UI и исторический record её закрытия.
> **Не является нормативным:** актуальные правила принадлежат [ADR-0041](../../ADR/0041-ui-commit-rollback-model.md), [UI Document and Reconciliation](../../UI/UIDocumentAndReconciliation.md) и [Universal UI Property Pipeline](../../ADR/0040-universal-ui-property-pipeline.md); действующие расхождения — [Implementation Status](../ImplementationStatus.md).

## Охват и метод

Исходное review проверяло residual defects после `GenericBoundaryHardening` на commit `d9280908a348b5a41b58284bac98a48ce46dfaeb`. Закрытие materialized план `GenericUiTransactionFollowUp` задачами `GBF-01…07`; независимая сверка `GBF-08` выполнена на implementation tip `453eb30` и зафиксирована в source commit ниже.

Метод включал current production-source inspection, source-derived enumerators, executed structural negative self-tests, UE assertions, portable CTest, `gv2-headless --check-scripts` и documentation validation. Для UE repairs record различает assertion, которая станет red при возврате старой production logic, от выполненного structural self-test: audit не создавал фиктивный временный C++ revert build.

## Счёт

| Категория исходной находки | Количество | Итог |
|---|---:|---|
| Implementation bug | 3 | устранены |
| Architectural implementation/lifecycle gap | 3 | устранены |
| Verification architecture defect | 1 | устранена |
| Всего | 7 | 7 устранены |

## Outcomes

| ID и исходная формулировка | Исход |
|---|---|
| `GBH-R1` — `AttachScreenToLayer()` возвращает success, если `UPanelWidget::AddChild()` вернул `nullptr` | *(Закрыто задачей GBF-01.)* `AttachScreenToLayer` распространяет `nullptr` как failure, а `CommitReconcile` восстанавливает Shell/metadata/`ActiveScreens`. `GV2.UI.LayeredReconciliationContract` проходит production path через real occupied `USizeBox`; возврат success-ветки делает assertion red. `validate_shell_attach_failure_consumption.py` выводит все `AddChild` в этой функции и его executed negative self-test отклоняет erased propagation и discarded result. |
| `GBH-R2` — rollback строится по current schema и неверно восстанавливает schema-switch | *(Закрыто задачей GBF-04.)* Committed tuple хранит previous value/schema/schema ID; generic inverse строится 1:1 прежней схемой или candidate reset. Schema A→B fault-injection в `GV2.UI.LayeredReconciliationContract` восстанавливает форму A и станет red при current-schema rollback. `GetUiMutationKindsRequiringInverse()` перечисляет kinds, а `GV2.UI.PrepareCommitAndFailureInjection` удаляет inverse каждого из них. |
| `GBH-R3` — higher-level rollback восстанавливает physical state, но не `LastCommittedProperties` | *(Закрыто задачей GBF-05.)* `RollbackFieldPlans` возвращает committed snapshot после physical inverse; document, keyed collection и nested tabs переиспользуют этот путь. UE checks охватывают reused screens, keyed item, nested child, schema metadata и next Prepare; удаление restore accounting делает их red. `EGV2UiRollbackBoundary` и source-derived boundary inventory перечисляют все шесть transaction roots. |
| `GBH-R4` — невозможность подготовить rollback только логируется и не блокирует transaction | *(Закрыто задачей GBF-04.)* Prepare теперь typed-rejects missing committed schema, inverse build failure и plan mismatch для screen host и reused keyed item. UE checks ожидают `core:diagnostic.ui_rollback.missing_committed_schema` и `core:diagnostic.ui_rollback.plan_mismatch`; warning-and-continue делает assertions red. Nullable rollback call остаётся только у one-property test-only observability probe, не у live multi-mutation transaction. |
| `GBH-R5` — `OnScreenFieldsApplied()` вызывается до окончательной публикации document transaction | *(Закрыто задачей GBF-06.)* Retired screen/tab callbacks удалены. `GV2.UI.LayeredReconciliationContract` enumerates all `Content` `.uasset`, сверяет с Asset Registry, инспектирует каждый Widget Blueprint generated class и требует zero implementers/null base API; возврат callback surface или implementation делает reflection assertions red. |
| `GBH-R6` — schema `keyed_by` не проецируется в `KeyPropertyName` capability descriptor | *(Закрыто задачей GBF-02.)* Generic schema projection сохраняет actual `keyed_by`, и shared subset rule сравнивает имя. `GV2.UI.PropertyHostAndCapabilities` отвергает `id` против `key` без runtime items; flag-only projection делает assertion red. Projection function и independent capability-member inventory закрывают весь class array keyed identity. |
| `GBH-R7` — `sizeof(FGV2UiPropertyCapability)` не является completeness gate | *(Закрыто задачей GBF-03.)* CTest inventory выводит все data members public capability struct и сравнивает independent classification table. Its executed negative self-test добавляет unknown member и удаляет classified member, требуя failure; padding/ABI больше не может скрыть change. |

## ADR-0041 и schema surface

Ни одна repair не сужает [ADR-0041](../../ADR/0041-ui-commit-rollback-model.md): partial state остаётся запрещённым, physical recovery replay-ит ordinary generic Prepare/Commit, а accounting возвращает captured committed tuple. Нет schema-specific rollback DTO, consumer или отдельного framework; `ui_pipeline_legacy_gate_contract` и его negative self-test запрещают рост schema-specific Prepare/Build, payload и DTO surface.

## STATUS-008

`STATUS-008` не закрывался и не является finding этого review. Параллельные commits `fe3cfdf`/`ade64db` изменили только его evidence и linked DCA closure tasks: Stable ID, `known_nonconformance`, normative requirement и verbatim reopening condition остались теми же. Current `GameData` содержит 12 `ui_field` в `core`, 5 в `textsystem`, ноль `ui_value` и ни одной mod-owned UI schema; semantic reopening condition не наступило.

## Verification

- `ctest --test-dir build --output-on-failure`: 74/74 passed.
- `gv2-headless --check-scripts`: `ok=true`, `modules_checked=42`.
- `python3 Tools/Documentation/validate_docs.py`: passed.
- `Automation RunTests GV2.UI`: 16/16 success, including `LayeredReconciliationContract`, `PrepareCommitAndFailureInjection`, `PropertyHostAndCapabilities` and `StandardPropertyConsumers`.
- Current and negative-self-test variants passed for shell attach failure consumption, capability member inventory, rollback boundary inventory and UI legacy gate.

## Source record

`source_commit`: `3f21074570ca1303a7c0f3965d8a4c0e4c6b444a` ([browse commit](https://github.com/kkkingqz/ue5test/commit/3f21074570ca1303a7c0f3965d8a4c0e4c6b444a)).

Before this archive removed the active review, its exact path was checked with `git cat-file -e 3f21074570ca1303a7c0f3965d8a4c0e4c6b444a:Docs/Status/GV2_GenericBoundaryHardening_Followup_Review_2026-09-02.md` and recovered for inspection with `git show`.
