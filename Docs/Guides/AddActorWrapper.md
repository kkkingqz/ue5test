---
title: Add Actor Wrapper
status: informative
version: 1.2
updated: 2026-08-17
depends_on:
  - README.md
---

# Добавить доменные методы сущности

> **Задача:** дать категории сущностей доменные методы вместо ручной правки таблиц состояния.
> **Нужно:** понимание разницы definition / экземпляр / обёртка — [RuntimeInstances](../Concepts/RuntimeInstances.md).
> **Нормативно:** [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md), [Commands and Events](../Architecture/CommandsAndEvents.md).

## Шаги

**1. Определить дискриминатор.** Дискриминатор (например, `"player"` или `"npc"`) задаётся в definition актора (`definitions/actors.json5`) и читается ядром для выбора подходящего декоратора обёртки.

**2. Зарегистрировать декоратор в пакете.** На фазе `register` пакет регистрирует фабрику-декоратор для своего дискриминатора через `game.instances.actors.register_type`:

```lua
local function actor_decorator(base)
    return setmetatable({
        get_gold = function() return base.gold or 0 end,
        add_gold = function(amount)
            assert(type(amount) == "number" and amount >= 0)
            base.gold = (base.gold or 0) + amount
            return { ok = true, value = { gold = base.gold } }
        end,
    }, {
        __index = base,
        __newindex = base,
    })
end

function M.register(_ctx)
    game.instances.actors.register_type("player", actor_decorator)
end
```

**3. Полагаться на инварианты ядра.** Базовая обёртка (`base`) автоматически защищает `instance_id`, `definition_id`, `discriminator` от изменения (`ActorDiscriminatorImmutable`), транслирует чтение и запись свойств в состояние инстанса, и не кэшируется.

**4. Разделить ответственность.** Реестр — identity, поиск, создание, удаление, детерминированное перечисление. Обёртка — локальные операции над одной сущностью. Сервис — сценарии над несколькими сущностями.

**5. Добавить спеку.** Полезные кейсы: повторный `get` возвращает свежую таблицу; запись через обёртку видна в состоянии; сохранение обёртки в состояние отвергается валидатором; методы декоратора работают.

## Типичные ошибки

**Мутирующие методы в реестре.** `registry.add_gold(id, 20)` превращает реестр в менеджер сущностей. Правильно — `registry.get(id):add_gold(20)`.

**Регистрация после фазы register.** Реестр декораторов замораживается (`ActorTypeRegistryFrozen`).

**Попытка переопределить identity-поля.** Попытка изменить или подделать `instance_id`, `definition_id`, `discriminator` в декораторе отклоняется ядром.
