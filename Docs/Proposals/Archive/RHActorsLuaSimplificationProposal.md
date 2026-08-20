---
title: RH Actors Lua Simplification Proposal
status: archived
proposal_state: implemented
version: 1.2
updated: 2026-08-19
depends_on:
  - EntityAuthoringExtensionProposal.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0031-entity-authoring-extensions.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# RH Actors Lua Simplification Proposal

> **Предлагает:** декларативные контракты полей сущностей (`field.*`), разделение структурных инвариантов и геймплейных предусловий, обобщённое создание инстансов и устранение инфраструктурного кода из `rh/scripts/gameplay/actors.lua`.
> **Затрагивает:** [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md), [Gameplay Model](../../Concepts/GameplayModel.md).
> **Состояние:** реализовано планом [RHActorsSimplification](../../Plans/Archive/RHActorsSimplification.md); нормативное поведение — [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md) и [ADR-0032](../../ADR/0032-field-contracts-and-generic-instance-creation.md).

## Context

После внедрения Entity Authoring Extensions файл:

```text
GameData/rh/scripts/gameplay/actors.lua
```

уже больше не содержит старую механику `actor_decorator`, `register_type()` и ручную композицию metatable.

Однако текущая версия всё ещё содержит значительный объём инфраструктурного и defensive-кода, который не относится непосредственно к доменной логике RH.

Актуальный файл по-прежнему:

- описывает `gold` и `stamina` через многословную таблицу `RESOURCES`;
- вручную валидирует каждый `amount`;
- содержит специальный fallback `self -> player`;
- повторяет проверки результата перед каждой mutation;
- напрямую импортирует `core:module.runtime.instance_allocator`;
- напрямую знает физическую структуру `game.state.item_instances`;
- вручную преобразует item definition handle в raw ID.

Цель proposal — довести `actors.lua` до того же уровня authoring abstraction, что уже достигнут для `commands` и Entity Extensions.

---

# 1. Goals

## G1. `actors.lua` должен содержать только RH gameplay semantics

При чтении файла должно быть видно:

```text
Actor имеет gold
Actor имеет stamina
Actor может получить item

Actor умеет:
  get/require/spend/add gold
  get/require/spend/add stamina
  add item
```

В файле не должно требоваться знание:

```text
runtime registries
instance_allocator
physical game.state collections
metatables
wrapper construction
runtime module lifecycle
```

## G2. State invariants объявляются рядом с полями

Вместо повторяющейся проверки типа/диапазона при каждой mutation ожидаемый контракт поля объявляется один раз:

```lua
Actor.gold = field.non_negative_integer()
Actor.stamina = field.non_negative_integer()
```

После этого authoring/runtime гарантирует этот invariant при каждой записи.

## G3. Gameplay preconditions остаются доменной логикой RH

Проверка:

```text
может ли Actor потратить N gold?
```

не является проверкой типа поля.

Она остаётся методом RH:

```lua
Actor:require_gold(amount)
```

и возвращает нормальный gameplay refusal через `fail()`.

## G4. Не вводить `Resource` в TextSystem

`gold`, `stamina`, `mana`, `morale` и т. п. не являются общей семантикой текстовой игры.

Даже если несколько полей внутри RH имеют одинаковые операции:

```text
get
require
spend
add
```

это не является основанием добавлять `Resource` abstraction в TextSystem.

## G5. `Item` и inventory остаются RH

TextSystem не должен знать:

```text
Item
Inventory
Actor:add_item()
```

Общий слой предоставляет только generic instance creation machinery.

Семантика item и структура item instance принадлежат RH.

---

# 2. Non-goals

Proposal не вводит:

- `Resource` как сущность TextSystem;
- universal economy system;
- Item или Inventory в TextSystem;
- автоматические `get/add/spend/require` методы для любого поля;
- новый ECS/component model;
- gameplay validators для всех аргументов методов;
- скрытую генерацию RH domain methods из field declarations.

Field declaration описывает state contract.

Gameplay methods остаются явным RH-кодом.

---

# 3. Current problems

## 3.1 `RESOURCES` descriptor содержит лишние производные данные

Текущий код хранит для каждого ресурса примерно следующее:

```lua
gold = {
    field = "gold",
    invalid_amount_error = "InvalidGoldAmount",
    default_fail_key = "economy.insufficient_gold",
    current_param = "current_gold",
    required_param = "required_gold",
}
```

