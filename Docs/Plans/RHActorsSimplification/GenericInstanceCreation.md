---
title: Generic Instance Creation Tasks
status: draft
version: 1.0
updated: 2026-08-19
depends_on:
  - FieldContracts.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M2 — Generic Instance Creation

> **Материализует:** [ADR-0032 § 4](../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части создания динамических сущностей через фасад `instances.create()`.
> **Задачи:** RAS-05…07.
> **Результат:** обобщённый фасад `instances.create(kind, payload)` в авторском `_ENV`, нормализация ссылок и инкапсуляция `instance_allocator` и коллекций состояния.

## Результат этапа

Авторские скрипты получают высокоуровневый фасад `instances.create(kind, payload)`. Фасад берет на себя аллокацию идентификаторов, валидацию определений и владельцев, и запись в соответствующую секцию `game.state` (например, `item_instances`). Геймплейные пакеты избавлены от прямого взаимодействия с `instance_allocator` и сырыми таблицами состояния.

## Задачи

- [ ] **RAS-05 — Механизм обобщённого создания экземпляров (`core:module.runtime.instance_registry` / `authoring_context.lua`)**
  - Зависимости: RAS-04.
  - Done: реализована функция создания экземпляров сущностей `instances.create(kind, data)`; поддерживает создание сущностей вида `"item"` (запись в `game.state.item_instances`) и `"actor"` (делегирование в `game.instances.actors.create`); аллокация ID выполняется через `instance_allocator.allocate(game.state, kind)`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RAS-06 — Нормализация дескрипторов и ссылок владельцев**
  - Зависимости: RAS-05.
  - Done: при передаче `definition = item_def_or_id` фасад автоматически извлекает строковый ID (`def.id` или `def.definition_id` или сырую строку); при передаче `owner = self` (ActorWrapper) извлекается канонический `instance_id`; проверяется валидность определения в репозитории (`game.repository.get`).
  - Evidence: <!-- tests/commit/PR -->

- [ ] **RAS-07 — Экспорт фасада `instances.create` в авторское окружение `_ENV`**
  - Зависимости: RAS-06.
  - Done: `Scripts/authoring/context.lua` предоставляет глобальный объект `instances` (с методом `instances.create`) в `_ENV` авторских скриптов; прямой доступ защищён от перезаписи.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Вызов `instances.create("item", { definition = "rh:item.misc.herb", owner = player })` создаёт экземпляр в `game.state.item_instances` и возвращает `item_id`.
- [ ] Некорректные параметры (несуществующее определение, пустой ID) вызывают понятные типизированные ошибки.
- [ ] Геймплейный код не требует прямого `require("core:module.runtime.instance_allocator")`.
