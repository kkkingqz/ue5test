---
title: Prepared Values and Property Host Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - DecisionsAndSchemas.md
  - ../../UI/WidgetRegistry.md
---

# M2 — Prepared Values and Property Host

> **Материализует:** фазу 2 proposal, разделы 6, 10.2a, 11–13.
> **Задачи:** UPP-07…11.
> **Результат:** существует механизм, применяющий свойство к виджету так, что «принято и не применено» становится невозможно.

## Результат этапа

Этап строит несущую конструкцию: подготовленное дерево значений, интерфейс хоста свойств, набор стандартных consumers и раздельные фазы Prepare/Commit.

**UPP-11 обязан быть последней задачей этапа и не может быть отложен.** Harness наблюдаемости — единственная проверка, делающая объявление capability неподделываемым. Мигрировать виджеты, не имея его, значит переносить ту же ложь в новую структуру и обнаружить это в следующем раунде проверки.

## Задачи

- [x] **UPP-07 — Подготовленное дерево значений**
  - Портативный `FValue` внутри UE недостаточен: после подготовки появляются локализованный `FText`, разрешённая типографика, opaque binding handles и проверенные Stable ID.
  - Done: `FGV2PreparedUiValue`/`FGV2PreparedUiObject`/`FGV2PreparedUiArray` реализованы как приватное native-представление на `TVariant`, не `BlueprintType`; kinds — `Null`, `Boolean`, `Integer`, `Number`, `String`, `Key`, `Text`, `StableId`, `Binding`, `Object`, `Array`; `Key` и `String` — **разные** kinds, взаимное приведение отсутствует; подготовленный кандидат неизменяем после создания; порядок обхода свойств канонический и не зависит от порядка ключей парсера — тест на двух объектах с одинаковым набором ключей в разном порядке даёт одинаковый обход; есть `ToDebugString` с полным `property_path`.
  - Evidence: `Source/GV2/Public/UI/GV2PreparedUiValue.h`, `Source/GV2/Private/UI/GV2PreparedUiValue.cpp`, тест `GV2.UI.PreparedUiValue` (`Source/GV2/Private/Tests/GV2PreparedUiValueTests.cpp`). **2026-08-24, ретроактивная проверка:** до этой даты задача была помечена `[x]` без единой сборки UE — `RunUBT.sh GV2Editor` падал (циклическая полнота типа: `FGV2PreparedUiObject` хранил `FGV2PreparedUiValue` по значению до её объявления). Класс переставлен (значение объявляется первым, контейнеры — после; `AsObject()`/`AsArray()` вынесены в `.cpp`), сборка и `GV2.UI.PreparedUiValue` подтверждены зелёными через `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests GV2.UI.PreparedUiValue"`.

- [x] **UPP-08 — `IGV2UiPropertyHost` и дескриптор capability**
  - Зависимости: UPP-07.
  - Единый UCLASS-родитель невозможен без искусственного изменения иерархии CommonUI, поэтому нужен интерфейс.
  - Done: `IGV2UiPropertyHost` и разделяемое состояние `FGV2UiPropertyHostState` реализованы; виджет объявляет capability-дерево, где **каждая** capability указывает конкретный target (renderer control, collection host или вложенный экран); статическая проверка `SchemaContract ⊆ WidgetCapabilities` выполняется до `Ready` и рекурсивно — свойство, kind, `ref.target_kind`, диапазон, binding input contract, дети объекта, контракт элемента массива, политика идентичности коллекции; лишнее свойство схемы, несовпадение kind и несовпадение `target_kind` дают различимые коды диагностики; более узкая схема принимается, более широкая отклоняется; alias между именем свойства схемы и именем capability невозможен по построению; несовместимая схема мода отклоняет мод и **не** мешает сессии стать `Ready`, несовместимая схема Core сессию блокирует.
  - Evidence: `Source/GV2/Public/UI/GV2UiCapability.h`, `Source/GV2/Private/UI/GV2UiCapability.cpp`, `Source/GV2/Public/UI/GV2UiPropertyHost.h`, тест `GV2.UI.PropertyHostAndCapabilities` (`Source/GV2/Private/Tests/GV2UiPropertyHostTests.cpp`). **2026-08-24, ретроактивная проверка:** до этой даты `GV2UiCapability.cpp` был написан под вымышленную форму `GV2ContentCore::FCompiledUiFieldSpec` (`EUiFieldKind::Bool/Integer/Number/String`, плоские `TargetKind`/`IntMin`/`NumberMin`/`ObjectFields`), не совпадающую с реальным API после UPP-02..04 (`Scalar`-обёртка, `RefTargetKind`, `Fields` из `FCompiledUiObjectField`) — ни разу не собиралось в UE. Маппинг и проверка совместимости переписаны под реальный API; сборка и `GV2.UI.PropertyHostAndCapabilities` (все 8 сценариев: subset, unknown property, kind mismatch, target_kind mismatch, диапазоны, mod-политика, keyed collection) подтверждены зелёными.

