---
title: Teardown and Closure Tasks
status: active
version: 1.1
updated: 2026-08-25
depends_on:
  - README.md
  - ScreenAndDocument.md
  - ../../Status/ImplementationStatus.md
---

# M8 — Teardown and Closure

> **Материализует:** фазы 7 и 8 proposal, разделы 23, 24, 25, 31.14, 36.
> **Задачи:** UPP-30…32.
> **Результат:** старая модель отсутствует физически, а не помечена устаревшей, и каждое заявленное закрытие проверено.

## Результат этапа

К этому моменту schema-specific ветки удалены поэлементно на своих этапах, поэтому здесь снимается только общий каркас. Это не cutover.

**UPP-32 — не формальность.** Три предыдущих раунда проверки показали, что ложное утверждение попадает не в код, а в сводку выполненного плана, и переживает закрытие. Этап заканчивается независимой сверкой каждого заявленного закрытия по правилу красного теста.

## Задачи

- [x] **UPP-30 — Снятие каркаса и постоянный запрет**
  - Зависимости: UPP-29.
  - Done: `FGV2ScreenFieldAdapterRegistry`, union payload `FGV2ScreenFieldValue` и остаточные schema-specific DTO удалены; публичная площадь `GV2BridgeTypes.h` сокращена до типов, которые действительно пересекают границу; удалены legacy fallible apply-пути и публичные `ApplyXxxModel`, существовавшие только для старой модели; гейт монотонного убывания доведён до нуля по всем трём счётчикам и превращён в постоянный запрет — попытка ввести новый schema-specific адаптер краснит сборку; [ADR-0038](../../ADR/0038-screen-field-value-flat-struct.md) отмечен как заменённый решением UPP-01: его предмет удалён, а не пересмотрен по наступлению условия; сборка UE и портативная зелёные, automation зелёная.
  - Evidence: `Source/GV2/Public/Bridge/GV2BridgeTypes.h`, `Source/GV2/Private/Application/`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-08-25):** `FGV2ScreenFieldAdapterRegistry` (класс/singleton `Get()`) удалён целиком, а не опустошён — `GV2ScreenFieldAdapterRegistry.h/.cpp` заменены на `Source/GV2/Private/Application/GV2ScreenFieldMaterializer.h/.cpp`: те же `PrepareBindingDefinitions`/`BuildFields`/`IsKnownSchema`, но свободные функции в `namespace GV2ScreenFieldMaterializer`, а не методы класса — кэш схем теперь function-локальный static, не член класса. Мёртвый `FGV2SessionCoordinator::PrepareScreenRequest` (0 вызывающих в production, только сохранившийся от более раннего варианта архитектуры) удалён вместе с ним. `ApplyButtonModels`/`CanApplyButtonModels`/`ResetButtonModels`/`GetAppliedButtonModels` (`UGV2ButtonListWidgetBase`) и `ApplyProgressBarModel` (`UGV2ProgressBarWidgetBase`) удалены — оба обходили `PrepareUiHostProperties`/`CommitUiHostProperties` напрямую через `FGV2KeyedCollection::Reconcile`/ручной вызов text pipeline. `FGV2ButtonViewModel` и `FGV2ProgressBarViewModel` удалены из `GV2BridgeTypes.h`; полностью мёртвый `UGV2DebugStartScreenWidget` (0 ссылок где-либо в кодовой базе, единственный потребитель `FGV2ButtonViewModel` вне тестов) удалён целиком. Тесты REV3-01/02/05 (`GV2.Runtime.UI.FailurePropagationAndTextPipelineRouting`) переписаны на прямой вызов `PrepareUiHostProperties`/`CommitUiHostProperties` с вручную собранной `FCompiledUiFieldSpec` — тот же паттерн, что уже использовался для REV3-09/10 в этом же тесте, и для UPP-20 в `GV2PropertyConsumersTests.cpp`.
    `FGV2RichTextHoverViewModel`/`FGV2RichTextSpanViewModel` **не удалены** — они остаются активным внутренним представлением `FGV2RichTextSpansPropertyConsumer::PreparedSpans`, то есть частью нового, а не старого пути; занесены в `INFRA_STRUCT_ALLOWLIST` гейта с обоснованием, а не посчитаны как legacy DTO.
    Гейт `Tools/Content/validate_ui_pipeline_legacy_gate.py`: `schema_specific_dtos` 4→0 (постоянный запрет, самотест подтверждает обнаружение новой DTO); `prepare_build_functions` остался 0 — потребовалась правка самого regex (`PrepareBindingDefinitions`/`BuildFields` теперь свободные функции без `Class::`-квалификатора перед именем и совпадали бы с шаблоном без явного allowlist двух генерических точек входа, что было бы ложным срабатыванием, а не найденной legacy-функцией). `screen_field_value_payload_members` **честно остался на 2**, не 0: `FGV2ScreenFieldValue.PreparedValue`/`CompiledSchema` — единственный носитель материализованного значения между материализатором и `UGV2ScreenWidgetBase::PrepareScreenFields`/`CommitScreenFields`, оба поля одного и того же генерического типа для любой схемы; удалить их означало бы удалить сам механизм Prepare/Commit, построенный в UPP-27..29, а не устаревший транспорт — решение зафиксировано с обоснованием в самом гейте (UPP-27, подтверждено здесь).
    Verification: 93/93 UE `GV2.*`, 68/68 портативный ctest, `validate_docs.py`, легаси-гейт (позитивный и все 4 негативных самотеста) зелёные.

