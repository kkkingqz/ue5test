---
title: Presentation Effect Pipeline Acceptance Findings
status: informative
version: 1.0
updated: 2026-09-18
depends_on:
  - ../Plans/Archive/PresentationEffectPipeline.md
  - ../UI/PresentationSnapshotAndEffects.md
  - ../UI/UIDocumentAndReconciliation.md
  - ../UI/WidgetRegistry.md
decisions:
  - ../ADR/0047-one-shot-effect-pipeline-and-origins.md
  - ../ADR/0048-widget-exit-lifecycle-and-input-gating.md
---

# Приёмка Presentation Effect Pipeline

> **Показывает:** находки повторной приёмки выполненного плана `PresentationEffectPipeline` перед его архивацией; документ не вводит новых правил.

## Охват и метод

Проверены task-level `Done`/`Evidence`, production call graph от обоих источников эффекта до физического применения, lifecycle host-local участника, нормативные contracts и соответствие тестов заявленным свойствам. Универсальные утверждения сверялись с фактическим перечислителем и production-путём.

## Findings

### PEP-AF-01 — Очередь эффектов не управляет ни одним физическим действием

`UGV2RuntimeSubsystem::DrainPresentationEffects` только вычисляет `ResolveEffectTarget` и сохраняет тестовую диагностику. Открытие и закрытие hover выполняются до публикации эффекта, поэтому accepted и rejected эффекты имеют одинаковый результат, а `FGV2PresentationEffectApply` не вызывается из queue consumer. Это противоречит ADR-0047 и `Done` PEP-07/PEP-10 про один apply path.

**Исход:** закрыт. `OpenHoverOverlay`/`CloseHoverOverlay` больше не выполняют attach/detach до публикации: открытие кладёт `request_id` в `Args`, публикует эффект и запрашивает общий drain, а `AttachHostLocalScreen`/`DetachHostLocalScreen` выполняет только accepted-эффект в `UGV2RuntimeSubsystem::HandlePresentationEffects`. `GV2.Runtime.Presentation.HoverEffectQueueContract` проверяет обе стороны различия: accepted queued close физически снимает адресованное окно, close со `StaleTarget` оставляет его в `overlay_stack`.

Часть формулировки про `FGV2PresentationEffectApply` в queue consumer закрыта иначе, чем предполагала находка, и это зафиксировано отдельно: очередь несёт сигнал открытия/закрытия, а `EPresentationEffectKind::Transparency` тикается покадрово владельцем состояния fade. Диспетчеризация сигналов в самом consumer осталась без перечислителя — выделено в `PEP-AF-06` ниже.

### PEP-AF-02 — Lua-effects не имеют самостоятельного production drain

Единственный production-вызов `TakePendingEffects` запускается только из `PublishHoverEffect`. Эффект, опубликованный Lua после semantic input, остаётся в Lua-очереди до будущего hover; без hover он не достигает UE вообще. Одновременно sequence Lua-эффекта назначается лишь при таком позднем pull, поэтому реальный порядок между источниками сохраняется только при ручном eager drain из теста.

**Исход:** закрыт. Дренаж перенесён в `FGV2SessionCoordinator::DrainPresentationEffects` и вызывается после каждого protected runtime entry — после `PublishReady` в `ExecuteSessionStart` и в `PumpIngress` — до несвязанной host-работы; host-local продюсер запрашивает тот же drain. Правило внесено в [Presentation Snapshot and Effects](../UI/PresentationSnapshotAndEffects.md) как нормативное. `GV2.Runtime.Presentation.EffectsDrainAfterRuntimeEntry` доказывает путь без единого hover; перечислитель production-вызовов `TakePendingEffects` по-прежнему даёт ровно один call site.

### PEP-AF-03 — Evidence PEP-06B слабее заявленного `Done`

Требование «нижние слои не заблокированы; проверено взаимодействием» заменено проверкой свойства `SelfHitTestInvisible`, а replacement-сценарий внутри теста заменён прямым `FGV2LayeredUiReconciler::Reset()`. Нужен реальный routed input через окно и production replacement/rebuild path.

**Исход:** закрыт, с поправкой к самой находке. Replacement-половина закрыта production-путём: `EndSession` уничтожает Shell с host-local окнами, следующий `StartSession` их не восстанавливает без нового hover (`GV2.Runtime.Presentation.HoverEffectQueueContract`); прямой `Reset()` оставлен отдельным узким тестом реестра.

