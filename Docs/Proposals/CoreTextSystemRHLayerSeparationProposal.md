# Proposal: Core / TextSystem / RH Layer Separation and Final RH Cleanup

```yaml
title: Core / TextSystem / RH Layer Separation and Final RH Cleanup
status: draft
proposal_state: proposed
updated: 2026-08-18

depends_on:
  - DesignerLuaAuthoringProposal
  - SimplifiedDesignerAuthoringAndRHCleanupProposal
  - LuaRuntimeContract
  - CommandsAndEvents
  - CanonicalStateAndSave
  - StableIDSpecification
  - ContentEditorPluginProposal
```

## 1. Назначение

По мере упрощения `rh` стало видно, что текущего разделения на `Core` и конкретную игру недостаточно.

Часть функциональности, которую ранее предполагалось переносить в `Core`, не является универсальной для любой игры. Она универсальна только для класса текстовых игр с:

- персонажами;
- локациями;
- переходами;
- квестами;
- screen-based presentation;
- событиями, связанными с игровой логикой текстовой игры.

Если оставить такие сущности в `Core`, Core перестаёт быть универсальным framework и начинает включать предположения о конкретном жанре.

Если оставить их в `RH`, каждая следующая текстовая игра будет вынуждена повторно реализовывать одни и те же механизмы.

Поэтому архитектура разделяется на три уровня:

```text
Core
  ↑
TextSystem
  ↑
RH
```

Главный принцип:

> **Core предоставляет универсальный game runtime и UE integration. TextSystem предоставляет reusable основу текстовой игры. RH содержит только правила и сущности конкретной игры.**

---

# Part A. Общая архитектура

## 2. Зависимости

Зависимости направлены строго сверху вниз:

```text
RH
    depends on
TextSystem
    depends on
Core
```

Запрещены зависимости:

```text
Core → TextSystem
Core → RH
TextSystem → RH
```

Core не знает о существовании TextSystem и RH.

TextSystem не знает о конкретной игре RH.

---

## 3. Три уровня ответственности

### Core

Универсальный framework, пригодный для любой игры.

### TextSystem

Genre/framework layer для текстовых и screen-driven игр.

### RH

Конкретная игра, использующая и расширяющая TextSystem.

---

# Part B. Core

## 4. Назначение Core

Core предоставляет runtime, infrastructure и presentation primitives, которые не зависят от жанра игры.

Core не должен знать о:

```text
Actor
Location
Quest
Travel
Inventory
Gold
Stamina
Dialog
Race
Faction
```

Любая такая сущность означает протекание game/genre semantics в generic framework.

---

## 5. Core Runtime

В Core остаются:

```text
Lua runtime
VM lifecycle
Package/module loading
GameDataRepository
Stable IDs
Canonical state
Save/load
Commands
Services
EventBus
Module lifecycle
Determinism
Diagnostics
Localization infrastructure
Authoring infrastructure
```

---

## 6. EventBus

EventBus остаётся в Core как универсальный механизм:

```text
publish
subscribe
unsubscribe
post-command facts
deferred handling
```

Core EventBus не знает значения события.

Например для него одинаковы:

```text
textsystem:location.entered
rh:quest.rewarded
somegame:spaceship.destroyed
```

Он обеспечивает только lifecycle, ordering, payload validation и subscription semantics.

---

## 7. Commands

Command infrastructure остаётся в Core.

Core знает:

```text
Command identity
registration
validation lifecycle
mutation window
run
later
failure semantics
```

Core не знает, что означает:

```text
travel
buy
work
attack
talk
```

---

## 8. Generic Presentation

Core предоставляет только presentation primitives.

Например:

```lua
show_screen {
    ...
}
```

и generic elements:

```text
button
text
image
list
panel
3D viewport
input
semantic action
```

Core отвечает за:

```text
Desired Presentation
Action Registry
UI semantic input
UE Bridge
Widget/Blueprint integration
presentation invalidation
presentation rebuild
```

---

