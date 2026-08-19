---
title: Field Contracts and Descriptors Tasks
status: normative
version: 1.1
updated: 2026-08-19
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M1 — Field Contracts and Descriptors

> **Материализует:** [ADR-0032 § 1—4](../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части дескрипторов полей `field.*`, их интеграции с авторскими прототипами `_ENV` и правил композиции схем.
> **Задачи:** RAS-01…04, RAS-13…15.
> **Результат:** модуль `core:module.authoring.field`, сборка дескрипторов полей через `EntityKind.field_name = field.*`, валидация при записи по композиции схем трёх источников.

## Результат этапа

В авторском слое доступна таблица дескрипторов `field`. Авторские прототипы сущностей (`Actor`, `Location`, `Quest`, `Item`) распознают присваивание дескриптора поля и регистрируют его в схеме свойств (`core:module.authoring.properties`). `ActorWrapper` при обращении к свойствам валидирует значения по эффективной схеме, собранной из generic entity kind, `definition_id` и `discriminator`.

## Задачи

- [x] **RAS-01 — ADR-0032: Field Contracts and Generic Instance Creation**
  - Принят ADR, фиксирующий декларативные дескрипторы полей `field.*`, автоматическую валидацию при записи и фасад `instances.create`.
  - Evidence: `Docs/ADR/0032-field-contracts-and-generic-instance-creation.md`.

- [x] **RAS-02 — Модуль дескрипторов полей (`core:module.authoring.field`)**
  - Зависимости: RAS-01.
  - Done: создан модуль `Scripts/authoring/field.lua` с фабриками `non_negative_integer()`, `positive_integer()`, `integer()`, `number()`, `string()`, `boolean()`, `enum()`, `ref_definition()`, `ref_instance()`; фабрики возвращают типизированный дескриптор с флагом `__gv2_field_descriptor` и параметрами схемы (`kind`, `storage`, `write_policy`, `override`); модуль зарегистрирован в `Scripts/bootstrap/manifest.lua`.
  - Evidence: `Scripts/authoring/field.lua`, `Scripts/authoring/context.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [x] **RAS-03 — Интеграция `field` с авторскими прототипами (`authoring_context.lua`)**
  - Зависимости: RAS-02.
  - Done: модуль `Scripts/authoring/context.lua` инжектирует таблицу `field` в авторский `_ENV`; метаметод `__newindex` прокси прототипа сущности перехватывает присваивание дескриптора поля и регистрирует поле в `properties.register_schema(entity_kind, { fields = { [name] = fspec } })`.
  - Evidence: `Scripts/authoring/field.lua`, `Scripts/authoring/context.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [x] **RAS-04 — Валидация значения при записи в `ActorWrapper`**
  - Зависимости: RAS-03.
  - Done: в `Scripts/runtime/actor_registry.lua` методы `read_property` и `write_property` работают с эффективной схемой сущности; запись проверяется через `properties.validate_field_value(k, fspec, v)` до мутации состояния; отказ не оставляет частично записанного значения.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [x] **RAS-13 — Композиция схем вместо цепочки перекрытия**
  - Зависимости: RAS-04.
  - Done: эффективная схема сущности собирается слиянием по имени поля в порядке `generic → definition_id → discriminator` в `properties.get_effective_schema`; более конкретный источник переопределяет одноимённое поле и не скрывает остальные; переопределение сужает ограничения (`min`/`max`/`min_length`/`max_length`), смена `kind` — ошибка `InvalidFieldOverrideKindMismatch`; композиция кэшируется при заморозке реестров; диагностика ошибки поля указывает источник.
  - Evidence: `Scripts/authoring/properties.lua`, `Scripts/runtime/actor_registry.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [x] **RAS-14 — Запрет повторного объявления поля**
  - Зависимости: RAS-03.
  - Done: повторное объявление поля с уже занятым именем для той же сущности отклоняется ошибкой `FieldAlreadyDeclared`; осознанное переопределение выражается явным `field.<kind>({ override = true })`; отрицательный случай покрыт спекой.
  - Evidence: `Scripts/authoring/properties.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [x] **RAS-15 — Сохранить проверку вида актора**
  - Зависимости: RAS-03.
  - Done: наличие схемы generic entity kind не делает `discriminator` известным; `ActorTypeNotRegistered` продолжает отклонять незарегистрированный вид; отрицательный случай покрыт спекой.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `Tests/Lua/actors/field_contracts.lua`.

## Проверка milestone

- [x] Синтаксис `Actor.gold = field.non_negative_integer()` регистрирует свойство в схеме актора.
- [x] Попытка записать в `actor.gold` отрицательное или дробное число выбрасывает ошибку валидации поля.
- [x] Допустимые значения (`0`, `10`, `1000`) успешно записываются в состояние.
- [x] Поле, объявленное на `Actor`, действует и для актора со схемой по `discriminator`.
- [x] Повторное объявление поля без `override` отклоняется.
- [x] Неизвестный `discriminator` отклоняется независимо от объявленных полей.
