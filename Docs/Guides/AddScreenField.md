---
title: Add Screen Field
status: informative
version: 3.0
updated: 2026-09-04
depends_on:
  - README.md
  - ../UI/ScreenTemplates.md
  - ../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# Добавить Screen Field

> **Задача:** собрать новый composite-блок из Designer и добавить декларативную схему поля экрана (`ui_field`) или переиспользуемой структуры (`ui_value`) в репозиторий контента — оба шага данными, без написания C++.
> **Предмет:** Widget Blueprint на `UGV2DeclaredCompositeWidgetBase`, `DeclaredCapabilities`, `HostIdentity`; `schema_id`, файл схемы JSON5 в `GameData/<package>/schemas/`, стандартные UI kinds, связывание с UMG виджетом.
> **Нормативно:** [Screen Templates](../UI/ScreenTemplates.md), [Definition Envelope and Schema Rules](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [UI Document](../UI/UIDocumentAndReconciliation.md).

Схемы полей интерфейса являются чистыми данными в репозитории контента (`GV2ContentCore`) и компилируются универсальным компилятором схем. Composite, который эти данные принимает, — тоже данные: Widget Blueprint на `UGV2DeclaredCompositeWidgetBase` с объявлением `DeclaredCapabilities`. Создание специализированного C++-класса или C++-адаптера схемы для добавления нового поля **не требуется** ни на одной стороне — это доказано трижды на возрастающей глубине задачами `DCA-09…11` плана [DeclaredCompositeAdoption](../Plans/Archive/DeclaredCompositeAdoption.md): двухлистовой композит (`npc_portrait`), композит с необязательным полем (`location_description`), и четырёхуровневая цепочка экран → вкладки → вложенный экран → коллекция (`inventory_tabs`) — ни один не потребовал строки production-кода.

## Часть A — собрать composite из Designer

Применимо и к новому top-level Screen Field host, и к вложенному composite (блок внутри вкладки, внутри другого composite и т.д. — глубина не ограничена).

1. **Создать Widget Blueprint**, родитель — `UGV2DeclaredCompositeWidgetBase`. Если нужное поведение уже даёт существующий generic host (например, вкладки — `UGV2TabContainerWidgetBase`), можно либо использовать его напрямую, либо обернуть его composite'ом, который делегирует его capability под своим именем (см. `WBP_InventoryTabs`, `DCA-11`).
2. **Собрать WidgetTree** только из уже существующих виджетов проекта (`WBP_Text`, `WBP_Image`, `WBP_Portrait`, `WBP_ListView_Wrap`, `WBP_Icon`, и т.д.). Написание нового C++-класса ради самой композиции — сигнал, что модели не хватает возможности, а не повод обойти её.
3. **Заполнить `DeclaredCapabilities`** — список независимых троек `PropertyName`/`ChildWidgetName`/`Kind`, редактируемый в Details panel (или через `unreal-mcp` `ObjectTools.set_properties` на CDO):
   - `Kind` — один из `EGV2DeclaredUiCapabilityKind`, доказанных end-to-end: `Boolean`, `Integer`, `Number`, `String`, `Key`, `Text`, `ResourceRef`, `Binding`, `CollectionHost`, `NestedScreen`. (`RichTextSpans` помечен `Hidden` — см. `FGV2DesignerCapabilityKindGate` — до отдельной задачи, которая его проверит.)
   - `Number`/`Integer` дополнительно несут `NumberMin`/`NumberMax`/`IntMin`/`IntMax`; `ResourceRef` — `TargetKind` (обычно `"resource"`); `CollectionHost` — `EntryWidgetClass` (реальный виджет-элемент; его **собственная** `DescribeUiCapabilities` определяет форму элемента, а не ручное объявление на composite) и `KeyPropertyName`.
   - `bOptional=true` делает запись отсутствующей в дереве capability всякий раз, когда `ChildWidgetName` не резолвится в `WidgetTree` **этого конкретного ассета** — не Prepare-time отказ, а отсутствие. Используйте для вариантов composite, где часть детей сознательно опущена: два ассета с одинаковым объявлением, один без опционального ребёнка (см. `DCA-10`'s `WBP_LocationDescription`/`WBP_LocationDescriptionNoIllustration`).
   - `ChildCapabilityName` нужен только если целевой ребёнок сам объявляет **несколько** capability одного `Kind` (например, `UGV2TabContainerWidgetBase` объявляет и `default_tab_key`, и `key`, обе `Key`). Резолвер сначала пробует однозначный `Kind`, затем совпадение имени top-level свойства с именем capability ребёнка (`FallbackNameHint` — так уже резолвится `default_tab_key`/`key` у `WBP_InventoryTabs` и `WBP_Duc10TabsHost` безо всякого явного селектора), и только потом отклоняет как неоднозначное.
4. **Задать `HostIdentity`** (`PropertyHostState`, `ShowOnlyInnerProperties` в Details) — непустое значение делает ассет top-level Screen Field host (его `field_id`); `NAME_None` оставляет его вложенным composite, адресуемым только по имени в дереве родителя.
5. **Скомпилировать** Blueprint. Правки `DeclaredCapabilities` через `set_properties` не видны автоматическим тестам без явной рекомпиляции CDO — компилируйте после каждого изменения объявления, не только в конце.
6. **Проверить попадание в sweep**: `GV2.UI.CapabilityObservabilityCompositeSweep` рефлективно находит любой WBP под сконфигурированными UI-путями проекта, чей generated class реализует `IGV2UiPropertyHost`, и прогоняет реальный Prepare/Commit round-trip по каждой объявленной capability — для нового ассета это не требует ни строчки C++, только присутствия в дереве.

## Часть B — добавить схему поля

1. **Выбор идентичности схемы**:
   - Для поля экрана выберите Stable ID вида `<namespace>:schema.ui_field.<name>.vN` (например, `core:schema.ui_field.progress_bar.v2`).
   - Для переиспользуемой вложенной структуры выберите `<namespace>:schema.ui_value.<name>.vN` (например, `core:schema.ui_value.button_item.v1`).
   - Пакет обязан владеть своим namespace (`core:`, `textsystem:`, `rh:`, `<mod>:`).

2. **Создание файла схемы в `GameData/<package>/schemas/`**:
   - Создайте файл `<name>_vN.schema.json5` с замкнутым конвертом:
     ```json5
     {
       id: "core:schema.ui_field.progress_bar.v2",
       schema_domain: "ui_field",
       schema_version: 2,
       root: {
         kind: "object",
         fields: {
           percent: {
             kind: "number",
             required: true,
             min: 0.0,
             max: 1.0,
           },
           label: {
             kind: "text",
             required: false,
           },
         },
       },
     }
     ```
   - Используйте исключительно стандартные kinds:
     - **Скалярные**: `bool`, `integer`, `number`, `string`;
     - **Семантические**: `key` (валидируемый ключ коллекции, не приводится из `string`), `text` (`TextSpec`), `ref` (ссылка Stable ID), `binding` (привязка команды);
     - **Структурные**: `object` (всегда замкнут), `array` (с `keyed_by`, `min_items`, `max_items`), `screen_fields`, `schema_ref` (inline-включение именованной схемы `ui_value`).
   - Введение нестандартных примитивных видов запрещено.

3. **Регистрация схемы в пакете**:
   - Добавьте schema binding в манифест пакета (`manifest.json5` / дескриптор пакета) для связывания пути файла схемы с `schema_id`.

4. **Связывание с презентацией**:
   - `field_id` — это `HostIdentity`, уже заданный в Части A, шаг 4, на самом composite; отдельного поля для него в Dynamic Screen Element нет.
   - Виджет связывает валидированные свойства данных со своими элементами отображения через `DeclaredCapabilities` (Часть A, шаг 3), а не через C++.

5. **Генерация данных в Lua**:
   - Презентер формирует значение поля в UI-документе в соответствии со схемой. Все ключи обязаны соответствовать объявленным в `root.fields`. Лишние ключи на любом уровне вложенности будут отклонены валидатором.

6. **Верификация**:
   - Запустите автоматические тесты репозитория (`ctest`, `gv2-headless --self-test`).
   - Проверьте положительный сценарий (валидный документ применяется) и отрицательные сценарии:
     - Отклонение неизвестного ключа (`core:diagnostic.ui_schema.value.unknown_field`);
     - Отклонение невалидного диапазона или типа скаляра;
     - Отклонение невалидного или дублированного ключа повторяемого элемента.

## Правило замкнутости схемы

Все объекты UI-схем являются строго замкнутыми на всех уровнях вложенности. Посторонний ключ в значении поля или внутри элемента коллекции не игнорируется, а приводит к типизированному отказу валидации. Это исключает silent data loss и рассинхронизацию между Lua-презентером и виджетом.