Большая часть значений либо выводится из имени ресурса, либо существует только для обслуживания generic loop.

При двух ресурсах это делает код менее прозрачным, чем два небольших явных domain blocks.

## 3.2 `validate_amount()` повторяется концептуально в каждом method path

Сейчас каждый:

```text
require_gold
spend_gold
add_gold
require_stamina
spend_stamina
add_stamina
```

вручную вызывает одну и ту же проверку.

Проверка конечного canonical state должна выполняться field contract автоматически.

## 3.3 Fallback `self -> player` должен быть удалён

Текущий `get_*()` допускает отсутствие `self` и в таком случае пытается получить player через runtime API.

Это создаёт неожиданную семантику:

```lua
Actor.get_gold()
```

может неявно означать:

```text
get player gold
```

После внедрения Entity Authoring Extensions нормальный вызов:

```lua
player:get_gold()
```

уже всегда передаёт корректный Actor handle как `self`.

Entity method должен работать с получателем, которому он вызван.

## 3.4 `add_item()` содержит runtime leakage

RH сейчас напрямую знает:

```lua
require("core:module.runtime.instance_allocator")
game.state.item_instances
instance_id
definition_id
owner_id
```

Это инфраструктурные детали.

Gameplay-код должен описывать создание item instance, а не способ его физического размещения в canonical state.

---

# 4. Field contracts

## 4.1 Proposed syntax

Authoring layer должен поддерживать декларации полей:

```lua
Actor.gold = field.non_negative_integer()
Actor.stamina = field.non_negative_integer()
```

Field descriptor задаёт только ожидаемый контракт значения.

Он не должен автоматически означать:

```text
resource
currency
energy
spendable
regenerating
```

Это только state field с определённым value constraint.

## 4.2 `non_negative_integer`

Семантика:

```text
type(value) == number
math.type(value) == "integer"
value >= 0
```

Допустимо:

```lua
self.gold = 0
self.gold = 10
self.gold = 1000
```

Недопустимо:

```lua
self.gold = -1
self.gold = 1.5
self.gold = "10"
```

Нарушение является structural/runtime error, а не gameplay refusal.

## 4.3 Validation happens on write

Field contract должен проверяться при любой записи:

```lua
self.gold = new_value
```

независимо от того, откуда произошла mutation:

```text
Actor method
Gameplay Service
Command Handler
future mod extension
```

Таким образом invariant нельзя случайно обойти отдельным code path.

## 4.3.1 Схема сущности является композицией

Контракты полей приходят из трёх источников: generic entity kind (`Actor`), `definition_id` и `discriminator`. Эффективная схема собирается слиянием **по имени поля**, где более конкретный источник переопределяет одноимённое поле и не скрывает остальные.

Альтернатива «первый найденный источник побеждает целиком» неприемлема: она делает объявление `Actor.gold` молча недействующим для любого актора, имеющего собственную схему по `discriminator`, — то есть отключает защиту инварианта именно у главного персонажа.

Переопределение поля может сузить ограничения; смена `kind` поля является ошибкой.

Композиция вычисляется однократно при заморозке реестров, поэтому запись получает плоскую таблицу полей.

## 4.3.2 Повторное объявление поля запрещено

Объявление поля с уже занятым именем для той же сущности отклоняется ошибкой `FieldAlreadyDeclared`. Осознанное переопределение выражается явно:

```lua
Actor.gold = field.non_negative_integer({ override = true })
```

Это та же форма, что уже выбрана для замещения Lua-модулей: запечатано по умолчанию, замещаемо по явному флагу.

## 4.3.3 Объявление поля не регистрирует вид актора

Наличие схемы generic entity kind не делает произвольный `discriminator` известным. Проверка `ActorTypeNotRegistered` остаётся привязанной к `discriminator`, иначе одно объявление `Actor.gold` превращает опечатку в дискриминаторе в молчаливое создание актора.

## 4.4 General reusable field types

Общий authoring слой может предоставлять reusable descriptors:

```lua
field.integer()
field.non_negative_integer()
field.positive_integer()
field.number()
field.string()
field.boolean()
field.enum(values)
field.ref_definition(kind)
field.ref_instance(kind)
```

Это generic value/state validation.

Это **не** gameplay-domain abstraction.

---

# 5. Field invariants vs gameplay preconditions

Эти два механизма должны оставаться раздельными.

## Field invariant

Отвечает:

