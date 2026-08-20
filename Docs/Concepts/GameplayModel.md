---
title: Gameplay Model
status: informative
version: 1.3
updated: 2026-08-20
depends_on:
  - README.md
---

# Модель геймплея

> **Объясняет:** как действие игрока становится изменением состояния и возвращается на экран.
> **Нормативно:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md).
> **Не является нормативным:** при расхождении прав contract.

Объяснение того, как действие игрока превращается в изменение состояния и обратно в картинку. Нормативные правила каждого этапа — в contracts, на которые ссылается текст.

## Понятия

**Definition** — неизменяемое описание из репозитория: предмет, локация, актор, экран. Живёт в контенте, не в состоянии прохождения. Идентифицируется Stable ID вида `rh:item.weapon.iron_sword`.

**Runtime Instance** — конкретный экземпляр в текущем прохождении. Хранит `instance_id` и ссылку `definition_id`, но не копию самого definition. Один definition порождает сколько угодно экземпляров.

**Canonical State** — единственное сохраняемое состояние прохождения. Обычная Lua-таблица из простых значений: строки, числа, булевы, массивы, объекты. Ни функций, ни объектов, ни ссылок на движок.

**Actor** — персонаж: игрок и NPC используют одну модель. Хранится в `state.actors`.

**Runtime Wrapper** — одноразовый объект поверх таблицы состояния, дающий удобные методы. Не копия: пишет в ту же таблицу. Живёт до конца вызова и в состояние не попадает.

**Instance Registry** — точка получения обёртки по идентификатору (`game.instances.actors`, `game.instances.world`). Отвечает за identity и создание, но не за правила игры.

**Command** — намерение изменить состояние: «отправиться в таверну», «купить меч». Не запись в поле, а действие.

**Validator** — проверка «можно ли» до любого изменения. Только читает.

**Command Handler** — тонкая связка команды с логикой. Правил не содержит. Диспетчер (`Scripts/runtime/command_dispatcher.lua`) не знает имён команд: он ищет обработчик по `command_id` в реестре `game.commands.handlers` (`core:module.runtime.handler_registry`) и вызывает найденный; неизвестный `command_id` — типизированный отказ `core:error.command.unknown` без открытия mutation window. Ядро и каждый пакет регистрируют свои обработчики сами, без правок C++ и без перечисления в `Scripts/boundary/ingress.lua`.

**Gameplay Service** — работа, затрагивающая несколько сущностей: торговля, перемещение. Локальную операцию над одной сущностью делает метод обёртки.

**Gameplay Event** — факт, что что-то уже произошло. Публикуется только после успешного изменения и отменить его нельзя.

**Desired Presentation** — описание желаемого экрана: какой `screen_id` и какие значения полей. Не виджеты.

## Сквозной сценарий

```text
Игрок нажимает «Отправиться»
        ↓
UE превращает нажатие в semantic input
        ↓
Command  core:command.location.travel
        ↓
Validators           ← только читают; отказ останавливает всё
        ↓
Command Handler      ← открывается mutation window
        ↓
Gameplay Service / методы Actor и World
        ↓
Изменение canonical state
        ↓
Events  leave → enter ← после успешного commit, окно уже закрыто
        ↓
Подписчики реагируют; могут поставить новую команду в очередь
        ↓
Desired Presentation
        ↓
UE приводит экран к описанному состоянию
```

Точная семантика этапов: [Commands and Events](../Architecture/CommandsAndEvents.md) — конверт команды, порядок валидаторов, момент публикации фактов, фазы и очереди; [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md) — фасад `game`, права обработчиков, границы значений; [Semantic Input](../UI/SemanticInput.md) — как нажатие становится командой.

## Что из этого следует на практике

**Изменить состояние можно только внутри выполнения команды.** Не «только через сервисы», а именно в этом временном окне: вне его любая запись в `game.state` является нарушением и ловится тестом. Нормативная формулировка — [Commands and Events § Mutation authority](../Architecture/CommandsAndEvents.md).

**Команда описывает действие, а не запись в поле.** `buy_item` — да, `set_gold` — нет: второе раскрывает устройство состояния и превращает публичный API в протокол записи полей. Правило и примеры — там же.

**Событие нельзя использовать как вопрос.** Оно сообщает о свершившемся. Если нужно что-то разрешить или запретить — это валидатор.

**Обработчик события не меняет состояние.** Он может прочитать состояние, опубликовать ещё одно событие или поставить команду в очередь; сама команда выполнится позже и откроет собственное окно.

**Условие подписки проверяется в самом обработчике.** Подписка идёт по `event_id`; декларативных фильтров нет, и это осознанное решение, а не недоделка.

## Как это выглядит для автора правил игры

Всё выше — механика ядра. Автор игрового пакета (например, `rh`) пишет правила через упрощённый authoring-слой (`scripts/authoring/*.lua`, [ADR-0027](../ADR/0027-designer-lua-authoring-layer.md), [ADR-0028](../ADR/0028-simplified-authoring-surface.md)): свой лексический `_ENV` без префиксов `M.`, неявный успех команды (не нужно оборачивать результат в `{ ok = true }`) и глобальные `commands`/`actions` вместо низкоуровневых registry calls.

```lua
-- GameData/rh/scripts/authoring/gameplay.lua (сокращённо)
local tavern = location("city.tavern")

commands["rh:command.travel"] = function(target)
    player.current_location:require_connected(target)
    player:require_stamina(5, "travel.insufficient_stamina")
    player:spend_stamina(5)
    player:move_to(target)
end

actions["textsystem:action.location.travel"] = "rh:command.travel"
```

Authoring-скрипты компилируются в обычные обработчики `game.commands.handlers`/`game.events.subscribers` загрузчиком — геймплейный контракт (валидаторы, mutation window, события) не меняется, меняется только то, сколько текста автор набирает руками.

## Где посмотреть работающий пример

Перемещение между локациями реализовано на два слоя: базовая операция `move_to` и факты `leave`/`enter` — в `GameData/textsystem/scripts/gameplay/actors.lua` (переиспользуемая для любой текстовой игры часть), конкретные правила перехода (стоимость выносливости, где можно перемещаться) — в `GameData/rh/scripts/authoring/gameplay.lua` (authoring-слой конкретной игры). Проверки — `Tests/Lua/world/travel_command.lua`/`travel_events.lua` (на демо-пакете `sample`) и `Tests/Lua/economy/travel_stamina.lua` (на `rh`). Это самый короткий путь увидеть все этапы сразу.

## Дальше

- Откуда берутся definitions — [ContentModel](ContentModel.md).
- Чем экземпляр отличается от definition и как устроены обёртки — [RuntimeInstances](RuntimeInstances.md).
- Что происходит после desired presentation — [PresentationModel](PresentationModel.md).
- Добавить свою команду — [Guides/AddCommand](../Guides/AddCommand.md).
