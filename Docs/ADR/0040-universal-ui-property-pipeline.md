---
title: "ADR-0040: Universal UI Property Pipeline"
status: accepted
date: 2026-08-23
---

# ADR-0040: Universal UI Property Pipeline

> **Решение:** schema-specific цепочка `PrepareXxx → BuildXxx → FGV2XxxViewModel → ApplyXxx` заменяется одним универсальным pipeline — data-driven UI-схемы, generic подготовленное дерево значений (`FGV2PreparedUiValue`), единый `IGV2UiPropertyHost` со standard property consumers и разделение lifecycle на **Prepare** (fallible, не мутирует live UI) и **Commit** (infallible-style, применяет уже подготовленное). Инвариант `SchemaContract ⊆ WidgetCapabilities` действует вместе с требованием наблюдаемости capability; alias между именем свойства схемы и именем capability запрещён; schema ID принадлежит namespace пакета; несовместимая схема мода отклоняет мод, а не сессию; обратная совместимость не поддерживается.

## Context

Текущая граница Lua → C++ преобразует небольшой набор фундаментальных portable-значений (`null`/`bool`/`integer`/`number`/`string`/`TextSpec`/Stable ID/`BindingSpec`/`array`/`object`) в 13 schema-specific DTO (`FGV2ButtonViewModel`, `FGV2ProgressBarViewModel`, `FGV2LocationPlayerStatusViewModel`, …), хранящихся одновременно в union `FGV2ScreenFieldValue`, и в ручную пару `PrepareXxx`/`BuildXxx`/`ApplyXxx` на каждую схему.

Это породило повторяющийся класс дефектов «значение принято границей и молча не применено» — `REV3-01`…`REV3-10`, зафиксированные [Universal Data-Driven UI Property Pipeline](../Proposals/UniversalDataDrivenUIPropertyPipelineProposal.md) (`§2`, `§28`). Одно семейство («значение пересекло границу и молча исчезло») дало четыре независимых экземпляра за четыре раунда проверки: персонажи сцены, иконки предметов, метки метров, свойства `is_read_only`/`max_length`. Каждый закрывался точечно, частота не падала. Точечные патчи устраняют экземпляр и ничего не говорят о рецидиве.

Отдельно от этого семейства открыты два архитектурных пробела: `STATUS-003` («политика масштабирования не объявлена» невыразима — поле имеет default `PreserveAspect`, забытое объявление от него неотличимо) и `STATUS-004` (`CanApplyScreenFields` не опрашивает глубоких детей composite, поэтому не является реальным predictive preflight).

[ADR-0038](0038-screen-field-value-flat-struct.md) ранее мотивированно отклонил переход `FGV2ScreenFieldValue` на размеченное объединение с наблюдаемым условием пересмотра «добавление 14-го payload». Это решение фиксирует замену: план [UniversalUiPropertyPipeline](../Plans/UniversalUiPropertyPipeline/README.md) удаляет union целиком, а не расширяет его до 14-го payload, поэтому условие ADR-0038 не наступает, а снимается вместе с самой структурой. Формальный статус ADR-0038 переводится в `superseded` тем change set, который физически удаляет union (`UPP-30`), а не этим ADR — до тех пор union продолжает существовать и решение ADR-0038 остаётся описанием текущего кода.

## Decision

1. **Подготовленное дерево значений вместо schema-specific DTO.**
   После Lua-границы UE получает не `FGV2XxxViewModel`, а generic recursive `FGV2PreparedUiValue` (`Null`/`Boolean`/`Integer`/`Number`/`String`/`Key`/`Text`/`StableId`/`Binding`/`Object`/`Array`) внутри `FGV2PreparedScreenField { FieldId, SchemaId, CompiledSchema, Properties }`. `Key` — самостоятельный kind, не приводимый из `String`: идентичность, выведенная из отображаемого значения или позиции в массиве, дважды воскрешала одну и ту же проблему в предыдущих раундах.

2. **Разделение Prepare/Commit и запрет fallible-работы в Commit.**
   **Prepare** может отклонить кандидата по любой причине (schema, renderer target, ScalePolicy, binding contract, дочерний preflight) и обязан **не менять live presentation state** ни при валидном, ни при невалидном входе. **Commit** выполняется только после успешного Prepare всего дерева, не выполняет validation/resolution/loading/lookup и проектируется как infallible-style operation.

3. **Наблюдаемое поведение отказа Commit.**
   Неожиданный отказ Commit — нарушение инварианта, а не обычный `false`. Наблюдаемое поведение: экран, чей Commit отказал, **не публикуется** как interactive; предыдущая ревизия остаётся активной; диагностика структурирована и содержит полный `property_path` (раздел 32 proposal). Это поведение не зависит от `STATUS-002` (Presentation Effects) — аварийный путь Commit не использует recovery-механизм, которого ещё нет.

