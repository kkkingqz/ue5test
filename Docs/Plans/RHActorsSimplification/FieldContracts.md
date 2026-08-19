---
title: Field Contracts and Descriptors Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M1 — Field Contracts and Descriptors

> **Материализует:** [ADR-0032 § 1, 2](../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части дескрипторов полей `field.*` и их интеграции с авторскими прототипами `_ENV`.
> **Задачи:** RAS-01…04.
> **Результат:** модуль `core:module.authoring.field`, сборка дескрипторов полей через `EntityKind.field_name = field.*`, валидация при записи в `ActorWrapper` с поиском схемы по имени сущности `"Actor"`.

## Результат этапа

В авторском слое доступна таблица дескрипторов `field`. Авторские прототипы сущностей (`Actor`, `Location`, `Quest`, `Item`) распознают присваивание дескриптора поля и регистрируют его в схеме свойств (`core:module.authoring.properties`). `ActorWrapper` при обращении к свойствам валидирует значения по схеме сущности `"Actor"`.

## Задачи

- [ ] **RAS-01 — ADR-0032: Field Contracts and Generic Instance Creation**
  - Принят ADR, фиксирующий декларативные дескрипторы полей `field.*`, автоматическую валидацию при записи и фасад `instances.create`.
  - Evidence: `Docs/ADR/0032-field-contracts-and-generic-instance-creation.md`.

- [ ] **RAS-02 — Модуль дескрипторов полей (`core:module.authoring.field`)**
  - Зависимости: RAS-01.
  - Done: создан модуль `Scripts/authoring/field.lua` с фабриками `non_negative_integer()`, `positive_integer()`, `integer()`, `number()`, `string()`, `boolean()`, `ref_definition()`, `ref_instance()`; фабрики возвращают типизированный дескриптор с флагом `__gv2_field_descriptor` и параметрами схемы (`kind`, `storage`, `write_policy`, `schema`); модуль зарегистрирован в `Scripts/bootstrap/manifest.lua`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RAS-03 — Интеграция `field` с авторскими прототипами (`authoring_context.lua`)**
  - Зависимости: RAS-02.
  - Done: модуль `Scripts/authoring/context.lua` инжектирует таблицу `field` в авторский `_ENV`; метаметод `__newindex` прокси прототипа сущности перехватывает присваивание дескриптора поля и регистрирует поле в `properties.register_schema(entity_kind, { fields = { [name] = fspec } })`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RAS-04 — Поиск схемы по базовому имени сущности в `ActorWrapper`**
  - Зависимости: RAS-03.
  - Done: в `Scripts/runtime/actor_registry.lua` методы `read_property` и `write_property` ищут схему поля сначала по `discriminator`, затем по `definition_id`, а затем по generic entity kind `"Actor"`; запись в поле проверяется через `properties.validate_field_value(k, fspec, v)`.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Синтаксис `Actor.gold = field.non_negative_integer()` регистрирует свойство в схеме актора.
- [ ] Попытка записать в `actor.gold` отрицательное или дробное число выбрасывает ошибку валидации поля.
- [ ] Допустимые значения (`0`, `10`, `1000`) успешно записываются в состояние.
