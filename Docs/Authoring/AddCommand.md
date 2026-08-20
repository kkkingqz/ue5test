---
title: Add Command
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - LuaGameplayReference.md
---

# Добавить Command

> **Помогает:** добавить действие, которое проверяет условия и меняет gameplay-state.
> **Нормативно:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md).
> **Источник примера:** `GameData/sample/scripts/authoring/gameplay.lua`, `GameData/rh/scripts/authoring/gameplay.lua`.

## Шаги

1. Выберите короткий action-oriented key: `travel`, `shop.buy`, `time.wait_day`. Package превратит его в `<namespace>:command.<key>`.
2. Объявите handler в authoring module. В нём сначала выполняются все `require_*`/`fail()` checks, затем mutation.
3. Общую policy поверх своей или чужой Command добавьте через read-only `validate()`.
4. Операцию одной сущности оставьте entity method, координацию нескольких — Gameplay Service.
5. Добавьте Lua spec на success, typed refusal и неизменность state при отказе.

```lua
commands.travel = function(target)
    player.current_location:require_connected(target)
    player:require_stamina(5, "travel.insufficient_stamina")
    player:spend_stamina(5)
    player:move_to(target)
end

validate(commands.travel, "route_open", function(target)
    if not player.current_location:is_connected(target) then
        fail("travel.route_closed", { target = target })
    end
end)
```

Полные сигнатуры и диагностика: [Lua Gameplay Reference § Command](LuaGameplayReference.md#объявить-команду) и [§ Validator](LuaGameplayReference.md#добавить-независимый-validator).

## Не делайте так

- Не меняйте state из module load, Event или Presentation: mutation window открыт только Command Dispatcher-ом.
- Не вызывайте Command синхронно из Command; используйте `descriptor:later(...)`.
- Не возвращайте ручной `{ ok = false }`: ожидаемый отказ — `fail()` до первой записи.
- Не публикуйте технические команды вида `set_gold`; публичный Command описывает игровое действие.
