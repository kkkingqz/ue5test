---
title: Screen and Document Transaction Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - LocationScreen.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../Status/ImplementationStatus.md
---

# M7 — Screen and Document Transaction

> **Материализует:** фазу 6 proposal, разделы 19, 20, 21.
> **Задачи:** UPP-27…29.
> **Результат:** атомарность перестаёт быть компенсирующим откатом и становится свойством конструкции.

## Результат этапа

Сегодня безопасность обеспечивается компенсацией: `ApplyScreenFields` захватывает предыдущее значение каждого элемента и восстанавливает его при отказе. Это работает, но оставляет два незакрытых свойства — публичный preflight не предсказывает отказ глубокого ребёнка (`STATUS-004`), а замена экранов в слоях неатомарна.

Prepare/Commit убирает обе причины: к моменту Commit отказать нечему, а старый экран не отсоединяется, пока новый не подготовлен полностью.

## Задачи

- [x] **UPP-27 — Экран на Prepare/Commit и закрытие `STATUS-004`**
  - Зависимости: UPP-26.
  - Contract требует предиктивного preflight, но `CanApplyScreenField` проверяет идентификатор поля и схему и детей не опрашивает, поэтому отказ глубокого ребёнка предсказать нельзя.
  - Done: `UGV2ScreenWidgetBase` переведён на подготовку полного плана мутаций до единственного Commit; публичный preflight предсказывает отказ **глубокого** ребёнка — регрессионный сценарий «схема проходит, ребёнок отказывает при commit» больше не воспроизводится и покрыт тестом; компенсирующий захват предыдущих значений удалён как ненужный, а не оставлен «на всякий случай»; строка `STATUS-004` удалена.
  - Evidence: `Source/GV2/Private/UI/GV2ScreenWidgetBase.cpp`, `Docs/Status/ImplementationStatus.md`.
  - **Реализация (2026-08-25):** `IGV2DynamicScreenElement` и `FGV2ScreenFieldDescriptor` удалены целиком (0 виджетов их реализовывало). Новый `IGV2ScreenFieldHost` (`Source/GV2/Public/UI/GV2ScreenFieldHost.h`) заменяет сканирование дерева по старому интерфейсу; композиты LocationScreen отвечают на него через `ScreenFieldId`, настроенный per-instance в WBP_LocationScreen через unreal-mcp. `ApplyScreenFields`/`CanApplyScreenFields` теперь готовят план мутаций для КАЖДОГО поля через `PrepareUiHostProperties` (которая уже рекурсивно валидирует вложенные keyed-коллекции) ДО единственного Commit; ни одного виджета не трогает, пока не подготовлены все поля. Компенсирующий `PreviousValue`/rollback полностью убран — восстанавливать после Commit больше нечего, потому что к этому моменту отказать уже нечему. Регрессия `GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure` строит схема-валидное поле "commands" с дублирующимся ключом внутри вложенного keyed-массива items (ошибка, которую видно только рекурсией в коллекцию, не по field_id/schema_id) и проверяет, что `CanApplyScreenFields` предсказывает отказ ДО `ApplyScreenFields`, а сам `ApplyScreenFields` не оставляет частичной мутации.
    Попутно обнаружен и исправлен реальный регресс в самом консьюмере изображений (`FGV2ImageResourcePropertyConsumer::Commit`, `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`): он обходил `UGV2ImageWidgetBase`/`UGV2PortraitWidgetBase` напрямую через их внутренний `UImage*`, из-за чего `AppliedResourceId`/`GetPortraitResourceId()` никогда не обновлялись через Prepare/Commit-путь —броский эффект замечен именно на LocationScreen (Market-фон оставался пустым после travel), но баг общий для любого экрана. Теперь Commit вызывает `ApplyImageResource`/`ApplyPortrait` хоста напрямую.
    Схема резолвится генерически через новый `FGV2UiSchemaCache` (`Source/GV2/Private/UI/GV2UiSchemaCache.h/.cpp`) — сканирует `*.schema.json5` по `id`, компилирует лениво через `GV2ContentCore::CompileUiFieldSpec`; `FGV2ScreenFieldAdapterRegistry` (`Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp`) полностью переписан на этой основе — старый эвристический `ExtractGenericBindings`/`IsFlatBindableItemsSchema`/hardcoded `IsKnownSchema` список удалены; `PrepareBindingDefinitions`/`BuildFields` теперь один и тот же генерический schema-driven обход (`WalkFieldValue`), и заодно закрывают UPP-26's известный gap для C++-стороны рантайма (реальная schema-driven валидация вместо ручного списка ключей — Lua-сторона по-прежнему не имеет доступа к схеме, это остаётся отдельным, ещё не сделанным пунктом). Verification: 92/92 UE `GV2.*`, 68/68 портативный ctest, `validate_docs.py` 167/167.

