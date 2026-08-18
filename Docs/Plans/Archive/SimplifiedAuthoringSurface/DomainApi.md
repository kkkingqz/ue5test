---
title: Domain API Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - AuthoringEnvironment.md
  - ../../../Architecture/CommandsAndEvents.md
---

# M3 — Domain API

> **Материализует:** [Commands and Events](../../../Architecture/CommandsAndEvents.md) в части предусловий и доменных операций.
> **Задачи:** SAS-10…13.
> **Результат:** правило игры выражается предусловием и операцией, без разбора результатов и транспорта.

## Результат этапа

`player:require_gold(price)` и `player:spend_gold(price)` — два вызова с разным смыслом, и оба читаются без пояснений.

## Задачи

- [x] **SAS-10 — Предусловия `require_*`**
  - Зависимости: SAS-04.
  - Done: `require_location`, `require_stamina`, `require_gold` ничего не мутируют; при выполненном условии возвращают управление, при невыполненном дают типизированный отказ команды с параметрами; отказ до первой мутации остаётся обычным отказом, после — `AuthoringFailAfterMutation` по существующему правилу; rollback не вводится.
  - Evidence: `Scripts/authoring/context.lua` (`M.fail` с нелокальным выходом через сентинел `FAIL_SENTINEL`), `GameData/rh/scripts/gameplay/actors.lua` (`require_gold`, `require_stamina`, `require_location`, `location:require_connected`), `Tests/Lua/authoring/simplified_surface.lua` (`preconditions_and_non_local_exit`).

- [x] **SAS-11 — Операции `spend_*` без разбора результата**
  - Зависимости: SAS-10.
  - Done: успешная операция возвращает управление, и дизайнер не пишет проверку результата; **невыполненное предусловие в `spend_*` даёт fault, а не отказ** — если `require_*` является санкционированной проверкой, то отказ списания означает, что её забыли, и сообщение указывает на пропущенную проверку (`PreconditionNotChecked`); автопродвижение реализуется нелокальным выходом; ограничение зафиксировано: обёртывание в `pcall` внутри designer-кода запрещено.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua` (`spend_gold`, `spend_stamina` выбрасывают `PreconditionNotChecked` при попытке списания без достаточных ресурсов), `Tests/Lua/authoring/simplified_surface.lua` (`spend_operations_and_precondition_not_checked_fault`).

- [x] **SAS-12 — Обработчик получает объекты**
  - Зависимости: SAS-04.
  - `travel.lua` сегодня вручную разбирает транспортный DTO, а designer-код смешивает `current_location`, `current_location_id` и `world.current_location`.
  - Done: аргумент команды приходит обработчику готовым handle (`DefinitionHandle` / `ActorWrapper`), разбора DTO в designer-коде не остаётся; в designer-facing API остаётся `player.current_location`, возвращающая `DefinitionHandle<Location>`; canonical ID остаётся внутренним представлением; negative case на невалидные типы.
  - Evidence: `Scripts/authoring/context.lua` (распаковка и реконсилиация `primary_arg` в `wrapped_handler`), `Scripts/runtime/actor_registry.lua` (`current_location` возвращает `DefinitionHandle`), `GameData/rh/scripts/gameplay/travel.lua`, `Tests/Lua/authoring/simplified_surface.lua` (`command_handlers_receive_definition_handles`).

- [x] **SAS-13 — Единый доменный API актора**
  - Зависимости: SAS-11.
  - Done: `add_gold`, `spend_gold`, `add_stamina`, `spend_stamina`, `add_item` и `travel` существуют в одном месте — на акторе; двойная форма вызова `function(a, b)` убрана в пользу устойчивого colon-синтаксиса; смена локации выполняется `player:travel(target)`, который меняет состояние и публикует сопутствующие факты, а проверка связности остаётся в команде через `location:require_connected(target)`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua` (`actor_decorator` и `location_definition_decorator`), `Scripts/authoring/properties.lua` (`default_location_decorator`), `GameData/rh/scripts/gameplay/` (`travel.lua`, `shop.lua`, `work.lua`, `time.lua`), `Tests/Lua/authoring/simplified_surface.lua` (`unified_actor_domain_api`).

## Проверка milestone

- [x] `require_*` даёт отказ, `spend_*` при непроверенном предусловии — fault.
- [x] Designer-код не разбирает результат доменной операции.
- [x] Аргумент команды приходит handle, а не строкой или таблицей.
- [x] `current_location_id` в designer-facing API отсутствует.
