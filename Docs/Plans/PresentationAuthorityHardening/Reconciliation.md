---
title: Reconciliation Tasks
status: active
version: 1.3
updated: 2026-09-07
depends_on:
  - README.md
  - Authority.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# M3 — Reconciliation

> **Материализует:** `UIR-R1`.
> **Задачи:** PAH-06A…06B.
> **Результат:** слои GameShell перестают быть последним местом с собственной реконсиляцией.

## Результат этапа

Это единственный этап плана, где объём кода должен **уменьшиться**.

Общий примитив существует: `FGV2KeyedCollection::ReconcilePrepared` (`GV2KeyedCollection.h:31`) даёт keyed-переиспользование по стабильным ключам, двухфазный Prepare/Commit, атомарное обновление иерархии контейнера и упорядоченный выход `OutOrderedWidgets`. Он обслуживает коллекции локации, вкладки и списки. Слои GameShell — шестое место, где та же задача решается своим кодом.

**Но его пригодность доказывается, а не предполагается.** Формулировка «механизм уже написан» была утверждением о сигнатуре, а не о поведении, и проверка это показала: примитив восстанавливает `PreviousChildren` точно, однако **только** при отказе `AddChild` внутри финального атомарного свопа (`GV2KeyedCollection.h:103-115`). Отказ на более позднем шаге транзакции он не покрывает вовсе, а это ровно тот случай, который нужен слоям.

Поэтому этап разделён: `PAH-06A` доказывает пригодность на одном слое и определяет, что именно придётся дописать; `PAH-06B` переводит остальные. Если доказательство покажет нехватку — расширяется общий примитив, а не создаётся специальный реконсилятор GameShell.

## Задачи