## 9. Core не содержит Location Presenter

Ранее предлагалось перенести generic Location Presenter в Core.

Это решение отменяется.

Location является genre-level concept, поэтому:

```text
Location Presenter → TextSystem
```

Core только умеет отображать абстрактный Screen.

---

# Part C. TextSystem

## 10. Назначение TextSystem

TextSystem — reusable foundation для текстовых и screen-driven игр.

Он строится поверх Core и предоставляет общие genre abstractions.

Минимальный набор:

```text
Actor
Location
Quest
```

Дополнительные общие сущности могут добавляться позже только при наличии устойчивой межигровой semantics.

---

## 11. TextSystem не является конкретной игрой

TextSystem не должен содержать:

```text
gold
stamina
work
shop prices
RH NPCs
RH quests
RH factions
RH races
game-specific balance
```

TextSystem определяет форму и общие операции, но не правила конкретной игры.

---

# Part D. Actor в TextSystem

## 12. Base Actor

TextSystem определяет базовый Actor.

Концептуально Actor содержит минимум:

```text
definition
runtime instance identity
current_location
```

Дополнительные базовые поля вводятся только если они действительно обязательны для любой игры на TextSystem.

---

## 13. `current_location`

`current_location` принадлежит TextSystem Actor.

Canonical storage:

```text
current_location
```

Тип:

```text
ref_definition<location>
```

Designer-facing read:

```lua
actor.current_location
```

возвращает:

```text
DefinitionHandle<Location>
```

Не существует параллельного:

```text
current_location_id
```

---

## 14. `require_location()`

TextSystem предоставляет:

```lua
actor:require_location(location)
```

Операция:

- ничего не мутирует;
- проверяет `actor.current_location`;
- принимает Location handle/canonical reference;
- при mismatch создаёт typed refusal;
- не является Validator;
- не содержит game-specific policy.

Пример:

```lua
player:require_location(tavern)
```

---

## 15. `move_to()`

Базовый primitive называется:

```lua
actor:move_to(location)
```

а не:

```lua
actor:travel(location)
```

Причина:

`travel` описывает gameplay action, а `move_to` — generic state transition.

Это позволяет конкретной игре иметь:

```lua
commands.travel = function(target)
    ...
    player:move_to(target)
end
```

и отдельно:

```lua
commands.teleport = function(target)
    player:move_to(target)
end
```

без semantic конфликта.

---

## 16. Ответственность `move_to()`

TextSystem `move_to()` отвечает за:

- target Location validation;
- canonical mutation `current_location`;
- generic Actor/Location transition invariants;
- generic TextSystem transition facts/events;
- единый mutation path.

Он не проверяет:

```text
stamina
gold
time
quest state
class restrictions
weather
danger
game-specific access rules
```

---

## 17. Никакого fallback mutation

`move_to()` должен иметь один canonical implementation path.

Недопустимо:

```text
если service существует → использовать service
иначе → записать state напрямую
```

Если обязательный TextSystem service отсутствует, это programming/runtime invariant failure.

---

# Part E. Location в TextSystem

## 18. Base Location

TextSystem определяет Location definition.

Минимальная модель:

```text
screen
connected_locations
```

Фактические schema fields могут быть названы иначе, но ownership остаётся в TextSystem.

---

## 19. Connectivity

TextSystem предоставляет:

```lua
location:is_connected(target)
location:require_connected(target)
```

---

## 20. `is_connected()`

```lua
location:is_connected(target)
```

- читает topology definition;
- ничего не мутирует;
- принимает Location handle/canonical reference;
- возвращает boolean.

---

## 21. `require_connected()`

```lua
location:require_connected(target)
```

- ничего не мутирует;
- использует `is_connected()`;
- при failure создаёт generic TextSystem typed refusal;
- не задаёт цену перехода;
- не выполняет сам переход.

---

## 22. Error identity

Generic connectivity refusal принадлежит TextSystem.

Например:

```text
textsystem:error.location.not_connected
```

