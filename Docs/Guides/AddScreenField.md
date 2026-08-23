---
title: Add Screen Field
status: informative
version: 1.2
updated: 2026-08-23
depends_on:
  - README.md
  - ../UI/ScreenTemplates.md
---

# Добавить Screen Field schema

> **Задача:** добавить новый reusable value contract между Lua Screen document и Dynamic Screen Element.
> **Предмет:** `schema_id`, portable envelope, fixed adapter, native Widget base и Editor asset.
> **Нормативно:** [Screen Templates](../UI/ScreenTemplates.md), [Widget Registry](../UI/WidgetRegistry.md), [UI Document](../UI/UIDocumentAndReconciliation.md).

Повторяемость подтверждают `image`/`progress_bar` adapters в `Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp`, native consumers `Source/GV2/Public/UI/GV2ImageWidgetBase.h` и `GV2ProgressBarWidgetBase.h`, а также negative cases в `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

## Процедура

1. Назовите минимум один concrete Screen consumer. Новый schema без consumer-а не добавляется.
2. Выберите versioned Stable ID `core:schema.ui_field.<name>.vN` и зафиксируйте portable value, required fields, limits, stable keys и failures в `ScreenTemplates.md`/owning component contract.
3. Добавьте trusted conversion в `FGV2ScreenFieldAdapterRegistry`: `PrepareBindings` только валидирует и собирает ordered binding definitions; `BuildField` потребляет prepared opaque handles и создаёт typed value. Обязательно вызовите `CheckClosedKeys` для значения поля и каждого вложенного объекта.
4. Добавьте/расширьте native `IGV2DynamicScreenElement` adapter. Он применяет только prepared value и умеет rollback/reset; gameplay authority не получает.
5. Создайте или измените `WBP_*` только через `unreal-mcp`. После изменения compile и save asset, проверьте native parent, `BindWidget`, schema descriptor и загрузку.
6. Добавьте schema в Screen Template consumer и Lua fixture полного field envelope.
7. Покройте valid apply, malformed value, unknown/mismatched schema, missing required field, binding failure, rollback и повторное применение. **Обязательно добавьте негативные тесты на лишний ключ (closed schema)** на уровне корня значения поля и каждого вложенного объекта (`meters`, `characters`, `items`, `spans`, `TextSpec`, `Binding`). Обновите test, фиксирующий полный набор registry schemas.

## Сверьте границу в обе стороны

Adapter читает те ключи, которые в нём написаны, и об остальных ничего не знает. Поэтому ключ, отправленный презентером и не прочитанный adapter-ом, не даёт ни данных, ни ошибки: значение доходит до границы и исчезает. Так дважды терялись уже вычисленные данные — персонажи сцены и иконки предметов.

Перед завершением задачи выпишите два набора и сравните их:

- ключи, которые Lua-презентер кладёт в `value` поля;
- ключи, которые читает соответствующий `Build*`/`Prepare*`.

Разница в любую сторону — дефект, а не задел на будущее. Ключ производителя без потребителя молча теряется; ключ потребителя без производителя означает мёртвую ветку, которую заполняет только тест.

Тест обязан пересекать ту же границу: значение поля строится со стороны Lua, а не собирается как view model прямо в C++. Тест, собирающий `FGV2*ViewModel` в C++, проверяет composite и не проверяет adapter — расхождение имени или формы ключа он не обнаружит по построению.

## Не расширяйте coordinator

`GV2SessionCoordinator` не знает concrete `schema_id` и field names. Добавление schema меняет один adapter/consumer path, а не вводит switch, второй binding registry или отдельную публикацию.
