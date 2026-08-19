---
title: Generic Instance Creation Tasks
status: normative
version: 1.1
updated: 2026-08-19
depends_on:
  - FieldContracts.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M2 — Generic Instance Creation

> **Материализует:** [ADR-0032 § 6](../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части создания динамических сущностей через фасад `instances.create()`.
> **Задачи:** RAS-05…07, RAS-16.
> **Результат:** обобщённый фасад `instances.create(kind, payload)` в авторском `_ENV`, нормализация ссылок и инкапсуляция `instance_allocator` и коллекций состояния.

## Результат этапа

Авторские скрипты получают высокоуровневый фасад `instances.create(kind, payload)`. Фасад берёт на себя аллокацию идентификаторов, валидацию определений и владельцев и запись в секцию `game.state`, соответствующую виду. Ядро при этом не знает ни одного имени вида: вид объявляет пакет, имя секции выводится из имени вида. Геймплейные пакеты избавлены от прямого взаимодействия с `instance_allocator` и сырыми таблицами состояния.

## Задачи

- [x] **RAS-16 — Реестр видов экземпляров**
  - Зависимости: RAS-04.
  - Done: вид экземпляра объявляется пакетом на фазе `register` (`instances.register_kind`) и замораживается вместе с остальными реестрами; имя секции `game.state` **выводится** из имени вида по общему правилу (`item` → `item_instances`), поэтому раскладка сейва не меняется и миграция не требуется; ядро не содержит ни одного имени вида; неизвестный вид — типизированный отказ `UnknownInstanceKind`; `actor` делегируется в реестр акторов.
  - Evidence: `Scripts/runtime/instance_registry.lua`, `Tests/Lua/actors/generic_instance_creation.lua`.

- [x] **RAS-05 — Механизм обобщённого создания экземпляров**
  - Зависимости: RAS-16.
  - Done: реализована функция `instances.create(kind, data)`; вид разрешается через реестр RAS-16, аллокация ID выполняется через `instance_allocator.allocate(game.state, kind)`, запись идёт в выведенную секцию состояния; создание вида `actor` делегируется в `game.instances.actors.create`.
  - Evidence: `Scripts/runtime/instance_registry.lua`, `Tests/Lua/actors/generic_instance_creation.lua`.

- [x] **RAS-06 — Нормализация дескрипторов и ссылок владельцев**
  - Зависимости: RAS-05.
  - Done: при передаче `definition = item_def_or_id` фасад автоматически извлекает строковый ID (`def.id` или `def.definition_id` или сырую строку); при передаче `owner = self` (ActorWrapper) извлекается канонический `instance_id`; проверяется валидность определения в репозитории (`game.repository.get`).
  - Evidence: `Scripts/authoring/context.lua`, `Scripts/runtime/instance_registry.lua`, `Tests/Lua/actors/generic_instance_creation.lua`.

- [x] **RAS-07 — Экспорт фасада `instances.create` в авторское окружение `_ENV`**
  - Зависимости: RAS-06.
  - Done: `Scripts/authoring/context.lua` предоставляет глобальный объект `instances` (с методами `instances.create` и `instances.register_kind`) в `_ENV` авторских скриптов; прямой доступ защищён от перезаписи (`AuthoringGlobalWriteDisallowed`).
  - Evidence: `Scripts/authoring/context.lua`, `Scripts/runtime/instance_registry.lua`, `Tests/Lua/actors/generic_instance_creation.lua`.

## Проверка milestone

- [x] Вызов `instances.create("item", { definition = "rh:item.misc.herb", owner = player })` создаёт экземпляр в `game.state.item_instances` и возвращает `item_id`.
- [x] Некорректные параметры (несуществующее определение, пустой ID) вызывают понятные типизированные ошибки.
- [x] Геймплейный код не требует прямого `require("core:module.runtime.instance_allocator")`.
- [x] В ядре нет ни одного имени вида экземпляра; неизвестный вид даёт типизированный отказ.
- [x] Раскладка секций сейва не изменилась.
