---
title: Swallowed Failure Tasks
status: active
version: 1.4
updated: 2026-08-31
depends_on:
  - README.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../Status/GV2_Universal_UI_Property_Pipeline_Review_2026-08-27.md
---

# M2 — Swallowed Failure

> **Материализует:** `UPP-R2`, `UPP-R7` в части reset, `UPP-R6`.
> **Задачи:** PCC-06…09.
> **Результат:** отказ невозможно потерять, а подготовка ничего не меняет.

## Результат этапа

`FGV2LayeredUiReconciler` вызывает `CommitScreenFields`, возвращающий `bool`, и отбрасывает результат. Это тот же проглоченный отказ, что закрывался в `REV3-05`, `REV3-06` и `BAI-05`, — теперь в реконсиляторе документа, сданном задачей `UPP-28` с DoD об атомарности.

Reset-мутация страдает симметрично: отсутствие target или consumer не отклоняет Prepare, а Commit молча пропускает такую мутацию.

Отдельно снимается нарушение чистоты подготовки: `DescribeUiCapabilities` объявлен `const`, но пятью `const_cast` создаёт внутренние репитеры.

Задачи этапа независимы и могут переставляться.

## Задачи

- [x] **PCC-06 — Результат подготовки и фиксации обязан быть потреблён**
  - `GV2LayeredUiReconciler.cpp:139` отбрасывает результат `CommitScreenFields`; результат присоединения экрана к слою рядом — тоже.
  - Done: результат каждого вызова, способного отказать, потребляется на всех путях реконсиляции документа; отказ приводит к поведению, описанному `ADR-0040` для нарушения инварианта Commit, а не к продолжению обхода; поведение подтверждено инъекцией отказа в один экран; отдельно введён гейт, краснящий сборку при отбрасывании результата любой функции `Prepare*`/`Commit*` UI-слоя — он же защищает от рецидива в новых местах.
  - Evidence: `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-08-31):**
    - `FGV2LayeredUiReconciler::CommitReconcile` (`GV2LayeredUiReconciler.h/.cpp`) получил параметр `FString& OutError` (ранее отсутствовал — сигнатура не могла сообщить об ошибке фазы Commit вообще) и опциональный `ScreenCommitFailureInjector` (`TFunction<bool(ScreenId, PropertyPath)>`, по умолчанию `nullptr`, зеркалит уже существующий injector `CommitUiHostProperties`; используется только тестами PCC-06/07).
    - Attach (`AttachScreenToLayer`) и Commit (`CommitScreenFields`) для каждого экрана теперь проверяются; первый отказ формирует диагностику `core:diagnostic.ui_reconcile.attach_failed`/`core:diagnostic.ui_reconcile.commit_failed` с `layer`/`instance_key`/`screen_id` и **немедленно** прерывает обход — шаги 4 (detach удалённых экранов), 5 (`ActiveScreens = Plan.NewActiveScreens`) и 6 (layer interactivity) не выполняются на этом пути, поэтому предыдущая ревизия `ActiveScreens` остаётся активной (ADR-0040: отказавший экран не публикуется).
    - Detach-вызовы (шаги 1 и 4 — очистка уже заменяемого/удаляемого виджета, не публикация) проверяются отдельно и **не** прерывают обход при `false` — логируются как предупреждение; обоснование зафиксировано в комментарии над каждым вызовом, а не подразумевается: это cleanup уже решённого прошлого состояния, а не commit нового.
    - Гейт компиляции: `[[nodiscard]]` добавлен на `PrepareScreenFields`/`CommitScreenFields`/`ApplyScreenFields`/`CanApplyScreenFields` (`GV2ScreenWidgetBase.h`), `AttachScreenToLayer`/`DetachScreen` (`GV2GameShellWidgetBase.h`), `PrepareReconcile`/`CommitReconcile`/`Reconcile` (`GV2LayeredUiReconciler.h`). Это, а не рантайм-гейт, — постоянный запрет: отбрасывание результата любой из этих функций красит сборку (`-Werror,-Wunused-result`), а не полагается на ревью глазами. Единственный найденный существующий discard (`Shell->DetachScreen(ProbeWidget)` в `GV2RuntimeSubsystemTests.cpp`, cleanup после probe-теста) исправлен потреблением результата.
    - Добавлен тест `Step I` в `GV2.UI.LayeredReconciliationContract` (`GV2RuntimeSubsystemTests.cpp`): реальный `UGV2LocationTopBarWidgetBase` (единственный вид виджета, реализующий `IGV2ScreenFieldHost`, кроме трёх других location-композитов) с `ScreenFieldId`/`DayText` выставленными через reflection (protected UPROPERTY, нет публичного сеттера) — baseline reconcile коммитит `day="Monday"`, второй reconcile с `ScreenCommitFailureInjector`, отказывающим коммит именно этого `screen_id`, подтверждает: `Reconcile` возвращает `false`; `ReconcileError` содержит `core:diagnostic.ui_reconcile.commit_failed` и `screen_id`; `IGV2UiPropertyHost::GetPropertyHostState().GetLastCommittedProperties()` для поля `day` **всё ещё "Monday"**, а не "Tuesday" — `SetLastCommittedProperties` вызывается только после успешного `CommitUiHostProperties`, поэтому непройденный инъекцией коммит физически не мог его переписать; виджет слота остаётся тем же экземпляром.
    - Красный тест на откате подтверждён: временно восстановлено discard-поведение (`CommitScreenFields`'s результат игнорируется, обход продолжается безусловно, за рантайм-флагом `-Pcc06NeverTrue` для обхода `-Werror=unreachable-code`, включённым по умолчанию) — `GV2.UI.LayeredReconciliationContract` красный ровно на трёх новых assertions (`Reconcile fails`, `diagnostic`, `screen_id`); откат снят, пересборка — снова зелёный.
    - Верификация: 95/95 UE Automation, 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.
    - **Передано PCC-07 и там закрыто:** после отказа Commit НЕ на первом экране многослойного документа ранее обработанные экраны в этом же вызове уже физически присоединялись к Shell, хотя `ActiveScreens` откатывалась целиком — устранено переупорядочением `CommitReconcile` в PCC-07 (все коммиты перед любым attach/detach).

- [x] **PCC-07 — Атомарность документа проверена, а не заявлена**
  - Зависимости: PCC-06.
  - `UPP-28` закрывался с утверждением «отказ подготовки любого экрана оставляет активный набор экранов и биндингов нетронутым». Утверждение относилось к фазе подготовки; отказ фазы фиксации не проверялся ничем.
  - Done: инъекция отказа **фиксации** одного экрана из нескольких слоёв проверяется отдельно от инъекции отказа подготовки; после отказа проверяется **состояние** остальных слоёв и предыдущей ревизии биндингов, а не их количество; блокировка нижних слоёв при модальном окне и её восстановление сохраняются; тест краснеет при возврате отбрасывания результата.
  - Evidence: `Source/GV2/Private/Tests/`, `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`.
  - **Реализация (2026-08-31):**
    - Найденный в PCC-06 разрыв подтверждён и устранён: `CommitReconcile` переупорядочен так, что **все** экраны документа коммитятся ПЕРВЫМИ (шаг 1), и только если каждый коммит прошёл успешно, выполняются detach/attach/`ActiveScreens`/layer interactivity (шаги 2-6). `Commit` мутирует только собственные bound-под-виджеты хоста (разрешаемые по имени через `GetWidgetFromName`), поэтому его не требуется откладывать до присоединения виджета к дереву Shell — переупорядочение не меняет поведение по успешному пути.
    - Следствие: при отказе Commit **любого** экрана ни один слой документа не тронут — ни Shell (детач/attach ещё не выполнялись), ни `ActiveScreens` (её присвоение — шаг 5, недостижим), ни layer interactivity (шаг 6). Это верно независимо от того, каким по счёту в документе идёт отказавший экран — устраняет ровно тот разрыв, что PCC-06 честно оставил открытым (частичное присоединение к Shell при отказе не первого экрана).
    - Добавлен тест `Step J` в `GV2.UI.LayeredReconciliationContract`: два слоя (`location_content`/`overlay_stack`), у обоих baseline и v2-кандидат с РАЗНЫМИ `screen_id` (принудительная замена виджета, не reuse — именно тот путь, где перепривязка Shell раньше могла быть частичной). Инъекция отказа коммита нацелена только на слой A (`core:screen.pcc07_a_v2`). После отказа проверено **состояние**, а не количество: `GetActiveScreen` для ОБОИХ слоёв — по-прежнему v1-виджет (не только "не null"); `Shell->GetScreensInLayer(...)` для ОБОИХ слоёв **содержит** v1-виджет и **не содержит** v2-виджет — то есть ни слой A (отказавший), ни слой B (не связанный, иначе успешный) не пострадали.
    - Красный тест на откате подтверждён вдвойне неожиданно точно: временный откат к старому «attach-затем-commit по одному экрану за раз, без переупорядочения» поведению (за рантайм-флагом, обход `-Werror=unreachable-code`) покраснел не только на ожидаемой assert'е "layer A всё ещё v1", но и на "layer B всё ещё v1" — старый код детачил ВСЕ заменяемые виджеты одним проходом (шаг, ранее шедший первым, до attach/commit для каждого экрана), включая слой B, ДО того как отказ слоя A вообще случался. Это подтверждает находку сильнее, чем ожидалось: старая структура была небезопасна даже без многослойного отказа — простое наличие replacement для НЕСВЯЗАННОГО слоя уже создавало частичное состояние при любом последующем отказе. Откат снят, пересборка — снова зелёный.
    - Верификация: 95/95 UE Automation, 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.
    - **Не входит в эту задачу (честно оставлено открытым):** отказ **Attach** (не Commit) всё ещё может оставить частично присоединённое состояние, если он падает не на первом экране, — `AttachScreenToLayer` возвращает `false` только при null-виджете или непривязанном host-панели слоя в Shell (структурная ошибка конфигурации Blueprint, не content-driven и не инжектируемая тестом per-screen данными способом, которым инжектируется Commit). Ни один существующий тест не воспроизводит отказ `AttachScreenToLayer`. Это уже нарисованный в PCC-06 комментарий над шагом 3 `CommitReconcile`, не новая находка здесь — и это единственный оставшийся путь к частичному состоянию после переупорядочения.

- [x] **PCC-08 — Reset подчиняется тем же инвариантам, что и apply**
  - Для отсутствующего опционального свойства план получает reset-мутацию, но отсутствие target или consumer её не отклоняет: `Commit` выполняет reset только при валидных обоих и молча продолжает иначе. Это та же фигура «принято и не потреблено», только на пути сброса.
  - Done: reset-мутация добавляется в план только когда consumer существует, target разрешается и сброс поддерживается явно; иначе Prepare отказывает с типизированной диагностикой; альтернативно сброс становится внутренней операцией самого consumer и тогда неразрешимого случая не существует — выбор обоснован в задаче, а не подразумевается; отрицательный тест: обязательный сброс при неразрешимом target отклоняется, а не пропускается.
  - Evidence: `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`.
  - **Реализация (2026-08-31):**
    - Выбор из двух альтернатив Done: **зеркалирование проверок apply-пути** в `PrepareUiHostProperties` (`GV2UiMutationPlan.cpp`), а не превращение сброса во внутреннюю операцию consumer'а. Обоснование: `IGV2PropertyConsumer::Reset` — чистый виртуальный метод (`= 0`), реализован во ВСЕХ консьюмерах без исключения, поэтому "сброс не поддерживается явно" как отдельная категория не существует — единственные реально возможные отказы совпадают один в один с уже проверяемыми на apply-пути (`missing_target`, `unsupported_kind`), и дублировать их внутри каждого consumer'а means умножать логику без причины.
    - В обеих ветках построения reset-мутации (`PrepareUiHostProperties`: "Absent optional property in candidate" и "Property not in schema: reused instance must reset") добавлены те же две проверки, что уже существовали для apply-ветки: `HostWidget && !TargetWidget` → `core:diagnostic.ui_consumer.missing_target`, `!Consumer` (от `FGV2PropertyConsumerFactory::CreateConsumer`) → `core:diagnostic.ui_consumer.unsupported_kind`. Обе — `return false` из `PrepareUiHostProperties`, а не пропуск построения мутации.
    - В `CommitUiHostProperties`: `else`-ветка для `Mutation.bIsReset` с невалидными `Consumer`/`TargetWidget` раньше молча ничего не делала. Теперь, поскольку Prepare гарантирует разрешимость каждой reset-мутации в плане, достижение этой ветки на Commit означает подлинную инвалидацию между Prepare и Commit (не предсказуемую content-ошибку) — она логируется и возвращает `false` с кодом `core:diagnostic.ui_mutation.reset_target_invalidated`, тем же способом, что и обычный (не-reset) отказ Commit рядом.
    - Добавлены тесты 10a/10b в `GV2.UI.PropertyHostAndCapabilities` (`GV2UiPropertyHostTests.cpp`): 10a (негативный) — capability с `TargetName`, не резолвящимся на хосте, и пустой Candidate (свойство отсутствует → путь reset) — `PrepareUiHostProperties` отклоняет с `missing_target`, план остаётся пустым. 10b (позитивный) — та же форма, но с реально резолвящимся target — `Prepare` принимает, план содержит ровно одну reset-мутацию — подтверждает, что фикс не блокирует легитимный сброс.
    - Красный тест на откате подтверждён: временный откат `GV2UiMutationPlan.cpp` к до-PCC-08 состоянию (через `git stash`) — тест 10a покраснел ровно на трёх новых assertions (`Prepare rejects`, `Diagnostic is missing_target`, `Plan is empty`); откат снят, пересборка — снова зелёный.
    - Верификация: 95/95 UE Automation, 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.

- [x] **PCC-09 — Объявление capability ничего не меняет**
  - `DescribeUiCapabilities` — `const`-метод, но пять раз обходит константность через `const_cast`, чтобы лениво создать внутренние репитеры (`GV2LocationCompositeWidgetBases.cpp:138,148,158,226,278`). Ленивая аллокация в чистом методе — та же ошибка, что снималась в `BAI-11`.
  - Done: подключение внутренних коллекций выполняется на этапе инициализации экземпляра, а не при опросе capability; `DescribeUiCapabilities` не содержит `const_cast` и не создаёт объектов; тест сравнивает состояние виджета до и после опроса capability и краснеет при возврате ленивого создания; проверка экземпляра из раздела 17.2 `ADR-0040` подтверждает наличие требуемых хостов коллекций до публикации экрана.
  - Evidence: `Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-08-31):**
    - Уточнение источника: "раздел 17.2 `ADR-0040`" в тексте Done — раздел `## 17.2. Instance wiring check` архивного `Docs/Proposals/Archive/UniversalDataDrivenUIPropertyPipelineProposal.md`, не самого ADR-0040 (там такого раздела нет). Формулировка раздела прямо предписывает найденный здесь фикс: "После создания WBP instance, но до публикации Screen: ... required collection host существует ... Никакого lazy `NewObject` из pure getter."
    - Все 5 `const_cast<...>(this)->ResolveXxxRepeater() != nullptr` в `DescribeUiCapabilities` (`UGV2LocationPlayerStatusWidgetBase` ×3 — meters/items/effects, `UGV2LocationSceneWidgetBase` ×1 — characters, `UGV2LocationCommandPanelWidgetBase` ×1 — items) заменены на уже существовавшие, но нигде не использовавшиеся const, без побочных эффектов методы `HasUsableXxxRepeaterHost()` (`XxxRepeater != nullptr || XxxContainer != nullptr` — булево тождественно тому же условию, что возвращал `Resolve...() != nullptr`, без объекта).
    - Реальное подключение (`NewObject<UGV2ListViewWidgetBase>` + `SetContainerPanel`) перенесено на этап инициализации экземпляра: каждый `ResolveXxxRepeater()` теперь вызывается эагерно (не через `const_cast`, `this` уже неконстантен) внутри `NativePreConstruct()` соответствующего класса — `UGV2LocationPlayerStatusWidgetBase` (все три репитера), `UGV2LocationSceneWidgetBase`, `UGV2LocationCommandPanelWidgetBase`. Сами `Resolve*()` методы не менялись — они и раньше были идемпотентны (создают `InternalXxxRepeater` только если он ещё `nullptr`), поэтому перенос точки первого вызова не меняет наблюдаемое поведение по успешному пути, только время создания.
    - Новый тест `GV2.Runtime.Presentation.LocationCompositeCapabilityQueryIsPure` (`GV2RuntimeSubsystemTests.cpp`): нативно сконструированный `UGV2LocationPlayerStatusWidgetBase` с `ItemIcons`, привязанным через reflection; `InternalItemRepeater` читается через reflection (protected, нет публичного геттера состояния) — подтверждено `nullptr` до `TakeWidget()` (запускает `NativePreConstruct` тем же путём, что и продакшн), не `nullptr` после — до единственного вызова `DescribeUiCapabilities`. Два последовательных вызова `DescribeUiCapabilities` сравниваются: указатель `InternalItemRepeater` идентичен во всех трёх точках (после construct, после 1-го запроса, после 2-го) — не просто "не null", а тот же самый объект.
    - Красный тест на откате подтверждён: временный откат `GV2LocationCompositeWidgetBases.cpp` к до-PCC-09 состоянию (через `git stash`) — новый тест покраснел ровно на assertion "InternalItemRepeater exists after construction, before any capability query" (репитер оставался `nullptr` до первого запроса capability, как и было до фикса); откат снят, пересборка — снова зелёный.
    - Верификация: 96/96 UE Automation (новый top-level тест), 68/68 Headless ctest, все 6 content/doc-гейтов зелёные.
    - **M2 (Swallowed Failure) закрыт целиком** — все четыре задачи (PCC-06…09) выполнены.

## Проверка milestone

- [x] Отбрасывание результата отказоспособной операции краснит сборку (`[[nodiscard]]` на всех `Prepare*`/`Commit*`/`Attach*`/`Detach*` UI-слоя, PCC-06).
- [x] Отказ фиксации проверен отдельно от отказа подготовки и по состоянию, а не по количеству (PCC-07: `Step J`, реордер `CommitReconcile`).
- [x] Неразрешимый reset отклоняется (PCC-08).
- [x] Опрос capability не меняет состояния виджета (PCC-09).
