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

- [ ] **RAS-02 — Модуль дескрипторов полей (`core:module.authoring.field`)**
  - Зависимости: RAS-01.
  - Done: создан модуль `Scripts/authoring/field.lua` с фабриками `non_negative_integer()`, `positive_integer()`, `integer()`, `number()`, `string()`, `boolean()`, `ref_definition()`, `ref_instance()`; фабрики возвращают типизированный дескриптор с флагом `__gv2_field_descriptor` и параметрами схемы (`kind`, `storage`, `write_policy`, `schema`); модуль зарегистрирован в `Scripts/bootstrap/manifest.lua`.
  - Evidence: `Scripts/authoring/field.lua`, `Scripts/authoring/context.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [ ] **RAS-03 — Интеграция `field` с авторскими прототипами (`authoring_context.lua`)**
  - Зависимости: RAS-02.
  - Done: модуль `Scripts/authoring/context.lua` инжектирует таблицу `field` в авторский `_ENV`; метаметод `__newindex` прокси прототипа сущности перехватывает присваивание дескриптора поля и регистрирует поле в `properties.register_schema(entity_kind, { fields = { [name] = fspec } })`.
  - Evidence: `Scripts/authoring/field.lua`, `Scripts/authoring/context.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [ ] **RAS-04 — Валидация значения при записи в `ActorWrapper`**
  - Зависимости: RAS-03.
  - Done: в `Scripts/runtime/actor_registry.lua` методы `read_property` и `write_property` работают с эффективной схемой сущности; запись проверяется через `properties.validate_field_value(k, fspec, v)` до мутации состояния; отказ не оставляет частично записанного значения.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [ ] **RAS-13 — Композиция схем вместо цепочки перекрытия**
  - Зависимости: RAS-04.
  - Текущая реализация выбирает **первый найденный** источник целиком: `get_schema(discriminator) or get_schema(definition_id) or get_schema("Actor")`. Регистрация схемы по дискриминатору молча отключает все поля, объявленные на generic entity kind.
  - Done: эффективная схема сущности собирается слиянием по имени поля в порядке `generic → definition_id → discriminator`; более конкретный источник переопределяет одноимённое поле и не скрывает остальные; переопределение может сузить `min`/`max`/`min_length`/`max_length`, смена `kind` — ошибка сборки схемы; композиция вычисляется однократно при заморозке реестров, и путь записи получает плоскую таблицу полей; диагностика ошибки поля указывает источник, давший это поле.
  - Evidence: `Scripts/authoring/properties.lua`, `Scripts/runtime/actor_registry.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [ ] **RAS-14 — Запрет повторного объявления поля**
  - Зависимости: RAS-03.
  - `register_schema` переведён с замены на слияние без обнаружения дубликатов: два пакета, объявившие одно поле, дают молчаливое «побеждает последний загруженный».
  - Done: повторное объявление поля с уже занятым именем для той же сущности отклоняется ошибкой `FieldAlreadyDeclared`; осознанное переопределение выражается явным `field.<kind>({ override = true })`; форма совпадает с замещением Lua-модулей ([ADR-0025](../../ADR/0025-lua-module-replacement-and-export-freezing.md)); отрицательный случай покрыт спекой.
  - Evidence: `Scripts/authoring/properties.lua`, `Tests/Lua/actors/field_contracts.lua`.

- [ ] **RAS-15 — Сохранить проверку вида актора**
  - Зависимости: RAS-03.
  - В `actor_registry.lua` условие `is_known_disc` расширено на `properties.get_schema("Actor") ~= nil`: одного объявления поля где угодно достаточно, чтобы любой неизвестный `discriminator` стал допустимым, а опечатка в нём — молчаливым созданием актора.
  - Done: наличие схемы generic entity kind не делает `discriminator` известным; `ActorTypeNotRegistered` продолжает отклонять незарегистрированный вид; вид актора, нужный пакету, объявляется явно; отрицательный случай покрыт спекой.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `Tests/Lua/actors/field_contracts.lua`.

## Проверка milestone

- [ ] Синтаксис `Actor.gold = field.non_negative_integer()` регистрирует свойство в схеме актора.
- [ ] Попытка записать в `actor.gold` отрицательное или дробное число выбрасывает ошибку валидации поля.
- [ ] Допустимые значения (`0`, `10`, `1000`) успешно записываются в состояние.
- [ ] Поле, объявленное на `Actor`, действует и для актора со схемой по `discriminator`.
- [ ] Повторное объявление поля без `override` отклоняется.
- [ ] Неизвестный `discriminator` отклоняется независимо от объявленных полей.