Generic wrong-location refusal:

```text
textsystem:error.actor.wrong_location
```

RH может создавать более специфичные ошибки только там, где есть RH-specific semantics.

---

# Part F. Quest в TextSystem

## 23. Base Quest

TextSystem определяет общую модель Quest.

Минимальный набор концепций:

```text
quest identity
state
start
complete
fail
objectives
```

Точный набор полей требует отдельного Quest contract, но ownership должен быть зафиксирован сразу.

---

## 24. Quest State

Концептуально:

```text
inactive
active
completed
failed
```

Если позже потребуется более сложная модель, она расширяется отдельным proposal.

---

## 25. Generic Quest operations

TextSystem может предоставлять:

```lua
quest:start()
quest:complete()
quest:fail()
```

и generic events:

```text
textsystem:quest.started
textsystem:quest.completed
textsystem:quest.failed
```

---

## 26. RH расширяет Quest

RH может добавлять:

```text
reward_gold
patron
faction_effect
special_conditions
RH-specific objective types
```

без изменения TextSystem base Quest.

---

# Part G. Расширение TextSystem сущностей из RH

## 27. Не использовать отдельное OO-наследование

Концептуальная модель:

```text
TextSystem.Actor
      ↓
RH Actor extensions
```

не требует отдельной Lua class hierarchy.

Предпочтительно использовать уже существующие механизмы:

```text
schema extensions
runtime decorators
definition decorators
managed operations
```

---

## 28. Один объект для Designer

Designer видит одну сущность:

```lua
player.current_location
player.gold
player.stamina

player:require_location(tavern)
player:require_gold(10)
player:move_to(tavern)
```

Ему не требуется знать, что:

```text
current_location / require_location / move_to
    пришли из TextSystem

gold / stamina
    пришли из RH
```

---

## 29. Расширение Location

TextSystem Location:

```text
connected_locations
screen
```

RH может добавить:

```text
danger
owner
faction
entry_fee
is_discovered
```

и RH-specific methods:

```lua
location:is_safe_for(actor)
location:require_discovered()
```

если они действительно понадобятся.

---

## 30. Новые RH сущности

RH может вводить собственные kinds, которых нет в TextSystem.

Например:

```text
School
Race
Horse
Faction
Arena
```

TextSystem не должен изменяться для поддержки каждой новой game-specific сущности.

---

# Part H. RH

## 31. Назначение RH

RH содержит только:

```text
конкретные gameplay rules
конкретные definitions
конкретные quests
game-specific state
game-specific domain extensions
game-specific entities
game-specific screens/content
```

---

## 32. RH Actor extensions

Для текущего slice RH добавляет Actor:

```text
gold
stamina
```

и operations:

```lua
player:require_gold(amount)
player:spend_gold(amount)
player:add_gold(amount)

player:require_stamina(amount)
player:spend_stamina(amount)
player:add_stamina(amount)

player:add_item(item)
```

---

## 33. RH Travel Command

Travel является RH gameplay Command:

```lua
commands.travel = function(target)

    player.current_location:
        require_connected(target)

    player:require_stamina(5)

    player:spend_stamina(5)
    player:move_to(target)

end
```

Ownership:

```text
require_connected → TextSystem
require_stamina   → RH
spend_stamina     → RH
move_to           → TextSystem
```

---

## 34. Не использовать `core:command.location.travel`

Текущий cross-package override:

```lua
commands["core:command.location.travel"] = ...
```

удаляется.

Core не должен владеть gameplay travel Command.

TextSystem также не обязан владеть gameplay travel Command, потому что цена и eligibility перехода зависят от конкретной игры.

RH объявляет:

```text
rh:command.travel
```

---

## 35. Action и Command

TextSystem generic Location Presenter может создавать semantic action:

```text
textsystem:action.location.travel
```

В RH binding:

```text
textsystem:action.location.travel
    → rh:command.travel
```

Action означает пользовательское намерение.

Command означает game-specific обработку этого намерения.

