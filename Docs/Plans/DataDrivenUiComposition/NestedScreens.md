---
title: Nested Screens Tasks
status: active
version: 1.1
updated: 2026-09-01
depends_on:
  - README.md
  - DeclaredComposite.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# M3 — Nested Screens

> **Материализует:** `UPP-R4` и два реальных риска неограниченной вложенности.
> **Задачи:** DUC-09…11.
> **Результат:** цепочка `экран → вкладки → блок` собирается из данных и выдерживает отказ на любом уровне.

## Результат этапа

`EUiFieldKind::ScreenFields` объявлен Core-видом и используется схемой контейнера вкладок, но материализатор проваливает его в `default` с «not supported». Вложенный экран во вкладке при этом работает по собственному протоколу, а не через обычный envelope `field_id` / `schema_id` / `value`.

Это средний уровень требуемой вложенности, поэтому без него композиция на двух уровнях не существует.

Ограничение глубины сознательно не вводится: рекурсия уже реализована и неограниченна, обнаружение циклов схем работает, а счётчика глубины нет нигде. Ограничение было бы новым правилом поверх работающего механизма. Вместо него закрываются два конкретных риска — циклы виджетов и отсутствие фикстур на глубине.

## Задачи

- [x] **DUC-09 — `screen_fields` через обычный envelope**
  - Зависимости: DUC-08, `PCC-04` плана доведения.
  - Done: `EUiFieldKind::ScreenFields` реализован в материализаторе; вложенный экран получает поля тем же envelope `field_id` / `schema_id` / `value`, что и экран верхнего уровня, — отдельного протокола для вкладок не остаётся; синтез дочерней схемы из capability дочернего экрана удалён; вкладка с вложенными полями материализуется, вкладка без них продолжает работать; отрицательный случай: неизвестное поле вложенного экрана отклоняется, а не игнорируется.
  - Evidence: `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.cpp`, `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`.
  - **Реализация (2026-09-01):**
    - `EUiFieldKind::ScreenFields` в `ProjectMaterializedValue` (`GV2ScreenFieldMaterializer.cpp`) больше не `return false`. Раскрытая форма — `Array<Object{field_id: Key, schema_id: String, value: Object}>`: для каждого envelope собственный `schema_id` резолвится через тот же `GetSchemaCache()`, что и верхнеуровневые поля, а `value` рекурсивно проходит через ту же пару `ValidateUiFieldValue` + `ProjectMaterializedValue`, что `BuildFields` использует для обычного top-level поля. Новый вид `EGV2PreparedUiValueKind` не понадобился — envelope целиком выражается существующими Key/String/Object.
    - Портативный `ValidateUiFieldValue` (`UiSchema.cpp`, `GV2ContentCore`) сужен с «array or object» до «array only» для `screen_fields` — сама схема остаётся закрытым leaf-маркером (глубокая структура резолвится позже, UE-side, потому что портативный компилятор не знает про Screen Registry), но форма верхнего уровня теперь однозначна.
    - `CollectBindingDefinitions` получил симметричный `EUiFieldKind::ScreenFields` case: резолвит `schema_id` каждого envelope через `Ctx.SchemaCache` и рекурсирует, в том же порядке обхода, что `ProjectMaterializedValue` — без этого курсоры binding-handle двух независимых проходов (`PrepareBindingDefinitions` и `BuildFields`) разошлись бы, как только вложенное поле само оказалось бы `Binding`-kind.
    - Новая публичная `GV2ScreenFieldMaterializer::GetCompiledSchema(SchemaId, OutError)` — та же кэш-синглтон, что резолвит верхнеуровневые схемы; используется `FGV2TabContainerTabsPropertyConsumer` для повторного (дешёвого, кэшированного) резолва compiled schema, нужного только чтобы заполнить `FGV2ScreenFieldValue::CompiledSchema`.
    - `FGV2TabContainerTabsPropertyConsumer::Prepare`/`Commit` (`GV2PropertyConsumers.cpp`) переписаны: вместо синтеза `ChildSchema` из `ChildCaps` (capability дочернего экрана) и ручного `PrepareUiHostProperties`/`CommitUiHostProperties` — `fields` разбирается как массив envelope, из каждого собирается настоящий `FGV2ScreenFieldValue`, и весь набор проходит через `ChildWidget->PrepareScreenFields(...)`/`CommitScreenFields(...)` — тот же публичный двухфазный API, которым пользуется верхнеуровневый экран. `FPreparedTabItem::ChildMutationPlan` (`TSharedPtr<FGV2UiHostMutationPlan>`) заменён на `ChildScreenPlan` (`TSharedPtr<FGV2ScreenMutationPlan>`). «Неизвестное поле вложенного экрана отклоняется» получилось бесплатно: `PrepareScreenFields` уже внутри себя использует `PrepareScreenFieldPlans`/`CollectScreenFieldHosts`, которые отклоняют payload с полем без хоста как `"payload contains unknown field"` — тот же путь, что и для экрана верхнего уровня.
    - **Подтверждено: старый путь был мёртвым кодом.** До этой задачи ветка `fields` в `FGV2TabContainerTabsPropertyConsumer::Prepare` не имела ни одного теста (ни прямого, ни через `NestedInstancesAndTabsContract`), и — что более показательно — сама структура кода (записывала non-null `fields` в компилированную схему tab_container через `ProjectMaterializedValue`'s Object-обход, чей `EUiFieldKind::ScreenFields` case всегда возвращал `false`) означала, что материализация ЛЮБОЙ вкладки с непустым `fields` через реальный `BuildFields` уже проваливалась целиком — вложенные поля не могли работать даже случайно.
    - Новые тесты (`GV2.Runtime.UI.NestedInstancesAndTabsContract`, секция «UIF-27»): (a) прямой тест `ProjectMaterializedValue`'s `ScreenFields`-ветки — резолвит реальную `textsystem:schema.ui_field.declared_composite_fixture.v1` из `GameData/`, материализует `day` через настоящий text pipeline (не просто пропускает opaque), отклоняет неизвестный `schema_id`; (b) тест consumer'а — реальный `UGV2ScreenWidgetBase` с вложенным `UGV2DeclaredCompositeWidgetBase` (форма DUC-08), seeded в `UGV2TabContainerWidgetBase` через `ApplyTabEntries`, применяет вложенные поля до настоящих `DayText`/`ValueBar` виджетов через `Prepare`+`Commit`; отдельно — лишнее поле без хоста отклоняется с `"payload contains unknown field"`.
    - Красный тест на откате подтверждён дважды по отдельности: (1) `ProjectMaterializedValue`'s `ScreenFields` case временно возвращён к `return false` — упал ровно тест (a), без каскада; (2) блок `fields` в consumer'е временно заменён на безусловный `return false` — упали ровно assertion'ы теста (b) (prepare/commit/оба виджета/сообщение negative-кейса), без каскада на остальные 100 тестов. Оба восстановления — снова 101/101.
    - Верификация: 101/101 UE Automation, 68/68 Headless ctest.

- [ ] **DUC-10 — Цепочка из трёх уровней собрана из данных и покрыта отказом**
  - Зависимости: DUC-09.
  - Done: существует фикстура `экран → вкладки → блок`, собранная **только** из ассетов, объявлений и схем, без C++-классов сверх generic-композита; значения приходят со стороны Lua и доходят до конечных виджетов; инъекция отказа подготовки **на третьем уровне** не оставляет следов на первых двух — проверяется состояние, а не количество; инъекция отказа фиксации на третьем уровне даёт поведение, описанное `ADR-0040`; sweep наблюдаемости проходит по всей цепочке.
  - Evidence: `Content/`, `GameData/`, `Source/GV2/Private/Tests/`.

- [ ] **DUC-11 — Композиционный цикл виджетов отклоняется**
  - Зависимости: DUC-10.
  - Схемные циклы обнаруживаются компилятором, но композиционный цикл виджетов — другой граф: блок, содержащий сам себя прямо или через промежуточный.
  - Done: установлено и записано, что именно гарантирует UMG для циклических ссылок между Widget Blueprint, и что остаётся непокрытым для generic-композита, разрешающего детей по имени; непокрытая часть закрывается проверкой до `Ready` с диагностикой, содержащей цепочку композиции; отрицательный тест на прямой и косвенный цикл; глубина по-прежнему не ограничивается, и это зафиксировано в контракте как решение, а не как умолчание.
  - Evidence: `Source/GV2/Private/UI/`, `Docs/UI/UIDocumentAndReconciliation.md`.

## Проверка milestone

- [x] Вкладка с вложенными полями материализуется через обычный envelope.
- [ ] Трёхуровневая цепочка собрана из данных и выдерживает отказ на третьем уровне.
- [ ] Композиционный цикл отклоняется до публикации.
- [ ] Ограничение глубины не введено, и это записано как решение.
