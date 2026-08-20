---
title: Add Command
status: informative
version: 1.5
updated: 2026-08-20
depends_on:
  - README.md
---

# Добавить команду

> **Задача:** добавить действие, меняющее состояние, вместе с проверкой допустимости.
> **Предмет:** authoring и runtime API Command/Validator.
> **Нужно:** понимание пути команды — [GameplayModel](../Concepts/GameplayModel.md).
> **Нормативно:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Stable ID](../Architecture/StableIDSpecification.md), [Authoring Surface](../Architecture/AuthoringSurfaceContract.md).

## Шаги

**1. Выбрать `command_id`.** Kind `command`, например `core:command.shop.buy` (для ядра) или `my_mod:command.craft` (для мода). Имя описывает действие, а не запись в поле: `set_gold` недопустимо как публичная команда.

**2. Объявить обработчик в authoring-скрипте.** `_ENV` уже содержит `commands`, сущности, services, `fail` и `emit`. Package namespace добавляется автоматически.

```lua
commands["shop.buy"] = function(item)
    player:require_gold(item.price)
    services.trade.buy(player, item)
end
```

Отсутствие `return` означает успех. Для отказа вызывается `fail(...)`; ручной `{ ok = ... }` envelope запрещён как авторская идиома.

**3. Добавить независимую политику через `validate`, если она нужна.** Валидатор только читает state и вызывает `fail` при отказе:

```lua
validate(commands["shop.buy"], "shop_open", function(_item)
    if not world.shop_open then
        fail("shop.closed")
    end
end)
```

Проверка, являющаяся частью самой операции, остаётся доменным методом вроде `player:require_gold(...)`, а не отдельным validator.

**4. Разместить логику.** Операция одного владельца — метод сущности. Координация нескольких сущностей — Gameplay Service.

**5. Добавить спеку.** Как минимум: успешный путь, отказ валидатора (состояние не изменилось), отказ на неизвестную команду, отсутствие изменения хэша состояния при отказе. См. [AddLuaSpec](AddLuaSpec.md).

**6. Связать с интерфейсом, если команда доступна игроку.** Экран публикует binding, а нажатие возвращается semantic input-ом — [Semantic Input](../UI/SemanticInput.md).

## Проверка результата

```bash
./build/Headless/gv2-headless --self-test
```

Спека нового поведения должна исполняться обоими хостами. Если команда попадает в golden-прогон, digest изменится — обновляется осознанно в том же изменении.

## Типичные ошибки

**Команда как запись в поле.** `set_health` раскрывает устройство состояния; правильный уровень — действие, которое к этому изменению приводит.

**Весь процесс в обработчике.** Длинный сценарий торговли означает, что нужен service или доменный метод.

**Мутация вне окна.** Изменить состояние можно только пока выполняется обработчик команды. Попытка из подписчика события, из инициализации модуля или из presentation отклоняется и ловится спекой.

**Синхронный вызов команды из команды.** Запрещён (`CommandDispatchReentrant`); отложенная команда ставится в очередь `game.commands.enqueue` и выполнится в собственном окне.

**Отказ строкой вместо кода.** Ошибка — это Stable ID kind `error`, а не человекочитаемая фраза: интерфейс сам решает, как её показать.
