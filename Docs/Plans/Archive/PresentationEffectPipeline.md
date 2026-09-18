---
title: Presentation Effect Pipeline Plan — Archive
status: archived
version: 1.1
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

## Раунд приёмки перед архивацией

Первая архивация была выполнена и **откачена** (`295fb62` → `1b45e6b`): приёмка нашла пять расхождений, план переоткрыт, находки закрыты, архивация повторена. Полный разбор — [Presentation Effect Pipeline Acceptance Findings](../../Status/AuditFindings.md).

| Находка | Исход |
|---|---|
| `PEP-AF-01` — очередь эффектов не управляла ни одним физическим действием: hover открывался и закрывался до публикации, accepted и rejected давали одинаковый результат | Закрыта. `attach`/`detach` выполняет только accepted-эффект в `HandlePresentationEffects`; обе стороны различия проверяет `HoverEffectQueueContract` |
| `PEP-AF-02` — Lua-эффекты не имели самостоятельного дренажа и ждали будущего hover | Закрыта. Дренаж в `FGV2SessionCoordinator::DrainPresentationEffects` после каждого protected runtime entry; правило внесено в contract нормативно; `EffectsDrainAfterRuntimeEntry` доказывает путь без hover |
| `PEP-AF-03` — evidence `PEP-06B` слабее заявленного `Done` | Закрыта с поправкой к самой находке: дефекта продукта нет, свойство держалось всегда. Установлено двумя мутационными пробами (см. ниже) |
| `PEP-AF-04` — structural gate использовал запрещённый суррогат `sizeof` | Закрыта. `std::is_empty_v` в production плюс `Visit` с overload-визитором: третья alternative без ветки не компилируется |
| `PEP-AF-05` — owner contract содержал взаимоисключающие сведения | Закрыта. Факт разделён точнее: общий animated swap документных экранов не реализован и в baseline не входит; реальный промежуток между логическим и физическим снятием есть у host-local hover-окна |
| `PEP-AF-06` — диспетчеризация очереди идёт строковым сравнением `effect_id` без перечислителя | Вынесена в [`STATUS-029`](../../Status/ImplementationStatus.md) |

**Урок `PEP-AF-03`.** Находка предполагала возможный дефект продукта — «нижние слои не заблокированы» проверялось чтением `ESlateVisibility` вместо взаимодействия. Первая попытка усилить проверку маршрутизировала `RoutePointerDownEvent`/`RoutePointerUpEvent` через `SVirtualWindow` и падала: окно не зарегистрировано в `FSlateApplication`, поэтому capture и парность press/release, которых требует `SButton::OnClicked`, там смысла не имеют, а прецедента такой маршрутизации в suite нет — все прочие `SVirtualWindow` используются только для геометрии.

Дефекта продукта не оказалось. Две мутационные пробы: добавление `SelfHitTestInvisible` на корневую панель участника исхода не меняет (пустая область канваса в хит-тесте не участвует) — правка отвергнута как ничем не подтверждённая; отключение уже существовавшего `Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible)` в `AttachHostLocalScreen` красит ровно проверку достижимости. Итоговая проверка — реальный хит-тест `FSlateApplication::LocateWindowUnderMouse` по фактической arranged-геометрии с утверждением, что путь содержит нижний контрол.

Общее: **красный тест приёмки дважды оказался сообщением о самом тесте, а не о продукте**, и оба раза это выяснилось только мутационной пробой. Отказ проверки не доказывает дефекта механизма, пока не показано, что проверка краснеет именно на его отключении.

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
- Unreal Build Tool: success (Linux, Development); полный `Automation RunTests GV2`: `208/208`, exit code `0`, fresh-process.
- Множество automation test id за весь план стабильно от `PEP-04` до закрытия: `197 → 198 → 199 → 199 → 201 → 203 → 205 → 207`, каждое изменение названо и обосновано в Реализация-записях соответствующих задач; `PEP-10` не добавил и не удалил ни одного test id, раунд приёмки добавил один — `EffectsDrainAfterRuntimeEntry`, итог `208`.
- `Tools/Documentation/validate_docs.py`: `193` Markdown-файла, без ошибок.
- `STATUS-002` и `STATUS-003` сняты из `Docs/Status/ImplementationStatus.md` (строки удалены, не помечены «closed»). Открытым переносится `STATUS-029` — форма диспетчеризации очереди.

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

`source_commit`: `ff8aeb1deff0de7ec09812ac2deb50b15e6bae84` ([browse commit](https://github.com/kkkingqz/ue5test/commit/ff8aeb1deff0de7ec09812ac2deb50b15e6bae84)) — коммит, которым закрыт раунд приёмки и план отмечен полностью.

Единственный исходный path (`Docs/Plans/PresentationEffectPipeline/README.md`) проверен через `git cat-file -e <source_commit>:<path>` и восстановлен через `git show` для побайтного сопоставления с рабочим файлом перед удалением каталога.