> может ли canonical state содержать такое значение?

Пример:

```lua
Actor.gold = field.non_negative_integer()
```

## Gameplay precondition

Отвечает:

> можно ли выполнить конкретное gameplay действие сейчас?

Пример:

```lua
function Actor:require_gold(amount)
    if self:get_gold() < amount then
        fail("economy.insufficient_gold", {
            current_gold = self:get_gold(),
            required_gold = amount,
        })
    end
end
```

Обычный flow:

```text
Command
  ↓
player:require_gold(price)
  ↓
если недостаточно → fail() до mutation
  ↓
player:spend_gold(price)
  ↓
field.non_negative_integer validates final state
```

`require_gold()` нужен не для защиты структуры state, а для нормальной gameplay-ошибки.

---

# 6. Amount argument validation

Proposal не требует писать:

```lua
expect.non_negative_integer(amount)
```

в каждом `add_*`, `spend_*` и `require_*`.

Это снова создало бы repetitive authoring boilerplate.

Field invariant гарантирует корректность **результирующего canonical state**.

Например:

```lua
self.gold = self:get_gold() - amount
```

не сможет записать `gold < 0`.

При этом знак самого `amount` является отдельным contract метода.

Например:

```lua
player:spend_gold(-10)
```

математически увеличит значение и не нарушит field invariant.

Такой вызов следует считать programmer error RH-кода, а не обязанностью generic field system.

Если RH хочет дополнительно защищать contract аргумента, это может быть:

- локальный `assert`;
- локальный helper;
- тесты;
- будущий общий typed-method-arguments mechanism.

Но proposal **не вводит Resource abstraction только ради этой проверки**.

## 6.1 Куда переносится проверка аргумента

`validate_amount()` удаляется, и его нужно чем-то заменить, иначе защита просто исчезает.

Проверка аргумента принадлежит границе, через которую в систему приходят недоверенные данные, — валидаторам команд. Контракт поля защищает результирующее состояние и по построению не может поймать `spend_gold(-10)`: такая запись увеличивает баланс и инварианта не нарушает.

До появления валидаторов команд пакет защищает собственные методы локальной проверкой в `rh`. Общий механизм типизированных аргументов методов остаётся возможным развитием и в этот proposal не входит.

---

# 7. Gold and stamina remain explicit RH semantics

На текущем размере игры предпочтителен явный domain code.

Не требуется делать:

```lua
define_resource("gold")
define_resource("stamina")
```

только ради устранения нескольких похожих строк.

Целевой стиль:

```lua
Actor.gold = field.non_negative_integer()
Actor.stamina = field.non_negative_integer()

function Actor:get_gold()
    ...
end

function Actor:require_gold(amount)
    ...
end

function Actor:spend_gold(amount)
    ...
end

function Actor:add_gold(amount)
    ...
end

function Actor:get_stamina()
    ...
end

...
```

Если в RH позже появится много полей с действительно одинаковой семантикой:

```text
gold
stamina
mana
morale
influence
rage
```

RH может самостоятельно создать локальную abstraction:

```lua
local function define_resource(...)
    ...
end
```

Она остаётся:

```text
GameData/rh/...
```

и не поднимается в TextSystem автоматически.

---

# 8. Item ownership

## 8.1 Item remains RH-specific

TextSystem не должен вводить:

```lua
Item
Inventory
Actor:add_item()
```

ради RH.

RH самостоятельно определяет:

- что является item;
- какие поля имеет item instance;
- кто может владеть item;
- что означает inventory;
- какие gameplay rules применяются при добавлении/удалении item.

## 8.2 Generic instance creation belongs to infrastructure

Общий authoring/runtime слой должен скрыть:

```text
instance ID allocation
canonical collection selection
raw state insertion
handle/reference normalization
```

Gameplay-код RH не должен импортировать:

```lua
core:module.runtime.instance_allocator
```

и не должен напрямую писать:

```lua
game.state.item_instances[item_id] = ...
```

## 8.3 Possible generic authoring API

Конкретный синтаксис может быть выбран отдельно.

Например:

```lua
instances.create("item", {
    definition = item_def_or_id,
    owner = self,
})
```

или:

```lua
instance.create("item", {
    definition = item_def_or_id,
    owner = self,
})
```

Важно не точное имя API, а его контракт:

- instance kind `"item"` определяется RH;
- **ядро не перечисляет виды экземпляров**: вид объявляется пакетом на фазе `register`, а имя секции состояния выводится из имени вида (`item` → `item_instances`), поэтому ядро знает правило именования, но не знает ни одного имени вида;
- `actor` остаётся осознанным исключением: он существует до загрузки любого пакета и имеет собственный реестр;
- переданные authoring handles автоматически canonicalize-ятся;
- allocator скрыт;
- physical state collection скрыта;
- возвращается stable instance ID или typed handle согласно общему instance contract.

---

# 9. Target `actors.lua`

После внедрения field contracts и generic instance creation целевой файл может выглядеть примерно так:

```lua
-- Actor domain extensions for RH.

Actor.gold = field.non_negative_integer()
Actor.stamina = field.non_negative_integer()


-- Gold

function Actor:get_gold()
    return self.gold or 0
end

function Actor:require_gold(amount, fail_key)
    local current = self:get_gold()

    if current < amount then
        fail(fail_key or "economy.insufficient_gold", {
            current_gold = current,
            required_gold = amount,
        })
    end
end

function Actor:spend_gold(amount)
    self.gold = self:get_gold() - amount
end

function Actor:add_gold(amount)
    self.gold = self:get_gold() + amount
end


-- Stamina

function Actor:get_stamina()
    return self.stamina or 0
end

function Actor:require_stamina(amount, fail_key)
    local current = self:get_stamina()

    if current < amount then
        fail(fail_key or "actor.insufficient_stamina", {
            current_stamina = current,
            required_stamina = amount,
        })
    end
end

function Actor:spend_stamina(amount)
    self.stamina = self:get_stamina() - amount
end

function Actor:add_stamina(amount)
    self.stamina = self:get_stamina() + amount
end


-- Inventory

function Actor:add_item(item_def_or_id)
    return instances.create("item", {
        definition = item_def_or_id,
        owner = self,
    })
end
```

Точный error key для stamina остаётся решением RH.

Точный синтаксис `instances.create()` также может быть выбран отдельно.

---

# 10. Why `spend_*` no longer needs duplicated underflow checks

Текущий код делает:

```lua
if current < amount then
    error("PreconditionNotChecked: ...")
end

self.gold = current - amount
```

После field contract структурная защита уже централизована:

```lua
Actor.gold = field.non_negative_integer()
```

Если `spend_gold()` попытается создать:

```text
gold = -10
```

запись будет отвергнута независимо от метода.

Gameplay-код при нормальном пути всё равно должен делать:

```lua
player:require_gold(amount)
player:spend_gold(amount)
```

поэтому:

- `require_gold()` даёт понятный gameplay refusal;
- field invariant ловит ошибку программиста/нарушение контракта;
- `spend_gold()` не дублирует обе проверки.

Важно, чем именно является срабатывание контракта поля. Это **runtime fault, а не gameplay refusal**: команда прерывается, событийный контекст отбрасывается, ошибка поднимается наверх. Каноническое состояние при этом **не откатывается** — записи, сделанные до ошибки, остаются, и прогон после такого отказа не является доверенным.

Поэтому контракт поля является средством обнаружения ошибки кода, а не средством сохранения целостности состояния. Транзакционность окна мутации — отдельное решение, вынесенное в самостоятельное предложение.

---

# 11. Authoring/runtime responsibility split

Итоговая граница:

```text
Core / generic authoring-runtime infrastructure
  entity extension machinery
  field descriptor machinery
  field validation on write
  generic instance creation
  stable instance allocation
  canonical state storage

TextSystem/common authoring surface
  reusable field/value contracts exposed to gameplay
  Actor / Location generic text-game semantics

RH
  Actor.gold
  Actor.stamina
  gold/stamina gameplay rules
  Item
  inventory
  Actor:add_item()
  RH-specific error keys
```

Ключевой принцип:

> Общий слой предоставляет механизмы и value contracts; RH определяет предметную семантику.

---

# 12. No `Resource` abstraction in TextSystem

Отдельно фиксируется:

```text
Resource
```

не вводится как сущность или authoring concept TextSystem.

Не должно появляться generic API вида:

```lua
Actor.gold = resource(...)
Actor.stamina = resource(...)
```

только потому, что текущая игра использует одинаковые методы.

Если повторение становится существенным, RH может локально написать:

```lua
local function define_resource(name, fail_key)
    ...
end
```

Это implementation detail RH.

