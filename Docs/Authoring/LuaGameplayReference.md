---
title: Lua Gameplay Authoring Reference
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/AuthoringSurfaceContract.md
---

# Lua gameplay: практический справочник

> **Помогает:** объявлять Commands, Validators, entity fields/methods, Runtime Instances, Services и Events в package Lua.
> **Нормативно:** [Lua Authoring Surface Contract](../Architecture/AuthoringSurfaceContract.md), [Commands and Events](../Architecture/CommandsAndEvents.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).
> **Источники примеров:** `GameData/rh/scripts/`, `GameData/textsystem/scripts/`, `GameData/sample/scripts/` и conformance fixtures в `Tests/Lua/`.

Authoring module исполняется с подготовленным `_ENV`: infrastructure импортировать и таблицу модуля возвращать не нужно. Объявляйте только `local` вспомогательные значения. Новый global отклоняется как `AuthoringGlobalWriteDisallowed`.

## Объявить команду

Назначение: создать Command Handler текущего package.

```lua
commands[key] = function(...)
    -- проверить предусловия, затем изменить canonical state
    return result -- необязательно
end
```

Пример из `GameData/rh/scripts/authoring/gameplay.lua`:

```lua
local tavern = location("city.tavern")

local function handle_wait_day()
    player:require_location(tavern)
    player:add_stamina(10)
end

commands["time.wait_day"] = handle_wait_day
```

Тот же entry point используется минимальным package `sample`:

```lua
local function handle_travel(target)
    player.current_location:require_connected(target)
    player:move_to(target)
end

commands.travel = handle_travel
```

Короткий key становится `<package>:command.<key>`; полный Stable ID допустим для cross-package reference. Handler возвращает `nil` или portable value. Не вызывайте его напрямую и не меняйте gameplay-state вне Command path.

Типичные ошибки: `InvalidCommandKey`, `InvalidCommandHandler`, `CommandAlreadyDefined`, `CommandDeclarationAfterFreeze`, `UnknownCommandKey`.

## CommandDescriptor: run, later и replaceable

Назначение: ссылаться на объявленную Command без callback. Сигнатуры:

```lua
local descriptor = commands[key]
local result, sequence = descriptor:run(...)
local request = descriptor:later(...)
descriptor:set_replaceable(enabled)
descriptor:replaceable(enabled) -- alias
```

`run` начинает отдельную синхронную dispatch и нужен прежде всего entry point/tests. Внутри активной Command используйте `later`:

```lua
commands.reward = function(amount)
    player:add_gold(amount)
end

commands.finish_work = function()
    commands.reward:later(10)
end
```

Форма покрыта `Tests/Lua/authoring/commands.lua`. `:run()` внутри Command отклоняется как `AuthoringNestedRunDisallowed`; `:later()` внутри Validator — как `AuthoringValidatorSideEffectDisallowed`.

Handler по умолчанию незаменяем. Разрешайте override только осознанно:

```lua
commands.travel = function(target)
    player:move_to(target)
end
commands.travel:replaceable(true)
```

Эквивалентная атомарная форма объявления: `commands.travel = { handler = function(target) ... end, replaceable = true }`.

## Добавить независимый Validator

Назначение: наложить read-only policy на Command, в том числе объявленную другим package.

```lua
validate(command_descriptor_or_id, validator_name, function(...)
    -- только чтение; при отказе вызвать fail(...)
end)
```

Пример по форме `Tests/Lua/authoring/command_validators.lua`:

```lua
validate("rh:command.travel", "route_open", function(target)
    if not player.current_location:is_connected(target) then
        fail("travel.route_closed", { target = target })
    end
end)
```

`validator_name` — один lowercase `snake_case` segment. Validator получает те же декодированные аргументы, что Handler. Запрещены mutation, `emit`, `show_*`, `commands.*:later` и mutating Service operations.

Типичные ошибки: `InvalidAuthoringValidatorCommand`, `InvalidAuthoringValidatorName`, `InvalidAuthoringValidatorFunction`, `AuthoringValidatorDuplicate`, `AuthoringValidatorTargetMissing`, `AuthoringValidatorDeclarationAfterFreeze`, `AuthoringValidatorSideEffectDisallowed`.

## Отказать без частичной мутации

Назначение: вернуть typed refusal из Command или Validator.

```lua
fail(error_key, params_optional)
```