- [x] **UPP-28 — Атомарная замена экранов в слоях**
  - Зависимости: UPP-27.
  - `FGV2LayeredUiReconciler` заменяет содержимое слоя, отсоединяя предыдущий экран до того, как новый гарантированно применим.
  - Done: документ готовится целиком до любой мутации дерева; старый экран не отсоединяется, пока замена не подготовлена полностью; отказ подготовки **любого** экрана документа оставляет активный набор экранов и биндингов нетронутым — проверено инъекцией отказа в один экран из нескольких слоёв, с проверкой состояния остальных слоёв, а не только их количества; блокировка нижних слоёв при модальном окне и её восстановление сохраняются; UI-local состояние переживает ревизию при переиспользовании экземпляра.
  - Evidence: `Source/GV2/Public/UI/GV2LayeredUiReconciler.h`, `Source/GV2/Public/UI/GV2GameShellWidgetBase.h`.
  - **Реализация (2026-08-25):** `FGV2LayeredUiReconciler` переведён на строгую двухфазную архитектуру (`PrepareReconcile` / `CommitReconcile`). На первой фазе (`PrepareReconcile`) валидируются слои и уникальность ключей экземпляров, создаются/находятся целевые виджеты и для КАЖДОГО экрана в документе готовится полный план мутаций (`UGV2ScreenWidgetBase::PrepareScreenFields`) off-tree без отсоединения/присоединения виджетов и без мутации живых деревьев UMG. Если хотя бы один экран не смог инстанциироваться или его поля не подготовились, `PrepareReconcile` возвращает `false` — ни один старый экран не отсоединяется от `UGV2GameShellWidgetBase`, ни один новый не присоединяется, а `ActiveScreens` остаётся неизменным. Фаза `CommitReconcile` выполняется только после полной успешной подготовки всех экранов документа: отсоединяются заменённые и удалённые экраны, присоединяются новые, коммитятся подготовленные мутации полей, обновляется `ActiveScreens` и накладывается маскирование интерактивности модальных слоёв. В `GV2.UI.LayeredReconciliationContract` добавлены Step G (инъекция невалидного поля в один экран при многослойном документе `location_content` + `overlay_stack` + `modal_stack` с проверкой сохранения всех 3 слоёв в Reconciler и Shell) и Step H (восстановление интерактивности и переиспользование экземпляра без мутации UI-local состояния).
  - **Независимый аудит (2026-08-25):** найден и исправлен один реальный регресс. `UGV2ScreenWidgetBase::CollectScreenFieldHosts` (`GV2ScreenWidgetBase.cpp`) был молча ослаблен: `WidgetTree == nullptr` стало возвращать `true` (0 полей, а не ошибку) вместо явного отказа, установленного в UPP-27. Красный тест подтвердил, что это изменение ничем не мотивировано и ни от чего не зависит: откат к строгой проверке (`OutError = "screen has no WidgetTree"; return false;`) не уронил ни один из 93 тестов `GV2.*` — ослабление не требовалось ни production-путём, ни каким-либо тестом этого прохода (тестовый `MockFactory` в `LayeredReconciliationContract` создаёт голый `UGV2ScreenWidgetBase::StaticClass()`, но его `WidgetTree` оказывается не `nullptr`). Строгая проверка восстановлена. Verification после отката: 93/93 UE `GV2.*`, 68/68 портативный ctest, `validate_docs.py` 167/167, легаси-гейт зелёный.

- [x] **UPP-29 — Координатор сессии и реестр биндингов на подготовленном commit**
  - Зависимости: UPP-28.
  - Done: `FGV2SessionCoordinator` готовит презентацию и публикует её одним commit-ом; реестр биндингов коммитит подготовленную ревизию только после успешной подготовки всего документа; handle предыдущей ревизии становится invalid, handle предыдущего поколения сессии — stale, и оба состояния различимы; отказ на любом шаге не оставляет частично опубликованной ревизии — проверено инъекцией отказа между подготовкой и публикацией.
  - Evidence: `Source/GV2/Private/Application/GV2SessionCoordinator.cpp`, `Source/GV2/Private/Application/GV2SessionCoordinator.h`, `Source/GV2/Private/Bridge/GV2UiBindingRegistry.cpp`.
  - **Реализация (2026-08-25):** `FGV2SessionCoordinator` и `FGV2UiBindingRegistry` полностью согласованы по двухфазному жизненному циклу публикации документа и биндингов. В `FGV2SessionCoordinator::PrepareDocumentRequest` candidate document model и candidate binding set готовятся off-tree (извлечение определений, проверка уникальности путей, генерация handle-кандидатов без коммита в активный реестр). Метод `PublishUiBindings` синхронизирует `UiRevision` координатора и гарантирует строгую монотонность candidate-ревизий. `CommitPreparedBindings` вызывается строго после успешного `DocumentSink` (применения документа); при отказе или инъекции ошибки на шаге подготовки/применения презентации подготовленный набор отбрасывается, активная ревизия и активные handles остаются полностью неизменными и доступными для взаимодействия, а нескоммиченные handles кандидатов разрешаются в `EGV2BindingResolveResult::Invalid`. После успешного commit хэндлы предыдущей ревизии текущей сессии становятся `Invalid`, а при смене поколения сессии (`BeginSession`/`EndSession`) хэндлы предыдущих сессий разрешаются в `EGV2BindingResolveResult::Stale`. Добавлен automation-тест `GV2.Runtime.Session.PreparedCommitAndFailureInjection`.

## Проверка milestone

- [x] Публичный preflight предсказывает отказ глубокого ребёнка; `STATUS-004` удалён.
- [x] Отказ подготовки одного экрана не меняет состояния остальных слоёв.
- [x] Частично опубликованная ревизия биндингов невозможна.
- [x] Компенсирующий откат удалён, а не оставлен параллельно новому механизму (на уровне одного экрана — UPP-27; атомарность между экранами/слоями — UPP-28).