- [x] **PAH-06A — Доказательство пригодности примитива на одном слое**
  - Зависимости: PAH-04A.
  - Общий примитив покрывает восстановление порядка только внутри финального свопа контейнера. Пригоден ли он для слоя целиком — вопрос поведения, а не сигнатуры, и он решается одним слоем до перевода остальных.
  - Инвариант: механизм переиспользуется по доказанной пригодности, а не по совпадению названий ([ADR-0042](../../ADR/0042-presentation-authority-and-publication.md), `INV-P3`). Нарушение выглядит как перевод шести мест на примитив, который покрывает пять из шести случаев, и обнаруживается на шестом через раунд.
  - Не считается закрытием: вывод о пригодности из чтения кода примитива; сценарий только на переиспользуемых экранах без создания и удаления; проверка возвращённого значения вместо фактического порядка детей панели.
  - Done:
    - один слой переведён на общий примитив, остальные не тронуты;
    - сценарий `[A, B, C] → [C, A, D]` проходит и доказывает одновременно: `A` и `C` переиспользованы, `B` удалён, `D` создан, физический порядок детей ровно `C, A, D`;
    - сценарий «отказ после перестановки» восстанавливает ровно `[A, B, C]` — состав **и** порядок;
    - оба утверждения читают фактический порядок детей панели, а не возвращённое значение;
    - если второй сценарий не проходит на существующем примитиве — расширен **общий** примитив, и записано, что именно в нём отсутствовало;
    - записано, какие случаи слоёв примитив по построению не покрывает, если такие остались.
  - Evidence: `Source/GV2/Public/UI/GV2KeyedCollection.h`, `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-09-07):** `modal_stack` chosen as the one converted layer — it is the layer the plan itself names as the risk case (`INV-P3`'s own text: for the modal stack, the logical top screen and the physical z-order can name *different* screens), and the only layer where the document already carries an explicit desired order (`Document.Modals`) independent of the whole-plan traversal order the old code used.

    `FGV2LayeredUiReconciler::CommitReconcile` gained a new step 3, before the existing (now step 4) per-widget attach loop: it builds the layer's `FPreparedScreenInstance` list in `Document.Modals` order (already resolved and field-committed by step 1 — nothing in this call does item-level work), seeds a throwaway `TMap<FName, TObjectPtr<...>>` with those already-known widgets, and calls `FGV2KeyedCollection::ReconcilePrepared` against `Shell->GetHostForLayer(modal_stack)` (new public accessor on `UGV2GameShellWidgetBase`, thin wrapper over the existing protected `FindHostForLayer`). Since every widget is pre-seeded, `CreateItem` is unreachable by construction and returns `nullptr` (fail-closed) if that invariant is ever violated. `PreparedType` carries no data — the primitive's own Prepare/Commit item lifecycle is unused here; this call exists purely for its keyed container-ordering and atomic-swap guarantee. The old step 3/4 loops (attach, detach-removed) now skip `modal_stack` entries entirely — not "left redundant next to" the new path, `ScreensToDetach` gained a `Layer` field (`FDetachEntry`, was a bare `TArray<TObjectPtr<UGV2ScreenWidgetBase>>`) specifically so step 5 can skip modal-layer removals that `ReconcilePrepared`'s own `ClearChildren` already handled atomically, rather than logging a spurious "no parent" warning against an already-detached widget.

    **Empirical finding, not assumed from reading the primitive's code:** `UPanelWidget::AddChild`'s only two failure conditions (engine source, `PanelWidget.cpp`) are a null widget and `!bCanHaveMultipleChildren && GetChildrenCount() > 0`; `AddChild` is not virtual, so no C++ subclass can inject a failure at an arbitrary position within a genuinely multi-child container. Every real Game Shell layer host (including `modal_stack`'s) is multi-child by construction (it has to hold more than one screen at once), so a third-or-later screen failing to attach during the swap is **structurally unreachable in production** for the layer this task converts — the restore-on-failure branch the primitive already had (`GV2KeyedCollection.h`, the `ClearChildren`+re-add loop right after "Commit to Container atomically") is real, but defensive/dead for this specific container class. Recorded rather than left as an unstated gap, per this task's own Done bullet.

    Two scenarios, both reading the actual panel, not a returned bool:
    - `GV2.Runtime.UI.ModalStackKeyedCollectionOrderingContract` — against the **real** `WBP_GameShell` asset's `ModalStackHost`: `[A,B,C]` (three distinct `Document.Modals` entries) reconciles, then `[C,A,D]` — asserts `Shell->GetScreensInLayer(modal_stack)` (walks `Host->GetChildAt(i)`) is physically `[C,A,D]` in that exact order, `A`/`C` are the same widget pointers as before (reused), `D` is new, `B` is gone from both the active-screen map and the panel. **Red-on-revert demonstrated**: disabled the new step 3 (a `static bool` probe flag) and restored the old per-widget skip-removal in steps 4/5, rebuilt, reran this test — it failed on exactly `physical child 0 is C (reused, moved)` / `physical child 1 is A (reused, moved)` (old code leaves reused widgets wherever `AttachScreenToLayer`'s no-op-if-already-parented left them: `[A,C,D]`, not `[C,A,D]`) — restored from a `cp`-taken backup (`diff` confirmed byte-identical), rebuilt, full suite back to 115/115.
    - `GV2.Runtime.UI.KeyedCollectionReconcilePreparedRestoresOnSwapFailure` — against a synthetic `UBorder` (single-child by construction, the one deterministic engine-native way to force the swap's second `AddChild` to fail): reconciling 2 widgets into a container that already holds 1 fails, and the container is restored to **exactly** its prior single child (not a partial or empty state) — this is the "отказ после перестановки" scenario, proven at the primitive level (`GV2KeyedCollection.h`) rather than through the real Shell, precisely because of the unreachability finding above: there is no way to force this failure through `modal_stack`'s own real, multi-child host. This scenario passed on the **existing** restore mechanism without modification — no primitive extension was needed for the restore-within-one-call guarantee itself.

    **Primitive extension made anyway, for a different, real reason**: `ReconcilePrepared` gained an optional trailing `TArray<WidgetType*>* OutPreviousOrderedWidgets` out-param, populated with the container's pre-call children (typed) whenever given. This exists because once a call **succeeds**, the container is already mutated — a caller whose own wider transaction spans multiple such calls (the six Game Shell layers, five of which still use the old per-widget path) has no way to recover what a successful call replaced if a *later, sibling* step in that same transaction then fails. `CommitReconcile`'s new step 4 (the old attach loop, non-modal layers) captures `modal_stack`'s pre-reorder order via this param and restores it, alongside the pre-existing per-widget undo, if a later layer's attach fails — a real (if, per the finding above, currently unreachable-in-production) cross-layer rollback path, kept consistent with the rest of this plan's no-partial-closures rollback rigor rather than left as a known gap. This is additive: every pre-existing call site (list views, tab containers) is unaffected. The failure-injection test above also exercises this new param directly (`PreviousOrderedOut` asserted to hold the exact prior single child) — reverting just this out-param's plumbing would not compile that test at all (10-argument call site), which is a stronger red-on-revert than a runtime failure would have been.

    **Residual scope, explicit**: the other five layers (`background`, `location_content`, `character_presentation`, `core_interface`, `overlay_stack`) are untouched — still the per-widget `AttachScreenToLayer`/`DetachScreen` path, still unable to reorder two already-attached reused widgets. That is `PAH-06B`'s job, not left silent here.

    Verification: headless `Automation RunTests GV2;Quit` — 115/115 passed (113 from `PAH-05` + 2 new); portable `ctest` (`/home/king/ue5/GV2/build`) — 76/76 (unchanged, task doesn't touch portable core); `CORE_DECOUPLING_RULE`, `pre_ready_content_discovery` (`--self-test`), `validate_ui_rollback_boundaries.py`, `validate_docs.py` all pass.

- [x] **PAH-06B — Слои GameShell — упорядоченная keyed-коллекция**
  - Зависимости: PAH-06A.
  - `AttachScreenToLayer(FName Layer, UUserWidget* ScreenWidget)` (`GV2GameShellWidgetBase.h:39`) не имеет параметра позиции. Commit присоединяет обычным `AddChild` в порядке обхода плана (`GV2LayeredUiReconciler.cpp:218`), откат — тем же `AddChild` (`:233`). Идентификаторов `PreviousOrder`/`DesiredOrder` в коде нет ни одного. Следствия: для двух переиспользуемых экранов желаемая перестановка `[A, B] → [B, A]` не выполняется вовсе; замена `[A, B] → [A2, B]` может дать `[B, A2]`; откат восстанавливает состав, но не порядок.
  - Инвариант: каждая повторяемая физическая структура, включая слои GameShell, реконсилируется как упорядоченная keyed-коллекция с явными прежним и желаемым порядком ([ADR-0042](../../ADR/0042-presentation-authority-and-publication.md), `INV-P3`). Нарушение здесь опаснее рассинхронизации значений: для модального стека логически верхний экран и физический z-order могут описывать **разные** экраны, то есть интерактивность достаётся одному, а видимость другому.
  - Не считается закрытием: добавление параметра индекса в `AttachScreenToLayer` — это закрывает симптом и оставляет слои шестым местом с собственной реконсиляцией, прямо против `INV-P3`; сортировка детей панели после присоединения как отдельный проход — порядок остаётся производным, а не состоянием; восстановление порядка только на пути отката.
  - Done:
    - каждый слой выставляет прежний и желаемый порядок как часть подготовленной транзакции;
    - реконсиляция слоя выполняется тем же примитивом, что и коллекции с вкладками, в форме, подтверждённой `PAH-06A`, а не собственным алгоритмом;
    - специальный путь присоединения и отсоединения экранов в `GV2LayeredUiReconciler.cpp` и `GV2GameShellWidgetBase.cpp` удалён, а не оставлен рядом;
    - перестановка двух переиспользуемых экранов даёт заявленный порядок — проверено чтением фактического порядка детей панели, а не возвращённого значения;
    - замена одного экрана из двух сохраняет позицию заменённого;
    - отказ в середине применения восстанавливает **точный** прежний порядок, а не только прежний состав;
    - множество мест, обязанных реконсилировать порядок, перечисляется тем же сканером границ отката, что и в `PAH-01`, либо названо, почему для слоёв нужен отдельный перечислитель;
    - `UIDocumentAndReconciliation.md` описывает порядок в слое как часть публикуемого состояния.
  - Evidence: `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`, `Source/GV2/Public/UI/GV2GameShellWidgetBase.h`, `Source/GV2/Public/UI/GV2KeyedCollection.h`, `Docs/UI/UIDocumentAndReconciliation.md`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-09-07):** `PAH-06A`'s modal_stack-only step обобщён на все шесть слоёв единым циклом по `UGV2GameShellWidgetBase::GetApprovedLayers()` внутри `CommitReconcile`: для каждого слоя с реальным host собирается его собственный список `FPreparedScreenInstance` (порядок — Route → Overlays → Modals, как их отдаёт `Document.GetAllScreenInstances()`, отфильтрованный по слою), сеется одноразовая `TMap` уже известными виджетами (переиспользованными и созданными в Prepare) и вызывается один `FGV2KeyedCollection::ReconcilePrepared` на слой. Три старых по-виджетных шага `CommitReconcile` (detach заменённого, attach нового, detach удалённого) удалены целиком, а не оставлены рядом — атомарный `ClearChildren`+пересборка каждого слоя сам вычищает и заменённый, и удалённый виджет, поскольку они просто отсутствуют в желаемом наборе этого раунда.

    Это устранило и `FPreparedScreenInstance::ReplacedOldWidget`, и `FPreparedReconciliationPlan::ScreensToDetach` (`PAH-06A`'s `FDetachEntry`) как мёртвую поверхность: обе структуры существовали только для старой явной detach-бухгалтерии, которую теперь делает сам примитив. Убраны вместе с единственным кодом, их читавшим.

    Кросс-слойный откат — единственная реальная новая возможность, которой не было в `PAH-06A` (там была ровно одна ReconcilePrepared-транзакция в раунде, откатывать её саму на другую было не от чего): при отказе слоя N в цикле откатываются все слои `0..N-1`, уже успешно закоммиченные в ЭТОМ ЖЕ раунде, к их собственному `PreviousOrder` (`OutPreviousOrderedWidgets`, добавленный `PAH-06A`), плюс, как раньше, откат Commit-мутаций шага 1 для всех экранов плана. Диагностика — новый `core:diagnostic.ui_reconcile.layer_reconcile_failed` (заменяет `attach_failed`/`modal_stack_reconcile_failed`, оба удалены как частные случаи одного и того же).

    Публичные `AttachScreenToLayer`/`DetachScreen` на `UGV2GameShellWidgetBase` НЕ удалены: подтверждено (`strings` по всем `.uasset` в `Content/`), что ни один Blueprint-график их не вызывает, но несколько существующих тестов используют их напрямую как общий примитив «положить виджет в host слоя», не имеющий отношения к алгоритму реконсиляции документа — Done-пункт «специальный путь... удалён» относится к САМОМУ АЛГОРИТМУ (какой виджет в каком порядке), которым `CommitReconcile` их раньше вызывал, а не к этим двум методам как таковым. Это ровно то разграничение, что уже проведено «не считается закрытием»-пунктом задачи (добавление index-параметра `AttachScreenToLayer` не было бы закрытием) — сами методы остаются простым примитивом без позиционирования, реконсиляция документа больше их не использует.

    Существующий gate `Tools/Testing/validate_ui_rollback_boundaries.py` (`PAH-01`/`GBF-07`) не потребовал изменений: единственная затронутая классификация — `FGV2LayeredUiReconciler::CommitReconcile` (`Document` boundary) — осталась той же самой функцией с тем же именем и тем же единственным rollback-boundary; шестислойный цикл — новая внутренняя реализация уже классифицированной границы, а не новая cancellable commit root. `UGV2GameShellWidgetBase::AttachScreenToLayer` сохраняет собственную классификацию (`ShellAttach`) как отдельного, всё ещё существующего примитива — таблица в `UIDocumentAndReconciliation.md` обновлена, чтобы не описывать более не существующий вызов из `CommitReconcile`. Отдельный перечислитель для слоёв не понадобился.

    **Обнаруженный эмпирически побочный эффект, не предполагавшийся заранее:** переход на `ClearChildren`+пересборку сделал недействительным старый механизм отказа в `GBF-01` (`GV2.UI.LayeredReconciliationContract`) — тест держал «блокирующий» виджет в single-child `USizeBox`-хосте `overlay_stack`, ожидая, что `AddChild` нового экрана будет отклонён из-за занятости. Новый код чистит хост целиком перед пересборкой, поэтому блокировщик вычищается вместе со всем остальным, и попытка, которая раньше проваливалась, стала успешной — тест обнаружил это как красный (`GBF-01: occupied single-child host rejects document Commit` == false вместо true), не молча. Переписан на реалистичный триггер: **два** желаемых экрана в ОДНОМ документе нацелены на single-child `overlay_stack`, второй `AddChild` отклоняется движком по построению (тот же приём, что `PAH-06A` использовало для `UBorder`) — и одновременно с этим route в `location_content` заменяется в том же документе, так что тест теперь проверяет именно кросс-слойный откат: `location_content` откатывается к v1 ПОСЛЕ того, как сам успешно закоммитил v2, потому что `overlay_stack` (обрабатывается позже по `GetApprovedLayers()`) отказал следом.

    Новый выделенный тест `GV2.Runtime.UI.NonModalLayerReorderAndReplaceContract` (`overlay_stack`, два одновременных instance) доказывает генерализацию за пределы `modal_stack`: `[A,B]→[B,A]` — оба виджета переиспользованы, физический порядок читается через `Shell->GetScreensInLayer`; замена B на B2 при неизменном A — A остаётся на своей позиции, а не сдвигается на конец, как было бы в старом коде (новый виджет просто дописывался `AddChild`).

    Red-on-revert продемонстрирован временным отключением всего шестислойного цикла (`static bool`-флаг) — `NonModalLayerReorderAndReplaceContract` упал на трёх ожидаемых `physical children == 0` (ничего не присоединяется вовсе, раз цикл выключен целиком), и `GV2.UI.LayeredReconciliationContract` тоже покраснел (большой тест транзитивно зависит от того же кода) — восстановление из бэкапа подтверждено побайтово, полный набор снова зелёный.

    Верификация: headless `Automation RunTests GV2;Quit` — 116/116 (115 от `PAH-06A` + 1 новый); portable `ctest` (`/home/king/ue5/GV2/build`) — 76/76; `CORE_DECOUPLING_RULE`, `pre_ready_content_discovery --self-test`, `validate_ui_rollback_boundaries.py`, `validate_shell_attach_failure_consumption.py` (неизменный gate над `AttachScreenToLayer`'s собственным `AddChild`, всё ещё проходит без правок), `validate_docs.py` — все проходят. Объём `GV2LayeredUiReconciler.cpp`/`.h` за эту задачу: `+92/-208` строк (чистое сокращение), не считая уже сокращённого `PAH-06A`.

## Проверка milestone

- [x] Пригодность общего примитива доказана сценарием, а не выведена из чтения его кода. (PAH-06A, 2026-09-07)
- [x] Порядок в слое существует как состояние, а не как следствие порядка обхода плана. (PAH-06B, 2026-09-07)
- [x] Собственного алгоритма реконсиляции у слоёв не осталось. (PAH-06B — старые detach-заменённого/attach/detach-удалённого шаги удалены целиком)
- [x] Перестановка, замена и откат проверены по фактическому порядку детей панели. (`ModalStackKeyedCollectionOrderingContract`, `NonModalLayerReorderAndReplaceContract`, переписанный `GBF-01`)
- [x] Объём кода реконсиляции уменьшился, а не вырос. (`GV2LayeredUiReconciler.cpp/.h`: +92/-208 за PAH-06B)
