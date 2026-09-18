---
title: Presentation Effect Pipeline Plan — Archive
status: archived
version: 1.0
updated: 2026-09-18
depends_on:
  - ../../UI/PresentationSnapshotAndEffects.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../UI/WidgetRegistry.md
  - ../../UI/SemanticInput.md
  - ../../Architecture/Invariants.md
decisions:
  - ../../ADR/0040-universal-ui-property-pipeline.md
  - ../../ADR/0041-ui-commit-rollback-model.md
  - ../../ADR/0043-presentation-apply-boundary.md
  - ../../ADR/0044-session-replacement-and-registry-sealing.md
  - ../../ADR/0047-one-shot-effect-pipeline-and-origins.md
  - ../../ADR/0048-widget-exit-lifecycle-and-input-gating.md
---

# Presentation Effect Pipeline: итог выполнения

> **Материализует:** исторический итог введения одного механизма одноразовых эффектов презентации с двумя источниками и перевода всплывающего окна наведения из Slate-тултипа в обычный ui-блок; документ не является источником правил или задач.

## Цель и результат

**Цель.** Закрыть `STATUS-002` (раздел `Effect` контракта презентации не реализован ни строкой) и `STATUS-003` (жизненный цикл уходящего виджета) одним механизмом, которым пользуются все будущие эффекты независимо от источника, и сделать всплывающее окно обычным ui-блоком, а не особым C++-классом.

**Результат.** Один pipeline одноразовых эффектов с двумя равноправными источниками (Lua-биндинг и host-local), общей очередью, монотонным `sequence` и типизированным отбрасыванием по stale target/чужому поколению сессии/устаревшей ревизии; apply на Game Thread с exhaustive dispatch по закрытому перечислению видов; non-persistence проверена реальными `SaveToSlot`/`StartFromSave`. Всплывающее окно наведения — экземпляр экрана в `overlay_stack`, собранный из declared composites, позиционируется и ограничивается вьюпортом изнутри себя, второй вид окна добавлен без единой строки C++. Появление и уход ведут прозрачность 0↔100% за авторскую длительность из данных; уход отменяем и продолжает от текущего значения. Два вида ухода (`stale`/`self-dismissal`) выражены типом: интерактивный stale структурно невыразим, self-dismissal остаётся интерактивным до конца. `UGV2RichTextPopoverWidgetBase`/`WBP_RichTextPopover` удалены целиком; каждый унаследованный тест распределён поимённо. Полный прогон закрытия (`PEP-10`) дополнительно нашёл и исправил шесть предсуществующих архитектурных расхождений (GBF-07/PAH-08 маркеры, move-closure allowlist, regex-ложное срабатывание ownership-гейта), ни разу не пойманных портативным `ctest`, который за весь план не запускался.

## Этапы

| Этап | Итог |
|---|---|
| M0 — Решения до кода | `ADR-0047` (единый pipeline эффектов с двумя источниками) и `ADR-0048` (жизненный цикл уходящего виджета и правило ввода) приняты до какой-либо реализации. |
| M1 — Механизм эффектов | DTO, очередь, монотонный `sequence`, три типизированные причины отбрасывания, apply на Game Thread с exhaustive dispatch, non-persistence поведением через реальный save/load. |
| M2 — Окно как ui-блок | `hover` схемы `rich_text` несёт nested screen (v4); окно принято в `overlay_stack` как host-local участник слоя; собственный детектор наведения заменяет `IToolTip`; окно доведено до авторской полноты (второй вид без C++, удержание в границах вьюпорта, длительность в данных); hover стал первым host-local источником эффекта. |
| M3 — Затухание | Появление/уход 0↔100% за авторскую длительность с отменой от текущего значения; интерактивный stale сделан структурно невыразимым, self-dismissal остаётся интерактивным до конца. |
| M4 — Закрытие | Реализованные правила перенесены в contracts как нормативные; `STATUS-002` снят; `WidgetRegistry.md` приведён к факту; шесть предсуществующих gate-расхождений найдены полным прогоном и исправлены; test id сверены поимённо за весь план. |