---

# Part I. Location Presentation

## 36. Location Presenter принадлежит TextSystem

TextSystem знает, что такое Location и Location Screen.

Поэтому generic Location Presenter находится в TextSystem, а не в Core и не в RH.

---

## 37. TextSystem Location Presenter

Conceptual flow:

```text
player.current_location
    ↓
Location definition
    ↓
screen definition
    ↓
description
    ↓
static actions
    ↓
connected locations
    ↓
semantic travel actions
    ↓
Core presentation primitives
    ↓
Desired Presentation
```

---

## 38. Core responsibility

TextSystem presenter вызывает только generic Core presentation API.

Например:

```lua
show_screen {
    ...
}
```

Core не знает, что отображаемый screen является Location.

---

## 39. Declarative screens

Статические screen data хранятся в definitions.

Например:

```json5
{
    id: "rh:screen.location.market",

    data: {
        title_text_id:
            "rh:text.location.market.title",

        description_text_id:
            "rh:text.screen.market.description",

        actions: [
            {
                key: "buy_sword",
                text_id: "rh:text.action.buy_sword",
                action_id: "rh:action.shop.buy",
                args: {
                    item: "rh:item.weapon.iron_sword",
                },
            },
            {
                key: "buy_armor",
                text_id: "rh:text.action.buy_armor",
                action_id: "rh:action.shop.buy",
                args: {
                    item: "rh:item.armor.leather_armor",
                },
            },
        ],

        connected_location_actions: true,
    },
}
```

---

## 40. Connected-location actions

Если:

```text
connected_location_actions = true
```

TextSystem presenter:

1. читает connected locations;
2. создаёт semantic travel action для каждого target;
3. передаёт action в Core presentation;
4. не знает RH stamina rules.

---

## 41. Удалить RH `location_screen.lua`

После появления TextSystem Location Presenter:

```text
rh/scripts/presentation/location_screen.lua
```

удаляется полностью.

RH vertical slice не содержит generic presentation builder.

---

# Part J. Module Autodiscovery

## 42. Удалить ручной `manifest.lua`

Ручной:

```text
scripts/manifest.lua
```

не должен быть source-of-truth.

---

## 43. Automatic discovery

Каждый package автоматически сканирует:

```text
scripts/authoring/
scripts/gameplay/
scripts/runtime/
scripts/presentation/
```

и строит deterministic generated module graph.

---

## 44. Generated IDs

Например:

```text
textsystem/scripts/gameplay/actors.lua
```

→

```text
textsystem:module.gameplay.actors
```

```text
rh/scripts/gameplay/actors.lua
```

→

```text
rh:module.gameplay.actors
```

---

## 45. Dependencies

Literal:

```lua
require("textsystem:module.gameplay.actors")
```

может автоматически входить в generated dependency graph.

Dynamic imports требуют explicit programmer metadata либо запрещаются для autodiscovered modules.

---

## 46. Generated manifest

Deterministic manifest сохраняется как build artifact.

Удаляется только:

```text
hand-authored manifest.lua
```

---

# Part K. Переработка текущего RH `actors.lua`

## 47. Удалить TextSystem responsibilities

Из RH `actors.lua` должны уйти:

```text
current_location
current_location_id
require_location
travel / move_to

Location definition decorator
is_connected
require_connected

Location reference field registration
```

Это переносится в TextSystem.

---

## 48. RH-specific responsibilities

После переноса в RH `actors.lua` остаются:

```text
Gold:
    require_gold
    spend_gold
    add_gold

Stamina:
    require_stamina
    spend_stamina
    add_stamina

Inventory:
    add_item
```

Плюс RH-specific registration.

---

## 49. Удалить unused getters

Если repository-wide usage check подтверждает отсутствие public dependency, удалить:

```lua
get_gold()
get_stamina()
```

Designer использует:

```lua
player.gold
player.stamina
```

---

## 50. Удалить `is_player()` / `is_npc()` при отсутствии использования

Если methods:

```lua
is_player()
is_npc()
```

не являются опубликованным mod API и нигде не используются, удалить их.

Discriminator остаётся доступным programmer-level способом.

---

## 51. Общая amount validation

Повторяющаяся проверка amount должна существовать один раз:

```lua
local function require_amount(amount, name)

    if type(amount) ~= "number"
        or math.type(amount) ~= "integer"
        or amount < 0
    then
        error(
            "Invalid" .. name ..
            "Amount: expected non-negative integer",
            3
        )
    end

end
```

---

## 52. Resource descriptors

```lua
local GOLD = {
    field = "gold",
    error = "economy.insufficient_gold",
    current_param = "current_gold",
    required_param = "required_gold",
}

local STAMINA = {
    field = "stamina",
    error = "economy.insufficient_stamina",
    current_param = "current_stamina",
    required_param = "required_stamina",
}
```

---

## 53. Local resource helpers

```lua
local function require_resource(
    base,
    spec,
    amount,
    opt_error
)
    ...
end

local function spend_resource(
    base,
    spec,
    amount
)
    ...
end

local function add_resource(
    base,
    spec,
    amount
)
    ...
end
```

Public methods остаются простыми:

```lua
function wrapper:require_gold(amount, key)
    return require_resource(
        base,
        GOLD,
        amount,
        key
    )
end

function wrapper:spend_gold(amount)
    return spend_resource(
        base,
        GOLD,
        amount
    )
end

function wrapper:add_gold(amount)
    return add_resource(
        base,
        GOLD,
        amount
    )
end
```

Аналогично для stamina.

---

## 54. Не создавать generic ResourceSystem

Gold и stamina остаются RH concepts.

TextSystem Actor не обязан иметь ни gold, ни stamina.

Не вводится:

```text
TextSystem Resource
Core Resource
Generic Stat System
```

до появления реального reuse между независимыми играми.

---

## 55. `add_item()`

`player:add_item(item)` пока остаётся RH primitive.

Inventory не переносится автоматически в TextSystem только потому, что в RH существует item.

---

## 56. Когда Inventory переносить в TextSystem

Перенос допустим только после появления устойчивой общей semantics, например:

```text
ownership
remove item
transfer item
containers
stacking
equipment
```

и подтверждения, что она нужна нескольким играм на TextSystem.

До этого Inventory остаётся RH-level domain logic.

---

# Part L. TextSystem Package Structure

## 57. Предлагаемая структура

Концептуально:

```text
textsystem/
├── definitions/
├── schemas/
├── localization/
├── package.json5
│
└── scripts/
    ├── gameplay/
    │   ├── actors.lua
    │   ├── locations.lua
    │   └── quests.lua
    │
    └── presentation/
        └── location_presenter.lua
```

Фактическое разбиение по файлам может отличаться.

Главное — ownership.

---

## 58. TextSystem не должен содержать RH data

В TextSystem отсутствуют:

```text
RH market
RH tavern
RH sword
RH stamina cost
RH work rule
RH quest content
```

Он содержит только reusable schemas, methods и presenters.

---

# Part M. RH Package Structure

## 59. Целевая структура RH

```text
rh/
├── definitions/
│   ├── actors.json5
│   ├── items.json5
│   ├── locations.json5
│   ├── screens.json5
│   ├── quests.json5
│   ├── resources.json5
│   └── texts.json5
│
├── localization/
│   └── ru.po
│
├── schemas/
│   └── ...
│
├── package.json5
│
└── scripts/
    ├── authoring/
    │   └── gameplay.lua
    │
    └── gameplay/
        └── actors.lua
```

Отсутствуют:

```text
manifest.lua
presentation/location_screen.lua
economy.lua
root.lua
```

---

# Part N. Целевой RH gameplay

## 60. `authoring/gameplay.lua`