- [x] **UPP-09 — Стандартные consumers свойств**
  - Зависимости: UPP-08.
  - Done: реализованы consumers для `text` (только через `UGV2TextPipeline`), `ref` с `target_kind = resource` (только через `FGV2ImagePresentation`), скаляров, `binding` (виджет получает только `FGV2UiBindingHandle`), объекта, keyed-коллекции и вложенного экрана; альтернативный путь применения текста, изображения или binding отсутствует — тест сканирует production-код и краснеет на прямом вызове `SetText`, `SetBrush` или эквивалента вне централизованных конвейеров; consumer, объявленный для отсутствующего target, отклоняется на проверке экземпляра, а не молча пропускается.
  - Evidence: `Source/GV2/Public/UI/GV2PropertyConsumers.h`, `Source/GV2/Private/UI/GV2PropertyConsumers.cpp`, тест `GV2.UI.StandardPropertyConsumers` (`Source/GV2/Private/Tests/GV2PropertyConsumersTests.cpp`, включает аудит source-текста на прямые `SetText`/`SetBrush`). **2026-08-24, ретроактивная проверка:** до этой даты `FGV2BindingPropertyConsumer` кастовал к `IGV2UiInteractionEmitter`, которого не существовало (был только `FGV2UiInteractionEmitter::Submit`, не UInterface, без `SetBindingHandle`) — не собиралось. Добавлен реальный `UGV2UiBindingTarget`/`IGV2UiBindingTarget` (`Source/GV2/Public/UI/GV2UiBindingTarget.h`), consumer переключён на него; также исправлен `EGV2PrimitiveScalePolicy::Fill` (не существует) → `PreserveAspect`. Сборка и `GV2.UI.StandardPropertyConsumers` подтверждены зелёными.

- [x] **UPP-10 — Prepare/Commit и наблюдаемый отказ Commit**
  - Зависимости: UPP-09.
  - Done: fallible-работа целиком вынесена в Prepare — проверка target, разрешение стиля и нормализация markup, разрешение изображения, проверка политики масштабирования, проверка binding input contract, создание дочерних виджетов off-tree, подготовка вложенных мутаций, проверка Screen Registry; Commit выполняет только подготовленные мутации и не грузит ассеты, не ищет схемы, не валидирует, не резолвит локализацию, не создаёт классы; **чистота Prepare** проверена на валидном и невалидном входе — физическое состояние виджета до и после Prepare совпадает; поведение отказа Commit реализовано по ADR и проверено **инъекцией отказа**: экран не становится interactive, предыдущая ревизия цела, диагностика содержит полный `property_path`; семантика полного поля реализована — присутствующее свойство применяется, отсутствующее с default материализует default, отсутствующее без default сбрасывает consumer, capability вне схемы не затрагивается; переиспользуемый экземпляр, чьё поле получило схему без данного свойства, свойство **сбрасывает**.
  - Evidence: `Source/GV2/Public/UI/GV2UiMutationPlan.h`, `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, тест `GV2.UI.PrepareCommitAndFailureInjection` (`Source/GV2/Private/Tests/GV2UiPrepareCommitTests.cpp`). **2026-08-24, ретроактивная проверка:** до этой даты `PrepareUiHostProperties` читал несуществующее `Schema.ObjectFields` (реальное поле — `Schema.Fields`) — не собиралось; тест дополнительно передавал `nullptr` вместо host-виджета и ожидал успешного Prepare, что противоречит собственному Done-пункту («fallible-работа... проверка target») — все конкретные consumers отклоняют `nullptr` target. Тест переписан на реальный `UGV2PanelWidgetBase` с именованными `Label`/`Bar` в `WidgetTree`; assertion числа mutations исправлен (2, не 3 — `enabled` не имеет ни схемы, ни прежнего committed-значения на свежем инстансе, поэтому не мутируется вовсе, что и есть корректная full-field семантика). Сборка и все три сценария (purity check на valid/invalid input, инъекция отказа Commit с `property_path`, full-field reset на переиспользуемом инстансе) подтверждены зелёными.

- [ ] **UPP-11 — Harness наблюдаемости capability и гейт убывания legacy**
  - Зависимости: UPP-10.
  - Это средство измерения для всего плана. Без него `SchemaContract ⊆ WidgetCapabilities` проверяет лишь наличие объявления, и ложь переезжает из `ConsumedKeys` в таблицу capability.
  - Done: harness перебирает **все** объявленные capability класса виджета и для каждой требует пару значений `A != B`, дающую различимое захваченное состояние; отсутствие такой пары — ошибка сборки, а не пропуск теста; harness краснеет в трёх сценариях, проверенных явно: consumer заменён на no-op, renderer target отвязан в ассете, capability объявлена без реализации; отдельно реализован гейт монотонного убывания — тест считает schema-specific `Prepare`/`Build`-функции, payload-члены `FGV2ScreenFieldValue` и schema-specific DTO в `GV2BridgeTypes.h`, хранит верхние границы и краснеет при их росте; попытка добавить новый schema-specific адаптер после этой задачи ломает сборку.
  - Evidence: `Source/GV2/Private/Tests/`, `Source/GV2/Public/Bridge/GV2BridgeTypes.h`.

## Проверка milestone

- [ ] Prepare не меняет физическое состояние ни на валидном, ни на невалидном входе.
- [ ] Инъекция отказа Commit даёт описанное ADR наблюдаемое поведение.
- [ ] Harness наблюдаемости краснеет во всех трёх сценариях подделки объявления.
- [ ] Гейт убывания зафиксировал текущие числа и запрещает их рост.
- [ ] Ни один виджет ещё не мигрирован.
