---
title: Decisions and Schema Infrastructure Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
  - ../../UI/ScreenTemplates.md
---

# M1 — Decisions and Schema Infrastructure

> **Материализует:** фазы 0 и 1 proposal.
> **Задачи:** UPP-01…06.
> **Результат:** UI-схема существует как данные, компилируется и валидируется без UE.

## Результат этапа

Сегодня «схема UI-поля» — это неявная договорённость между списком `ConsumedKeys` в адаптере и телом `BuildXxx`. Сверить её не с чем, поэтому расхождение между объявленным и применяемым не наблюдаемо.

Этап делает схему объектом: она объявляется данными, компилируется в неизменяемое представление, валидируется переносимым кодом и публикуется в репозитории. Ни один виджет на этом этапе не мигрирует — сначала появляется то, с чем можно сверять.

## Задачи

- [x] **UPP-01 — ADR: универсальный UI property pipeline**
  - Решения proposal не зафиксированы нормативно, поэтому реализация может разойтись с ними, не нарушив ни одного контракта.
  - Done: ADR фиксирует и обосновывает — подготовленное дерево значений вместо schema-specific DTO; разделение Prepare/Commit и запрет fallible-работы в Commit; наблюдаемое поведение отказа Commit (экран не публикуется, предыдущая ревизия цела, диагностика с `property_path`), не зависящее от `STATUS-002`; инвариант `SchemaContract ⊆ WidgetCapabilities` **вместе** с требованием наблюдаемости capability; запрет alias между именем свойства схемы и именем capability; владение schema ID по namespace; политику отказа для схем мода (мод отклоняется, сессия продолжается) в отличие от Core/TextSystem (сессия не становится `Ready`); отсутствие обратной совместимости как принятое условие миграции. Номер ADR резервируется созданием файла, а не заранее.
  - Evidence: [ADR-0040](../../ADR/0040-universal-ui-property-pipeline.md), [ADR index](../../ADR/README.md). Каждый пункт Done — отдельный `Decision`-параграф ADR-0040 (1–8); замена ADR-0038 зафиксирована в `Context` ADR-0040 и остаётся формально `rejected` до фактического удаления union задачей UPP-30, как и предписывает README плана.

- [x] **UPP-02 — `schema_domain` и стандартные UI-kinds в компиляторе схем**
  - Зависимости: UPP-01.
  - Существующий compiler (`Source/GV2ContentCore/`) знает `definition_type` и extension-схемы, но не знает домена UI и его kinds.
  - Done: введён `schema_domain` со значениями `ui_field`/`ui_value`; реализованы стандартные kinds — скалярные (`bool`, `integer`, `number`, `string`), семантические (`key`, `text`, `ref`, `binding`) и структурные (`object`, `array`, `screen_fields`); `key` **не приводится** из `string` — объявление `key: {kind: "string"}` является ошибкой сборки схемы; неизвестный kind, отсутствие обязательного поля kind и нарушение ограничений дают типизированную диагностику; каждый kind покрыт положительным и отрицательным случаем.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/UiSchema.h`, `Source/GV2ContentCore/Private/UiSchema.cpp` — новый `CompileUiFieldSpec`, не расширение существующего `EFieldKind`. Портативный тест `RunUiSchemaConformance` (`Source/GV2ContentCore/Private/UiSchemaConformance.cpp`) — 24 случая (по два на kind + `schema_domain` round-trip + unknown/missing kind + `default` только на scalar), выполняется из `Headless/Source/main.cpp` (`pcc_ui_schema_self_test_failed`) и из UE-теста `GV2.Runtime.ContentCore.UiSchema` (`Source/GV2/Private/Tests/GV2ContentCoreUiSchemaTests.cpp`) — идентичный код на обоих хостах. Красный тест на откате подтверждён вручную: ослабление проверки `keyed_by` (снятие `It->Spec->Kind != EUiFieldKind::Key`) даёт `pcc_ui_schema_self_test_failed: ui_schema.array.negative` на `gv2-headless --self-test`; проверка восстановлена, тест снова зелёный.

- [ ] **UPP-03 — `schema_ref` и обнаружение циклов**
  - Зависимости: UPP-02.
  - Композиция схем без раскрытия ссылок вынудит дублировать описания элементов коллекций, а без обнаружения циклов взаимная ссылка повесит сборку.
  - Done: `schema_ref` раскрывается компилятором в скомпилированный узел и не существует как runtime-kind; прямой и косвенный цикл обнаруживаются и дают диагностику с цепочкой ссылок; ссылка на несуществующую схему и ссылка через границу владения namespace отклоняются; тест на цикл длиной больше двух.
  - Evidence: `Source/GV2ContentCore/`, тесты компилятора.

- [ ] **UPP-04 — Переносимый валидатор значения**
  - Зависимости: UPP-02.
  - Валидация значения сегодня живёт в UE-модуле, поэтому headless и инструменты контента её не применяют и расходятся с игрой.
  - Done: валидатор живёт вне UE UI-модуля и не создаёт `FText`, brush, `UWidget` и binding handle; проверяет неизвестные и отсутствующие ключи, тип скаляра, `min`/`max`, грамматику ключа, дубликаты ключей коллекции, kind Stable ID, структуру `TextSpec` и `BindingSpec`, вложенные схемы; один и тот же набор случаев исполняется UE-тестом и headless-прогоном и даёт **совпадающий** результат; неизвестный ключ на любом уровне вложенности отклоняется.
  - Evidence: `Source/GV2ContentCore/`, `Tests/Lua/`, `gv2-headless --check-scripts`.

- [ ] **UPP-05 — Публикация UI-схем в репозитории и владение namespace**
  - Зависимости: UPP-03, UPP-04.
  - Done: UI-схемы загружаются существующим `GameData/<package>/schemas` pipeline и индексируются по ID; `core:`/`textsystem:`/`rh:` объявляются только своими пакетами, `<mod>:` — модом; попытка объявить чужой namespace отклоняется; мод может собрать схему **исключительно** из стандартных kinds — попытка ввести новый primitive kind отклоняется; тест на схему мода из чистых данных, проходящую компиляцию без единой строки C++.
  - Evidence: `Source/GV2ContentCore/`, `GameData/`, тесты репозитория.

- [ ] **UPP-06 — Контракты объявляют схемы UI данными**
  - Зависимости: UPP-05.
  - Done: [Screen Templates](../../UI/ScreenTemplates.md) и [UI Document](../../UI/UIDocumentAndReconciliation.md) описывают UI-схему как данные: домен, стандартные kinds, замкнутость на всех уровнях, правило владения namespace, политику отказа для мода; [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) описывает `schema_domain` и `schema_ref`; [Add Screen Field](../../Guides/AddScreenField.md) переписан под добавление схемы данными, а не C++-адаптером; ни один контракт не описывает одновременно старую и новую модель как действующие.
  - Evidence: `Docs/UI/`, `Docs/Architecture/`, `Docs/Guides/AddScreenField.md`.

## Проверка milestone

- [ ] Схема мода компилируется из данных без C++.
- [ ] Неизвестный kind, цикл `schema_ref` и чужой namespace отклоняются типизированно.
- [ ] Один набор случаев валидации даёт одинаковый результат в UE и headless.
- [ ] Ни один виджет ещё не мигрирован — этап не меняет поведение экрана.