Пример из `GameData/rh/scripts/gameplay/actors.lua`:

```lua
function Actor:require_gold(amount, error_key)
    local current = self:get_gold()
    if current < amount then
        fail(error_key or "economy.insufficient_gold", {
            current_gold = current,
            required_gold = amount,
        })
    end
end
```

Короткий key становится `<declaring_package>:error.<key>` и соответствует `<declaring_package>:text.error.<key>`. Сначала проверяйте все предусловия, затем мутируйте: `fail()` после первой записи даёт `AuthoringFailAfterMutation`. Вне Command/Validator он даёт `AuthoringFailOutsideCommand`. Обычный `error()` означает runtime fault, а не ожидаемый игровой отказ.

## Объявить поля и методы сущности

Назначение: расширить entity kind без ручных wrappers и registry calls.

```lua
Prototype.field_name = field.<constructor>(options_optional)

function Prototype:method_name(...)
    -- self — fresh disposable wrapper
end
```

Встроенные prototypes: `Actor`, `Location`, `Quest`, `Item`. Неизвестное PascalCase-имя создаёт prototype custom kind; форма `Faction` покрыта `Tests/Lua/authoring/simplified_surface.lua`.

Production-пример:

```lua
Actor.gold = field.non_negative_integer()

function Actor:get_gold()
    return self.gold or 0
end

function Actor:add_gold(amount)
    self.gold = self:get_gold() + amount
end
```

Вызывайте методы через `:`. Не храните wrapper в canonical state; передавайте его через Command/Event arguments — adapter превратит его в portable tagged reference. Повторное объявление без явного schema override, конфликт метода/поля и отсутствующий receiver отклоняются до запуска session или дают `MissingReceiver`.

## Справочник field.*

Назначение: объявить тип, constraints, storage и write policy поля. Все constructors возвращают descriptor; это не runtime value.

| Constructor | Сигнатура | Рабочий пример |
|---|---|---|
| non-negative integer | `field.non_negative_integer(opts?)` | `Actor.gold = field.non_negative_integer()` |
| positive integer | `field.positive_integer(opts?)` | `Quest.stage = field.positive_integer()` |
| integer | `field.integer(opts?)` | `Actor.mana = field.integer({ min = 0, max = 100 })` |
| number | `field.number(opts?)` | `Item.weight = field.number({ min = 0.0 })` |
| string | `field.string(opts?)` | `Actor.title = field.string({ min_length = 2, max_length = 40 })` |
| boolean | `field.boolean(opts?)` | `Quest.completed = field.boolean({ default = false })` |
| enum | `field.enum(values, opts?)` | `Quest.difficulty = field.enum({ "easy", "normal", "hard" })` |
| Definition reference | `field.ref_definition(target_kind, opts?)` | `Actor.home = field.ref_definition("location")` |
| Instance reference | `field.ref_instance(target_kind, opts?)` | `Item.owner = field.ref_instance("actor", { nullable = true })` |

Все формы constructors проверены в `Tests/Lua/actors/field_contracts.lua`; production использует `non_negative_integer` для `gold` и `stamina`.

Общие options: `storage = "definition" | "runtime_state"`, `write_policy = "read_only" | "plain" | "managed"`, `nullable`, `required`, `default`, `min`, `max`, `min_length`, `max_length`, `operations`, `override`. `override = true` означает явную замену inherited field contract, а не deep merge. Для `managed` перечислите опубликованные domain operations.

Runtime override поля Definition wrapper удаляется через `definition:reset(field_name)`:

```lua
commands.restore_default_tax = function(target_location)
    target_location:reset("tax_rate")
end
```

Форма проверена в `Tests/Lua/authoring/runtime_state.lua`. Проверяйте ограничения до записи. Несовместимый value, unknown field, конфликт schema или запись в `read_only`/`managed` вне разрешённой operation отклоняются canonical-state guard-ом.

## Получить player, world, Definition и Actor

Назначение: читать актуальные definitions и Runtime Instances через fresh wrappers.

```lua
local hero = player
local day = world.day
local sword = def.item("weapon.iron_sword")
local tavern = location("city.tavern")
local merchant = actor("npc.merchant")
local guards = actors("npc.guard")
```