```lua
local tavern = location("city.tavern")
local market = location("city.market")


commands.travel = function(target)

    player.current_location:
        require_connected(target)

    player:require_stamina(5)

    player:spend_stamina(5)
    player:move_to(target)

end


commands["time.wait_day"] = function()

    player:require_location(tavern)

    player:add_stamina(10)

end


commands["work.do_work"] = function()

    player:require_location(tavern)
    player:require_stamina(6)

    player:spend_stamina(2)
    player:add_gold(10)

end


commands.buy = function(item)

    player:require_location(market)
    player:require_gold(item.price)

    player:spend_gold(item.price)
    player:add_item(item)

end
```

---

## 61. Designer не видит layer boundary

Designer использует единый объект:

```lua
player.current_location
player.gold
player.stamina
```

и единый method surface:

```lua
player:require_location(...)
player:require_stamina(...)
player:move_to(...)
```

Layer ownership важен для architecture/programmer API, но не должен создавать отдельный syntax для designer.

---

# Part O. Package dependency

## 62. RH manifest/package metadata

На уровне package dependencies RH явно зависит от TextSystem.

Концептуально:

```json5
{
    id: "rh",
    dependencies: [
        "textsystem"
    ]
}
```

TextSystem зависит от Core.

---

## 63. Core package не зависит от TextSystem

Core build и tests должны успешно работать вообще без подключённого TextSystem.

Это важный acceptance criterion универсальности Core.

---

# Part P. Testing Strategy

## 64. Core tests

Core tests используют абстрактные fixtures без Actor/Location.

Проверяют:

```text
Lua runtime
Commands
EventBus
state/save
repository
Stable IDs
presentation primitives
UE bridge contracts
```

---

## 65. TextSystem tests

Отдельный минимальный sample package:

```text
sample_text_game
```

может содержать:

```text
Actor: player
Locations:
    room
    street
    shop
Quest:
    simple_test_quest
```

и проверять:

```text
current_location
move_to
require_location
connectivity
location presenter
generic quest lifecycle
save/load
generic events
```

без RH.

---

## 66. RH tests

RH tests проверяют только game-specific rules:

```text
travel stamina cost
work
wait
buy
RH quests
RH actor extensions
RH content bindings
```

---

# Part Q. Migration Plan

## 67. Stage 1 — Создать TextSystem package

Создать package:

```text
textsystem
```

с dependency на Core.

Пока без изменения RH behavior.

---

## 68. Stage 2 — Перенести Actor Location ownership

Из текущего Core/RH состояния перенести в TextSystem:

```text
Actor.current_location
Actor.require_location()
Actor.move_to()
```

Удалить transitional:

```text
current_location_id
```

---

## 69. Stage 3 — Перенести Location topology

В TextSystem:

```text
Location
connected locations
is_connected()
require_connected()
```

---

## 70. Stage 4 — RH travel

Удалить override:

```text
core:command.location.travel
```

Добавить:

```text
rh:command.travel
```

с RH stamina policy.

---

## 71. Stage 5 — Quest base

Перенести/создать generic Quest foundation в TextSystem до появления большого количества RH quest logic.

Именно сейчас это дешевле всего.

---

## 72. Stage 6 — Location Presenter

Перенести generic location presentation из RH в TextSystem.

Удалить:

```text
rh/scripts/presentation/location_screen.lua
```

---

## 73. Stage 7 — Declarative Screens

Расширить TextSystem screen schema / conventions:

```text
description
semantic actions
connected-location actions
```

RH screen definitions содержат только content/configuration.

---

## 74. Stage 8 — Module autodiscovery

Реализовать generated module graph для всех трёх layers.

Удалить hand-authored:

```text
manifest.lua
```

---

## 75. Stage 9 — RH `actors.lua` cleanup

После переноса Location responsibilities:

- удалить Location code;
- удалить location field registrations;
- удалить legacy fields;
- объединить amount validation;
- объединить gold/stamina helpers;
- удалить unused getters/helpers.

---

# Part R. Acceptance Criteria

## 76. Core

Core считается genre-agnostic, если в его public gameplay API отсутствуют concepts:

```text
Actor
Location
Quest
Travel
Inventory
Gold
Stamina
```

Core может отображать screen, но не знает, что такое Location Screen.

---

## 77. TextSystem

TextSystem считается самостоятельным reusable layer, если:

1. может работать с Core без RH;
2. предоставляет Actor;
3. предоставляет Location;
4. предоставляет Quest base;
5. поддерживает actor location;
6. поддерживает connectivity;
7. поддерживает `move_to`;
8. поддерживает Location Presenter;
9. не содержит RH-specific gameplay rules.

---

## 78. RH

RH считается корректно отделённым, если:

1. зависит от TextSystem;
2. не реализует base Location topology;
3. не реализует base actor movement;
4. не реализует generic Location Presenter;
5. содержит gold/stamina как собственные extensions;
6. содержит game-specific Commands;
7. может добавлять новые kinds независимо от TextSystem.

---

## 79. RH `actors.lua`

После cleanup:

1. нет `current_location`;
2. нет `current_location_id`;
3. нет `require_location`;
4. нет `move_to/travel`;
5. нет Location decorator;
6. нет `is_connected`;
7. нет `require_connected`;
8. gold/stamina используют общие локальные helpers;
9. нет duplicated amount validation;
10. `add_item()` остаётся RH-level до отдельного Inventory decision.

---

## 80. Presentation

RH не содержит:

```text
location_screen.lua
```

TextSystem Location Presenter строит generic location presentation через Core primitives.

---

## 81. Module System

Добавление нового Lua module не требует:

```text
manual manifest.lua
manual module list
root.lua
manual dependency duplication
```

---

# Part S. Что не входит

## 82. Не вводится

Proposal не требует:

- четвёртого architecture layer;
- отдельного DSL;
- classical OO inheritance;
- generic Stat System;
- generic Economy System;
- generic Inventory System;
- gameplay rollback;
- alternative EventBus;
- alternative Stable ID syntax;
- direct UI → Lua function calls;
- gameplay policy в TextSystem movement primitive.

---

# Part T. Главный архитектурный инвариант

## 83. Итоговая граница

```text
Core
    universal runtime/framework
    UE integration
    Commands/EventBus
    State/Save/Repository
    generic Presentation

TextSystem
    reusable text-game model
    Actor
    Location
    Quest
    movement/topology
    Location Presenter

RH
    concrete game
    gold/stamina
    work/buy/wait/travel policy
    RH quests
    RH entities
    RH content
```

---

## 84. Критерий выбора слоя

При добавлении новой функциональности задаётся вопрос:

### Вопрос 1

Нужна ли эта функциональность практически любой игре на GV2 независимо от жанра?

Если да:

```text
Core
```

### Вопрос 2

Нужна ли эта функциональность большинству текстовых игр с Actor/Location/Quest моделью?

Если да:

```text
TextSystem
```

### Вопрос 3

Относится ли она только к правилам и контенту конкретной игры?

Если да:

```text
RH
```

---

## 85. Пример

```text
EventBus
    → Core

show_screen()
    → Core

Actor
    → TextSystem

Location
    → TextSystem

require_connected()
    → TextSystem

move_to()
    → TextSystem

stamina
    → RH

travel costs 5 stamina
    → RH

Horse
    → RH

Race
    → RH
```

---

# 86. Критерий завершения

После этой переработки дальнейшее разделение не считается необходимым, пока не появится реальный набор reusable mechanics, не помещающийся ни в один из трёх уровней.

Главная цель достигнута, если:

> **Core ничего не знает о жанре. TextSystem ничего не знает о конкретной игре. RH не реализует повторно механику, общую для текстовых игр.**

И designer-facing gameplay остаётся кодом игровых правил, например:

```lua
commands.travel = function(target)

    player.current_location:
        require_connected(target)

    player:require_stamina(5)

    player:spend_stamina(5)
    player:move_to(target)

end
```

Сложность этого кода определяется правилами RH, а не устройством framework.