Input-половина потребовала двух заходов. Первая редакция маршрутизировала `FSlateApplication::RoutePointerDownEvent`/`RoutePointerUpEvent` через `SVirtualWindow` и падала: окно не зарегистрировано в `FSlateApplication`, поэтому capture и парность press/release, которых требует `SButton::OnClicked`, там смысла не имеют, а прецедента такой маршрутизации в suite нет — все прочие `SVirtualWindow` используются только для геометрии. Заменено проверкой достижимости по реальному хит-тесту: `LocateWindowUnderMouse` обходит фактическое дерево с фактической видимостью, и утверждается, что путь содержит нижний контрол.

**Поправка:** находка предполагала возможный дефект продукта. Его нет — свойство держалось всегда. Проверено двумя мутационными пробами. Первая: добавление `SelfHitTestInvisible` на корневую панель участника не меняет исход теста — пустая область канваса в хит-тесте не участвует, поэтому правка была отвергнута как ничем не подтверждённая. Вторая: отключение уже существовавшего `Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible)` в `AttachHostLocalScreen` красит ровно проверку достижимости. Тест сторожит настоящий механизм, а не собственную формулировку.

### PEP-AF-04 — Structural gate двух видов ухода использует запрещённый суррогат

`HostLocalDepartureAcceptsInputIsExhaustive` вручную создаёт две альтернативы, а отсутствие поля в `FGV2StaleHostLocalDeparture` пытается доказать через `sizeof >= 1`. Такой `sizeof` не перечисляет поля и прямо запрещён repository rule; добавление нового alternative или поля не обязано покрасить этот тест. Нужен compiler-exhaustive visitor и `std::is_empty_v` для фактического свойства типа.

**Исход:** закрыт. В production-заголовке `GV2PresentationInteractionSink.h` стоит `static_assert(std::is_empty_v<FGV2StaleHostLocalDeparture>)`, а `GV2HostLocalDepartureAcceptsInput` использует `Visit` с `FGV2HostLocalDepartureInputVisitor`: третья alternative без собственного overload не компилируется. Запрещённый `sizeof`-суррогат удалён из теста.

### PEP-AF-05 — Owner contract содержит взаимоисключающие сведения о реализации

`UIDocumentAndReconciliation.md` после нормативного описания stale/self-dismissal всё ещё утверждает, что enter/exit animation не реализована и `STATUS-003` открыт, тогда как `BuildAndTooling.md`, план и текущий код утверждают обратное. До архивации contract должен быть приведён к одному текущему факту.

**Исход:** закрыт. Оба contract приведены к одному факту, и факт разделён точнее, чем в исходной формулировке: общий animated swap документных экранов не реализован и в baseline не входит — такие участники по-прежнему снимаются синхронно; реальный промежуток между логическим и физическим снятием реализован для host-local hover-окна. Правило двух видов ухода проверяется на production-потребителе, а не выполняется пусто.

### PEP-AF-06 — Диспетчеризация очереди идёт по строке без перечислителя

Выделено из `PEP-AF-01` при повторной проверке. `UGV2RuntimeSubsystem::HandlePresentationEffects` выбирает действие сравнением `EffectId` со строковыми литералами (`GV2RuntimeSubsystem.cpp`, ветки `core:effect.rich_text_hover_open` и `core:effect.rich_text_hover_close`), а нераспознанный accepted-эффект даёт `UE_LOG(Warning, "Unsupported presentation effect")` и молчаливое бездействие. Перечислителя множества queued `effect_id` в дереве нет.

Это дословно то, что `PEP-04` вынес в «не считается закрытием»: «диспетчеризация по строковому `effect_id` через if-цепочку — при добавлении вида молчит компилятор». Само `EPresentationEffectKind` с exhaustive dispatch существует и гейтом покрыто, но относится к apply-видам (`Transparency`), а не к queued-сигналам: это два разных множества, и перечислитель есть только у одного.

Ущерб отложенный: сегодня очередь несёт ровно два сигнала от одного продюсера, поэтому расхождение ненаблюдаемо. Оно станет наблюдаемым на первом же третьем сигнале, добавленном без ветки, — и проявится как бездействие, а не как отказ.

**Исход:** вынесено в [`STATUS-029`](ImplementationStatus.md). Закрытие плана этим не блокируется: множество queued-сигналов закрыто и покрыто тестами поимённо, а расхождение относится к форме диспетчеризации, а не к поведению. **Условие закрытия:** queued `effect_id` выводится из закрытого перечисления с гейтом полноты, и сигнал без ветки не проходит гейт вместо того, чтобы дать предупреждение в лог.