- `player` — текущий player actor; `world` — wrapper canonical world section.
- `def.<kind>(name)` — одна Definition указанного kind. `def` не является function.
- `location(name)` — сокращение для location Definition.
- `actor(name)` — ровно один actor instance указанной actor Definition; ошибки `ActorInstanceNotFound` и `ActorInstanceAmbiguous`.
- `actors(name)` — deterministic list всех подходящих actor instances.

Короткое имя получает namespace текущего package; для cross-package lookup передайте полный Stable ID. Definition immutable. Wrapper disposable: не сравнивайте его identity и не сохраняйте сам объект в state.

## Создать Runtime Instance

Назначение: зарегистрировать generic instance category и создать instance внутри Command mutation window.

```lua
instances.register_kind(kind, options_optional)
local instance_or_id = instances.create(kind, data)
```

Production и conformance form:

```lua
instances.register_kind("item")

commands.start_game = function()
    local hero = instances.create("actor", {
        definition = def.actor("character.hero"),
        current_location = location("city.tavern"),
        is_player = true,
    })

    local sword_id = instances.create("item", {
        definition = def.item("weapon.iron_sword"),
        owner = hero,
    })
    return { player = hero, sword_id = sword_id }
end
```

`actor` — встроенная category и возвращает actor wrapper. Generic category возвращает instance ID; перед использованием её один раз объявляют `register_kind`. Опция `{ section_name = "custom_vehicles" }` задаёт canonical section явно. `definition`/`definition_id` и `owner`/`owner_id` нормализуются adapter-ом.

Форма проверена `Tests/Lua/actors/generic_instance_creation.lua`. Unknown kind, duplicate registration, invalid definition/owner и вызов вне mutation window отклоняются; category registry заморожен после bootstrap.

## Скоординировать несколько сущностей через Service

Назначение: атомарно объявить stateless process, который вызывают Commands и который может координировать несколько domain objects.

```lua
services.name = {
    method = function(...)
        -- reusable process
    end,
}

local result = services.name.method(...)
local external = services["other:service.name"]
```

Сокращённый production-пример из `GameData/rh/scripts/authoring/gameplay.lua`:

```lua
services.trade = {
    buy = function(buyer, seller, item)
        buyer:require_gold(item.price)
        seller:require_item(item)
        buyer:spend_gold(item.price)
        seller:add_gold(item.price)
        buyer:receive_item(seller:take_item(item))
        emit("trade.completed", { buyer = buyer, seller = seller, item = item })
    end,
}

commands.buy = function(item)
    services.trade.buy(player, actor("npc.merchant"), item)
end
```

Объявляйте Service целой таблицей; все поля обязаны быть functions. Service наследует scope вызывающей Command/Validator, не хранит state и immutable после registration.

Типичные ошибки: `InvalidServiceName`, `InvalidServiceImplementation`, `ServiceFieldNotFunction`, `ServiceDuplicateDeclaration`, `ServiceDeclarationAfterFreeze`, `UnknownServiceKey`, `ServiceTargetMissing`, `ServiceImmutableAfterRegistration`.

## Опубликовать и обработать Event

Назначение: сообщить post-commit gameplay fact и отреагировать на него без прямой связи producer/consumer.

```lua
emit(event_name_or_id, payload_optional)
on(event_name_or_id, function(payload, envelope)
    -- post-commit reaction
end)
```

Пример по `GameData/rh` и `Tests/Lua/authoring/events_and_presentation.lua`:

```lua
on("trade.completed", function(payload, envelope)
    emit("inventory.changed", {
        owner = payload.buyer, -- fresh rehydrated wrapper
        source_event_id = envelope.event_id,
    })
end)

commands.trade = function(buyer, seller, item)
    -- mutation уже выполнена
    emit("trade.completed", { buyer = buyer, seller = seller, item = item })
end
```

Короткое имя становится `<package>:event.<name>`. Payload обязан быть portable; wrappers автоматически превращаются в tagged references и восстанавливаются для subscriber. Event — только факт после успешной mutation. Subscriber работает с закрытым mutation window: прямое изменение state и `fail()` запрещены; следующую Command ставьте через `:later()`.

## Быстрая самопроверка

- Gameplay mutation начинается в Command и проходит через domain method/Service.
- Ожидаемый отказ использует `fail()` до первой записи; bug использует `error()`.
- Validator только читает.
- Definition immutable, wrappers не сохраняются в canonical state.
- Event описывает уже совершившийся факт и несёт portable payload.
- Все опубликованные ID соответствуют `<namespace>:<kind>.<path>`.
