---
title: Add Event
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - LuaGameplayReference.md
---

# Добавить Event и реакцию

> **Помогает:** опубликовать post-commit gameplay fact и независимо обработать его.
> **Нормативно:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md).
> **Источник примера:** `GameData/rh/scripts/authoring/gameplay.lua`, `Tests/Lua/authoring/events_and_presentation.lua`.

## Шаги

1. Назовите уже свершившийся факт: `trade.completed`, `location.entered`. Короткое имя станет `<namespace>:event.<path>`.
2. Вызовите `emit(name, payload)` из Command, entity method или Service после соответствующей mutation.
3. Объявите `on(name, handler)` в authoring module. Subscriber получает fresh wrappers в `payload` и envelope события.
4. Добавьте spec на отсутствие Event при отказе Command, payload и порядок реакции.

```lua
on("trade.completed", function(payload)
    emit("inventory.changed", { owner = payload.buyer })
end)

services.trade = {
    complete = function(buyer, seller, item)
        buyer:receive_item(seller:take_item(item))
        emit("trade.completed", {
            buyer = buyer,
            seller = seller,
            item = item,
        })
    end,
}
```

Полные сигнатуры и execution scope: [Lua Gameplay Reference § Event](LuaGameplayReference.md#опубликовать-и-обработать-event).

## Не делайте так

- Event не задаёт вопрос и не отменяет Command: для этого нужен Validator.
- Subscriber не меняет state напрямую; следующую mutation поставьте через `commands.<name>:later(...)`.
- Payload не содержит callback/UObject/wrapper identity. Authoring adapter переносит wrappers как tagged references.
- Не используйте `before_*`: EventBus публикует только post-commit facts.