Перенос вверх по слоям возможен только после появления независимых подтверждённых use cases, в которых `Resource` действительно является общей доменной концепцией.

---

# 13. No `Item` abstraction in TextSystem

Аналогично:

```text
Item
Inventory
```

не являются обязательной частью text-game architecture.

TextSystem не должен определять:

```lua
function Actor:add_item(...)
```

и не должен содержать item-specific schemas.

Generic instance API может использоваться RH для Item, а другой игрой — для совершенно других instance kinds.

---

# 14. Tests

## Field contract

```lua
actor.gold = 10
```

успешно.

```lua
actor.gold = 0
```

успешно.

```lua
actor.gold = -1
```

ошибка.

```lua
actor.gold = 1.5
```

ошибка.

```lua
actor.gold = "10"
```

ошибка.

## Gameplay refusal

При:

```text
gold = 5
```

вызов:

```lua
actor:require_gold(10)
```

должен вернуть expected gameplay refusal без mutation.

## Valid spend

```lua
actor:require_gold(5)
actor:spend_gold(5)
```

при `gold = 10` даёт `gold = 5`.

## Structural guard

Прямая или ошибочная mutation, приводящая к:

```text
gold < 0
```

должна быть отвергнута field validator независимо от code path.

## Entity receiver

```lua
actor:get_gold()
```

использует только переданный `self`.

`Actor.get_gold()` без receiver не должен неявно читать player.

## Item creation

`Actor:add_item()`:

- не импортирует runtime allocator;
- не пишет напрямую в `game.state`;
- создаёт корректный RH item instance через generic instance API;
- связывает owner с Actor;
- работает одинаково в UE и Headless.

---

# 15. Migration steps

1. Добавить generic field descriptor API.
2. Добавить `field.non_negative_integer()`.
3. Подключить field validation к Actor/entity state writes.
4. Объявить в RH:

   ```lua
   Actor.gold = field.non_negative_integer()
   Actor.stamina = field.non_negative_integer()
   ```

5. Удалить `RESOURCES`.
6. Удалить `validate_amount()`.
7. Удалить fallback `self -> player`.
8. Упростить `get/require/spend/add` до явных RH methods.
9. Добавить generic authoring API создания instance-backed сущностей.
10. Перевести `Actor:add_item()` на этот API.
11. Удалить из RH импорт `core:module.runtime.instance_allocator`.
12. Удалить прямую запись RH в `game.state.item_instances`.
13. Добавить tests field invariants, gameplay refusals и item creation.
14. Проверить identical behavior UE/Headless.

---

# 16. Acceptance criteria

Proposal считается реализованным, когда:

1. `actors.lua` не импортирует `core:module.runtime.*`.
2. `actors.lua` не обращается напрямую к `game.state`.
3. `Actor.gold` и `Actor.stamina` имеют явно объявленные field contracts.
4. Недопустимое значение поля отклоняется централизованно при записи.
5. `require_gold()` / `require_stamina()` остаются gameplay preconditions.
6. `spend_*()` не дублируют structural underflow validation.
7. `get_*()` не содержит fallback на global player.
8. `RESOURCES` descriptor и `validate_amount()` удалены.
9. TextSystem не получает `Resource` abstraction.
10. TextSystem не получает Item/Inventory semantics.
11. Generic instance creation скрывает allocator и physical state storage.
12. RH самостоятельно владеет item instance semantics.
13. Gameplay behavior существующих RH scenarios остаётся эквивалентным.
14. UE и Headless проходят одинаковые scenarios.
15. Поле, объявленное на generic entity kind, действует и для актора, имеющего схему по `discriminator`.
16. Повторное объявление поля без `override` отклоняется.
17. Неизвестный `discriminator` по-прежнему отклоняется независимо от объявленных полей.
18. Ядро не содержит ни одного имени вида экземпляра; неизвестный вид — типизированный отказ.

---

# 17. Result

До:

```text
RH actors.lua
  resource descriptors
  amount validators
  generated methods
  player fallback
  runtime allocator import
  raw game.state writes
  RH domain semantics
```

После:

```text
RH actors.lua
  field declarations
  RH domain methods
  RH item semantics
```

А инфраструктура:

```text
field validation
instance allocation
canonical storage
entity wrapping
```

остаётся за authoring/runtime boundary.

Главный принцип proposal:

> **`actors.lua` должен описывать, что умеет Actor в RH, а не как runtime технически хранит и защищает Actor state.**
