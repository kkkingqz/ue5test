---
title: Add Command
status: informative
version: 1.2
updated: 2026-08-16
depends_on:
  - README.md
---

# Добавить команду

> **Задача:** добавить действие, меняющее состояние, вместе с проверкой допустимости.
> **Нужно:** понимание пути команды — [GameplayModel](../Concepts/GameplayModel.md).
> **Нормативно:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Stable ID](../Architecture/StableIDSpecification.md), [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md).

## Шаги

**1. Выбрать `command_id`.** Kind `command`, например `core:command.shop.buy` (для ядра) или `my_mod:command.craft` (для мода). Имя описывает действие, а не запись в поле: `set_gold` недопустимо как публичная команда.

**2. Написать валидатор, если действие может быть недопустимо.** Валидатор только читает состояние и репозиторий, ничего не меняет и возвращает либо разрешение, либо типизированный отказ с кодом kind `error`. Регистрируется на фазе `register` через `game.commands.validators.register`.

**3. Зарегистрировать обработчик.** Обработчик регистрируется на фазе `register` через реестр `game.commands.handlers.register`. Он остаётся тонким: делегирует логику сервису или доменному методу, инициирует факты событий и возвращает результат.

```lua
function M.register(_ctx)
    game.commands.handlers.register("core:command.shop.buy", function(request)
        local trade_service = game.services.get("core:service.trade")
        if not trade_service then
            return {
                ok = false,
                error = { code = "core:error.service.not_found", params = { service_id = "core:service.trade" } },
            }
        end
        return trade_service.buy(request.args)
    end)
end
```

Никаких правок `ingress.lua` или C++ не требуется: диспетчер находит обработчик по ключу автоматически.

**4. Разместить логику.** Операция над одной сущностью — метод обёртки (`actor.add_gold`). Сценарий над несколькими — Gameplay Service (`game.services.register`).

**5. Добавить спеку.** Как минимум: успешный путь, отказ валидатора (состояние не изменилось), отказ на неизвестную команду, отсутствие изменения хэша состояния при отказе. См. [AddLuaSpec](AddLuaSpec.md).

**6. Связать с интерфейсом, если команда доступна игроку.** Экран публикует binding, а нажатие возвращается semantic input-ом — [Semantic Input](../UI/SemanticInput.md).

## Проверка результата

```bash
./build/Headless/gv2-headless --self-test
```

Спека нового поведения должна исполняться обоими хостами. Если команда попадает в golden-прогон, digest изменится — обновляется осознанно в том же изменении.

## Типичные ошибки

**Команда как запись в поле.** `set_health` раскрывает устройство состояния; правильный уровень — действие, которое к этому изменению приводит.

**Правила в обработчике.** Сто пятьдесят строк торговли в обработчике — признак того, что нужен сервис.

**Мутация вне окна.** Изменить состояние можно только пока выполняется обработчик команды. Попытка из подписчика события, из инициализации модуля или из presentation отклоняется и ловится спекой.

**Синхронный вызов команды из команды.** Запрещён (`CommandDispatchReentrant`); отложенная команда ставится в очередь `game.commands.enqueue` и выполнится в собственном окне.

**Отказ строкой вместо кода.** Ошибка — это Stable ID kind `error`, а не человекочитаемая фраза: интерфейс сам решает, как её показать.