4. **`SchemaContract ⊆ WidgetCapabilities` вместе с требованием наблюдаемости capability.**
   Widget публикует capability tree; допустима schema — подмножество capability, запрещена schema, требующая свойства шире capability. Объявление capability само по себе недостаточно: capability обязана быть **наблюдаемой** — для каждой объявленной capability `C` должны существовать значения `A ≠ B`, для которых `Capture(Commit(A)) ≠ Capture(Commit(B))`. Без этого правила ложь `whitelist != consumption` переезжает из `ConsumedKeys` в таблицу capability и ничего не решает.

5. **Запрет alias между именем свойства схемы и именем capability.**
   `schema property "text"` обязано совпадать с `widget capability "text"` буквально. Слой переименования/маппинга не вводится — это то, что делает автоматическую проверку coverage возможной.

6. **Владение schema ID по namespace.**
   `core:schema...` объявляет только Core, `textsystem:schema...` — только TextSystem, `<mod>:schema...` — только мод. Мод может собрать schema исключительно из стандартных Core kinds (scalar/semantic/structural) без нового `kind`, renderer pipeline, binding semantics или raw UE-локатора.

7. **Политика отказа зависит от владельца схемы.**
   Несовместимая схема `core`/`textsystem`/`rh` — ошибка сборки проекта: сессия не становится `Ready`. Несовместимая схема мода отклоняет сам мод с типизированной диагностикой; сессия продолжается без него. Сторонние данные не должны иметь возможности сделать игру незапускаемой.

8. **Обратная совместимость не поддерживается как условие миграции.**
   У проекта нет внешних потребителей, сохранённых сессий, переживающих смену формата presentation, или published schema, которую нельзя переписать вместе с кодом. Из этого следует: виджет мигрирует вместе со своим содержимым и своим адаптером в одном change set — `PrepareXxx`/`BuildXxx`/DTO/ветка union удаляются немедленно, а не остаются в parallel stack до итогового cutover. Двойной стек допустим только **между** мигрированными и немигрированными элементами, не **внутри** одного элемента.

## Consequences

Миграция становится непрерывным удалением, а не одномоментным cutover: каждый мигрированный виджет уменьшает legacy-поверхность на свою величину, и это измеряется гейтом монотонного убывания (число schema-specific `Prepare/Build`, число payload-членов union, число schema-specific DTO — верхняя граница фиксируется тестом и не может расти после фазы 2).

`STATUS-003` закрывается введением `EGV2PrimitiveScalePolicy::Unset` как default и startup-валидацией capability. `STATUS-004` закрывается тем, что успешный Prepare всего документа структурно означает, что все дочерние renderer targets, классы, ресурсы и nested screens разрешились — становится реальным predictive preflight.

`FGV2ScreenFieldAdapterRegistry`, схема-specific DTO из `GV2BridgeTypes.h` и парные `CanApplyXxx`/`ApplyXxx`/`CaptureXxx` на каждом виджете удаляются поэлементно по ходу миграции и полностью — на фазе 7 плана. `ApplyOptionalXxx`-варианты (`ApplyOptionalImageResource`, `ApplyOptionalPortrait`) удаляются: политика подстановки заглушки становится свойством схемы, а не второй C++ перегрузкой.

Headless и content-инструменты получают тот же переносимый валидатор UI-схем, что и UE — без зависимости от UE-типов (`FText`, brush, `UWidget`, binding handle).

Требуется обязательная остановка (go/no-go) после proving slice из трёх элементов (Text/Image/Button, `UPP-15`): если harness наблюдаемости capability, распространение отказа ребёнка, чистота Prepare и инъекция отказа Commit не удаётся заставить давать описанное наблюдаемое поведение на трёх элементах — на двадцати пяти не удастся тем более, и миграция останавливается до пересмотра, а не продолжается по инерции.

## Rejected alternatives

- **Reflection `UPROPERTY` binding по имени свойства.** Отклонён: превращает переименование C++ member в protocol change, делает Blueprint internals публичным Lua-контрактом, не позволяет гарантировать прохождение Text/Image pipelines, усложняет проверку mod permissions и не даёт выразить target-specific preflight (proposal §39).
- **Raw `FValue` до самого Widget без промежуточной подготовки.** Отклонён: переносит проблему разбора object внутрь каждого Widget вместо завершения schema/preparation до Widget — то есть воспроизводит текущую архитектуру под новым именем (proposal §40).
- **Продолжать точечно чинить экземпляры `REV3-*` семейства.** Отклонён: точечная починка убирает экземпляр и не говорит о рецидиве — ровно та стратегия, которая уже дала четыре повторения одного класса дефекта за четыре раунда.
- **Постоянный dual-stack до итогового cutover.** Отклонён ввиду отсутствия обязательства обратной совместимости (Decision 8) — постоянное сосуществование старого и нового пути для одного элемента не даёт ничего, кроме площади для расхождения между ними.