## Задачи

| ID | Исходное название |
|---|---|
| `PEP-01` | Зафиксировать единый pipeline эффектов с двумя источниками |
| `PEP-02` | Зафиксировать жизненный цикл уходящего виджета и правило ввода |
| `PEP-03` | Реализовать DTO, очередь и порядок эффектов |
| `PEP-04` | Реализовать исполнение эффектов и non-persistence |
| `PEP-05` | Перевести содержимое hover на nested screen |
| `PEP-06` | Принять в слой участника, не пришедшего из документа |
| `PEP-06A` | Ввести собственный детектор наведения и якорь спана |
| `PEP-06B` | Переселить окно в `overlay_stack` |
| `PEP-06C` | Довести окно наведения до авторской полноты |
| `PEP-07` | Сделать hover host-local источником эффекта |
| `PEP-08` | Реализовать появление и уход с отменой |
| `PEP-09` | Развести ввод при двух видах ухода |
| `PEP-10` | Перенести правила в contracts и закрыть расхождения |

## Верификация

- Портативный `ctest`: `134/134` (100%), включая пять Python-гейтов, впервые исправленных при закрытии (`validate_ui_rollback_boundaries.py`, `validate_presentation_authority_phase.py`, `validate_apply_move_closure.py`, `validate_session_replacement_ownership.py`, `validate_property_consumer_transaction_coverage.py`) и десять штатно проверяемых гейтов плана — все с явным `--self-test` прогоном.
- Unreal Build Tool: success (Linux, Development); полный `Automation RunTests GV2`: `207/207`, exit code `0`.
- Множество automation test id за весь план стабильно от `PEP-04` до закрытия: `197 → 198 → 199 → 199 → 201 → 203 → 205 → 207`, каждое изменение названо и обосновано в Реализация-записях соответствующих задач; `PEP-10` не добавил и не удалил ни одного test id.
- `Tools/Documentation/validate_docs.py`: `192` Markdown-файла, без ошибок.
- `STATUS-002` и `STATUS-003` сняты из `Docs/Status/ImplementationStatus.md` (строки удалены, не помечены «closed»).

## Актуальные owner contracts

- [Presentation Snapshot and Effects](../../UI/PresentationSnapshotAndEffects.md)
- [UI Document and Reconciliation](../../UI/UIDocumentAndReconciliation.md)
- [Widget Registry](../../UI/WidgetRegistry.md)
- [Semantic Input](../../UI/SemanticInput.md)
- [ADR-0040: Universal UI Property Pipeline](../../ADR/0040-universal-ui-property-pipeline.md)
- [ADR-0041: UI Commit/Rollback Model](../../ADR/0041-ui-commit-rollback-model.md)
- [ADR-0043: Presentation Apply Boundary](../../ADR/0043-presentation-apply-boundary.md)
- [ADR-0044: Session Replacement and Registry Sealing](../../ADR/0044-session-replacement-and-registry-sealing.md)
- [ADR-0047: One-Shot Effect Pipeline and Origins](../../ADR/0047-one-shot-effect-pipeline-and-origins.md)
- [ADR-0048: Widget Exit Lifecycle and Input Gating](../../ADR/0048-widget-exit-lifecycle-and-input-gating.md)

## Source record

`source_commit`: `03698b4e074c78c309ebddc2ec82dd6a99b5bded` ([browse commit](https://github.com/kkkingqz/ue5test/commit/03698b4e074c78c309ebddc2ec82dd6a99b5bded)).

Единственный исходный path (`Docs/Plans/PresentationEffectPipeline/README.md`) проверен через `git cat-file -e <source_commit>:<path>` и восстановлен через `git show` для побайтного сопоставления с рабочим файлом перед удалением каталога.
