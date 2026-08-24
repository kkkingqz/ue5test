---
title: Add Screen Field
status: informative
version: 2.0
updated: 2026-08-24
depends_on:
  - README.md
  - ../UI/ScreenTemplates.md
  - ../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# Добавить Screen Field schema

> **Задача:** добавить новую декларативную схему поля экрана (`ui_field`) или переиспользуемой структуры (`ui_value`) в репозиторий контента данными без написания C++.
> **Предмет:** `schema_id`, файл схемы JSON5 в `GameData/<package>/schemas/`, стандартные UI kinds, связывание с UMG виджетом.
> **Нормативно:** [Screen Templates](../UI/ScreenTemplates.md), [Definition Envelope and Schema Rules](../Architecture/DefinitionEnvelopeAndSchemaRules.md), [UI Document](../UI/UIDocumentAndReconciliation.md).

Схемы полей интерфейса являются чистыми данными в репозитории контента (`GV2ContentCore`) и компилируются универсальным компилятором схем. Создание специализированного C++-адаптера схемы для добавления нового поля **не требуется**.

## Процедура

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

4. **Связывание с презентацией (UMG / Dynamic Screen Element)**:
   - В Screen Blueprint укажите `field_id` и `schema_id` в Dynamic Screen Element.
   - Виджет связывает валидированные свойства данных со своими элементами отображения.

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