- [ ] **UPP-31 — Контракты и статус приведены к реализации**
  - Зависимости: UPP-30.
  - Done: [Screen Templates](../../UI/ScreenTemplates.md), [UI Document](../../UI/UIDocumentAndReconciliation.md), [Widget Registry](../../UI/WidgetRegistry.md), [Image Resources](../../UI/ImageResources.md) и [Invariants](../../Architecture/Invariants.md) описывают новую модель как единственную — упоминаний schema-specific адаптера как действующего механизма не осталось; [Implementation Status](../../Status/ImplementationStatus.md) содержит только реально открытые расхождения; ни один контракт не утверждает свойства сильнее, чем подтверждает тест — проверено выборочно по каждому нормативному утверждению об атомарности и предиктивности; архивные планы **не** переписываются.
  - Evidence: `Docs/UI/`, `Docs/Architecture/Invariants.md`, `Docs/Status/ImplementationStatus.md`.

- [ ] **UPP-32 — Сверка закрытий**
  - Зависимости: UPP-31.
  - Каждое утверждение о закрытии проверяется независимо от задачи, которая его заявила.
  - Done: для каждого пункта итогового Definition of Done плана назван конкретный тест и **продемонстрировано**, что он краснеет при откате соответствующего изменения; для `REV3-01`…`REV3-10` подтверждено, что закрыт класс, а не экземпляр: попытка воспроизвести дефект того же вида на любом мигрированном виджете невозможна структурно либо отклоняется проверкой; отдельно проверено, что harness наблюдаемости покрывает **все** объявленные capability всех мигрированных виджетов, а не подмножество; расхождения, обнаруженные сверкой, либо устраняются в этом же change set, либо записываются строкой `STATUS-NNN` — «закрыто, но не проверено» исходом не является.
  - Evidence: отчёт change set, `Source/GV2/Private/Tests/`, `Docs/Status/ImplementationStatus.md`.

## Проверка milestone

- [x] Старая модель отсутствует в коде, а не помечена устаревшей.
- [x] Гейт убывания равен нулю и стал постоянным запретом (кроме `screen_field_value_payload_members`, честно оставленного на 2 — см. обоснование в UPP-30 выше).
- [ ] Каждый пункт итогового DoD подтверждён красным тестом.
- [ ] Harness наблюдаемости покрывает все capability всех мигрированных виджетов.
