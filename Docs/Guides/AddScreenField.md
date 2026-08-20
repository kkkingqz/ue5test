---
title: Add Screen Field
status: informative
version: 1.0
updated: 2026-08-20
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
3. Добавьте trusted conversion в `FGV2ScreenFieldAdapterRegistry`: `PrepareBindings` только валидирует и собирает ordered binding definitions; `BuildField` потребляет prepared opaque handles и создаёт typed value.
4. Добавьте/расширьте native `IGV2DynamicScreenElement` adapter. Он применяет только prepared value и умеет rollback/reset; gameplay authority не получает.
5. Создайте или измените `WBP_*` только через `unreal-mcp`. После изменения compile и save asset, проверьте native parent, `BindWidget`, schema descriptor и загрузку.
6. Добавьте schema в Screen Template consumer и Lua fixture полного field envelope.
7. Покройте valid apply, malformed value, unknown/mismatched schema, missing required field, binding failure, rollback и повторное применение. Обновите test, фиксирующий полный набор registry schemas.

## Не расширяйте coordinator

`GV2SessionCoordinator` не знает concrete `schema_id` и field names. Добавление schema меняет один adapter/consumer path, а не вводит switch, второй binding registry или отдельную публикацию.
