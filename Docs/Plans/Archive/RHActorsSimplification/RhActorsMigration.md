---
title: RH Actors Migration Tasks
status: archived
version: 1.1
updated: 2026-08-19
depends_on:
  - GenericInstanceCreation.md
  - ../../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../../ADR/0030-textsystem-layer-and-data-driven-package-set.md
  - ../../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M3 — RH Actors Migration

> **Материализует:** [ADR-0032 § 5, 8, 9](../../../ADR/0032-field-contracts-and-generic-instance-creation.md) в части очистки и упрощения `GameData/rh/scripts/gameplay/actors.lua`.
> **Задачи:** RAS-08…10, RAS-17.
> **Результат:** `actors.lua` в пакете `rh` переведён на декларативные контракты полей и явные доменные методы; удалены `RESOURCES`, `validate_amount`, fallback на player и прямой импорт `instance_allocator`.

## Результат этапа

Файл `GameData/rh/scripts/gameplay/actors.lua` содержит исключительно доменную семантику игры RH. Поля `gold` и `stamina` объявлены через дескрипторы контрактов. Методы оперируют строго получателем `self`. Создание предметов использует фасад `instances.create`.

## Задачи

- [x] **RAS-08 — Перевод полей `gold` и `stamina` на `field.non_negative_integer()`**
  - Зависимости: RAS-07.
  - Done: в `GameData/rh/scripts/gameplay/actors.lua` добавлены декларации `Actor.gold = field.non_negative_integer()` и `Actor.stamina = field.non_negative_integer()`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`.

- [x] **RAS-09 — Упрощение методов ресурсов и удаление `RESOURCES` / `validate_amount`**
  - Зависимости: RAS-08.
  - Done: методы `get_gold`, `require_gold`, `spend_gold`, `add_gold`, `get_stamina`, `require_stamina`, `spend_stamina`, `add_stamina` переписаны как чистые явные функции; удалены таблица `RESOURCES` и fallback `self -> player`, а проверка аргумента сведена к одной локальной функции (RAS-17); `spend_*` производит прямую арифметику `self.prop = self:get_prop() - amount`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`.

- [x] **RAS-10 — Перевод `Actor:add_item()` на `instances.create()` и очистка импортов**
  - Зависимости: RAS-09.
  - Done: метод `Actor:add_item(item_def_or_id)` переведён на вызов `instances.create("item", { definition = item_def_or_id, owner = self })`; удалён импорт `instance_allocator` и прямое обращение к `game.state.item_instances`; в `GameData/rh/scripts/gameplay/actors.lua` не осталось импортов `core:module.runtime.*`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`.

- [x] **RAS-17 — Локальная проверка аргументов в `rh`**
  - Зависимости: RAS-09.
  - Контракт поля защищает результирующее состояние и по построению не ловит `spend_gold(-10)`: такая запись увеличивает баланс и инварианта не нарушает. Удаление `validate_amount` без замены оставило бы методы без всякой проверки аргумента.
  - Done: `GameData/rh/scripts/gameplay/actors.lua` содержит одну локальную проверку аргумента, применяемую методами `require_*`, `spend_*` и `add_*`; проверка не поднимается в общие слои и не превращается в дескриптор; в плане записано, что она снимается при появлении валидаторов команд; отрицательный случай покрыт спекой.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`, `Tests/Lua/economy/actor_rh_economy.lua`.

## Проверка milestone

- [x] Файл `GameData/rh/scripts/gameplay/actors.lua` не содержит импортов `core:module.runtime.*`.
- [x] В файле нет прямых обращений к `game.state`.
- [x] Все доменные методы актора (`get/require/spend/add` для золота и выносливости, `add_item`) работают корректно.
- [x] `spend_gold(-10)` отклоняется проверкой аргумента, а не проходит молча.
