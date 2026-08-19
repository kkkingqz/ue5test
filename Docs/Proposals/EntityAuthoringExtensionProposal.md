---
title: Entity Authoring Extension Proposal
status: draft
proposal_state: accepted_for_planning
version: 1.0
updated: 2026-08-19
depends_on:
  - TextSystemLayerProposal.md
  - SimplifiedAuthoringSurfaceProposal.md
  - ../Architecture/LuaRuntimeContract.md
decisions:
  - ../ADR/0028-simplified-authoring-surface.md
  - ../ADR/0030-textsystem-layer-and-data-driven-package-set.md
  - ../ADR/0031-entity-authoring-extensions.md
---

# Entity Authoring Extension Proposal

> **Предлагает:** декларативный механизм добавления методов к доменным сущностям через авторские прототипы `_ENV`.
> **Затрагивает:** [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md), [Modding](../Architecture/Modding.md).
> **Не является нормативным:** до реализации действует текущий contract.

## Summary

После реализации `CoreTextSystemRHLayerSeparationProposal` слои Core / TextSystem / RH в целом разделены правильно: общие сущности и их базовая семантика принадлежат соответствующему общему слою, а конкретная игра и моды могут добавлять к этим сущностям собственное доменное поведение.

Оставшаяся проблема находится не в распределении ответственности, а в **authoring syntax**.

Сейчас расширение сущности может требовать низкоуровневой runtime-механики:

- module descriptor;
- `register()`;
- registries;
- decorators;
- `setmetatable()`;
- wrapper composition;
- ручного знания lifecycle и внутреннего устройства entity handles.

Для игрового кода это лишняя инфраструктура.

По аналогии с упрощённым `gameplay.lua` предлагается ввести **универсальный authoring-механизм добавления методов к доменным сущностям**.

Целевой синтаксис:

```lua
function Actor:add_gold(amount)
    self.gold = (self.gold or 0) + amount
end

function Location:is_safe()
    return self.danger_level == 0
end

function Quest:is_completed()
    return self.state == "completed"
end
```

Без явных decorators, metatables, registries и ручной регистрации runtime-типа.

`Actor` является первым практическим кейсом миграции, но сам механизм не должен быть actor-specific.

---

# 1. Problem

## 1.1 Current architectural direction

После разделения слоёв реальная модель системы выглядит примерно так:

```text
Core
  generic runtime machinery

TextSystem
  generic text-game entities and behavior

RH
  RH-specific behavior

Mods
  additional behavior
```

Одна и та же сущность может последовательно получать поведение из нескольких слоёв.

Например:

```text
Core Actor
    ↓
TextSystem Actor methods
    ↓
RH Actor methods
    ↓
Mod Actor methods
```

Аналогично это может относиться к другим сущностям:

```text
Core/Repository entity
    ↓
TextSystem Location behavior
    ↓
RH Location behavior
    ↓
Mod Location behavior
```

Поэтому расширение сущностей является **общим механизмом композиции gameplay behavior**, а не особенностью Actor.

## 1.2 Current low-level style

Сейчас добавление поведения может требовать кода уровня:

```lua
local M = {
    id = "rh:module.gameplay.actors",
}

local function actor_decorator(base)
    return setmetatable({
        -- methods
    }, {
        __index = base,
        __newindex = base,
    })
end

function M.register(_ctx)
    game.instances.actors.register_type("player", actor_decorator)
    game.instances.actors.register_type("npc", actor_decorator)
end

return M
```

Проблема не в том, что RH добавляет методы к Actor.

Это корректно.

Проблема в том, что RH должен знать **как runtime технически реализует entity extension**.

## 1.3 Why this is a problem

### Boilerplate

Несколько простых методов требуют непропорционально большого количества инфраструктурного Lua-кода.

### Runtime leakage

Gameplay package начинает зависеть от деталей:

- registries;
- discriminator registration;
- wrapper/decorator construction;
- metatables;
- runtime module lifecycle.

### Wrong abstraction boundary

Автор игры должен описывать:

```lua
function Actor:add_gold(amount)
    ...
end
```

а не:

```lua
game.instances.actors.register_type(...)
```

### Poor layer composition

API вида:

```lua
register_type("player", decorator)
```

предполагает владение типом одним конкретным модулем.

Но новая архитектура требует композиции нескольких независимых слоёв.

### Inconsistency with gameplay authoring

Уже принятый стиль `gameplay.lua` выглядит как предметный код:

```lua
player:require_stamina(5)
player:spend_stamina(5)
player:move_to(target)
player:add_gold(10)
```

Объявление методов сущностей должно использовать тот же уровень абстракции.

---

# 2. Goals

## G1. Universal entity extensions

Любая поддерживаемая доменная сущность должна иметь возможность получать дополнительные Lua-методы через единый authoring mechanism.

Например:

```lua
Actor
Location
Quest
Item
Faction
```

Конкретный набор доступных entity prototypes определяется слоями и зарегистрированными entity kinds.

## G2. Domain-first syntax

Автор gameplay-кода пишет обычный Lua:

```lua
function Actor:add_gold(amount)
    ...
end
```

без runtime plumbing.

## G3. Preserve layer ownership

Наличие общего extension-механизма не означает перенос доменной логики вверх по слоям.

Например:

```text
TextSystem:
  Actor:is_player()
  Actor:is_npc()

RH:
  Actor:get_gold()
  Actor:add_gold()

SomeMod:
  Actor:get_corruption()
```

Каждый слой добавляет только принадлежащую ему семантику.

## G4. Layer composition

Core, TextSystem, game package и моды должны иметь возможность независимо расширять одну сущность.

## G5. Deterministic composition

Результат должен быть детерминирован и следовать уже определённому package/module load order.

## G6. No manual runtime-type registration

Добавление метода к сущности не должно требовать повторной регистрации `player`, `npc`, `location` или другого discriminator/type.

## G7. Keep machinery internal

Registries, metatables, decorators, wrapper creation и composition должны быть внутренней реализацией runtime/authoring layer.

## G8. Same semantics in UE and Headless

Entity extensions должны работать одинаково в UE-host и standalone Headless-host.

---

# 3. Non-goals

Proposal не вводит:

- новый gameplay state model;
- ECS;
- component system;
- новый inheritance hierarchy;
- обязательные typed subclasses;
- автоматическое добавление state fields;
- отдельную persistence model;
- перенос game-specific поведения в TextSystem;
- возможность monkey-patch arbitrary runtime tables.

Речь только о контролируемом authoring API для методов зарегистрированных доменных сущностей.

---

# 4. Proposed authoring model

## 4.1 Entity authoring prototypes

Authoring environment предоставляет специальные prototype objects для поддерживаемых entity kinds.

Например:

```lua
Actor
Location
Quest
Item
```

Эти объекты существуют только в authoring environment.

Они не являются конкретными gameplay instances.

Запись:

```lua
function Actor:add_gold(amount)
    ...
end
```

декларативно регистрирует метод `add_gold` как extension для entity kind `Actor`.

## 4.2 Example: Actor

```lua
function Actor:get_gold()
    return self.gold or 0
end

function Actor:add_gold(amount)
    self.gold = self:get_gold() + amount
end

function Actor:require_gold(amount, error_key)
    if self:get_gold() < amount then
        fail(error_key or "economy.insufficient_gold", {
            current_gold = self:get_gold(),
            required_gold = amount,
        })
    end
end

function Actor:spend_gold(amount)
    self.gold = self:get_gold() - amount
end
```

Usage:

```lua
player:add_gold(10)

local npc = actor("rh:npc.someone")
npc:add_gold(5)
```

## 4.3 Example: Location

Если TextSystem или RH хочет добавить общее поведение location instances:

```lua
function Location:is_safe()
    return (self.danger_level or 0) <= 0
end

function Location:can_rest()
    return self:is_safe() and self.allow_rest ~= false
end
```

Usage:

```lua
local target = location("rh:location.inn")

if target:can_rest() then
    ...
end
```

## 4.4 Example: Quest

```lua
function Quest:is_active()
    return self.state == "active"
end

function Quest:is_completed()
    return self.state == "completed"
end
```

Механизм тот же самый.

Нет отдельного actor-extension API, location-extension API и quest-extension API на уровне author-facing syntax.

---

# 5. Authoring environment API

## 5.1 Preferred syntax

Основной синтаксис:

```lua
function EntityKind:method_name(...)
    ...
end
```

Он должен оставаться обычным Lua и хорошо читаться без знания framework internals.

## 5.2 Explicit equivalent

Внутренне или для tooling может существовать эквивалентный API:

```lua
extend(Actor, {
    add_gold = function(self, amount)
        ...
    end,
})
```

или:

```lua
entities.extend("Actor", {
    add_gold = ...
})
```

Но это **не основной gameplay-facing syntax**.

Он может использоваться:

- runtime internals;
- tests;
- generated code;
- debugging/tooling.

## 5.3 Entity prototypes are controlled objects

`Actor`, `Location`, `Quest` и другие prototypes не должны быть обычными бесконтрольными global tables.

Их `__newindex` может перехватывать:

```lua
Actor.add_gold = function(...)
```

и регистрировать extension metadata.

Таким образом:

```lua
function Actor:add_gold(amount)
```

является синтаксическим сахаром над controlled extension registry.

---

# 6. Runtime model

## 6.1 Entity extension registry

Runtime должен иметь общий registry концептуального вида:

```text
EntityExtensionRegistry
  Actor
    method A
    method B

  Location
    method C

  Quest
    method D
```

Registry хранит **поведение**, а не gameplay state.

## 6.2 Extension registration

Conceptually:

```lua
entity_extensions.register(
    source_module,
    entity_kind,
    method_name,
    implementation
)
```

Например:

```text
rh:authoring.actors
Actor
add_gold
<function>
```

Но gameplay author этого API не видит.

## 6.3 Instance lookup

Когда вызывается:

```lua
player:add_gold(10)
```

lookup должен концептуально идти:

```text
instance/state fields
        ↓
entity-specific built-in behavior
        ↓
registered extension methods
```

Точная реализация через metatable chain, composed prototype или cached method table остаётся внутренней деталью.

## 6.4 No wrapper-per-extension requirement

Proposal не требует строить новый wrapper на каждое расширение.

Предпочтительно собирать effective method table для каждого entity kind при session/bootstrap preparation.

Пример:

```text
Actor effective methods
  Core methods
  + TextSystem methods
  + RH methods
  + Mod methods
```

После завершения bootstrap эта структура может быть immutable.

---

# 7. Layer composition

## 7.1 Example

TextSystem:

```lua
function Actor:is_player()
    return self.discriminator == "player"
end

function Actor:is_npc()
    return self.discriminator == "npc"
end
```

RH:

```lua
function Actor:add_gold(amount)
    self.gold = (self.gold or 0) + amount
end
```

Mod:

```lua
function Actor:add_corruption(amount)
    self.corruption = (self.corruption or 0) + amount
end
```

Effective Actor API:

```text
Actor
  is_player()
  is_npc()
  add_gold()
  add_corruption()
```

Ни один слой не должен повторно регистрировать `player`/`npc` discriminator только ради добавления методов.

## 7.2 Extension ordering

Регистрация extensions должна происходить в deterministic package/module order.

Например:

```text
Core
→ TextSystem
→ Game package
→ Mods in resolved load order
```

Точный порядок должен использовать существующий authoritative package resolution order, а не отдельную независимую систему приоритетов.

---

# 8. Method conflicts

Это критически важно для общего механизма.

## 8.1 Default rule: duplicate method is an error

Если два независимых extension sources определяют одинаковый метод:

```lua
function Actor:add_gold(...)
```

и такой метод уже зарегистрирован, bootstrap validation по умолчанию должна завершаться ошибкой.

Это предотвращает скрытый override по load order.

Пример diagnostic:

```text
entity_extension.method_conflict

entity_kind = Actor
method = add_gold
existing_source = rh:authoring.actors
new_source = some_mod:authoring.actors
```

## 8.2 Explicit override can be added separately

Если mods действительно должны переопределять методы, это должно быть явной операцией, например:

```lua
override(Actor, "add_gold", function(self, amount)
    ...
end)
```

или другим отдельным API.

**Implicit last-writer-wins не предлагается.**

Это решение можно оформить отдельным proposal, если override semantics понадобятся.

---

# 9. State semantics

Entity extension methods работают с тем же canonical gameplay state, которым владеет Lua.

Например:

```lua
function Actor:add_gold(amount)
    self.gold = (self.gold or 0) + amount
end
```

не создаёт дополнительную actor model.

`self.gold` остаётся частью canonical Lua gameplay state.

Extension mechanism добавляет **поведение**, а не второй слой данных.

---

# 10. Commands and mutation rules

Новый extension mechanism не меняет архитектурное правило:

> Gameplay changes occur through Commands.

Методы сущностей могут мутировать state, если они вызываются внутри command execution path и соответствуют существующим runtime rules.

Например:

```lua
commands.reward = function(ctx)
    ctx.player:add_gold(10)
end
```

Сам факт наличия:

```lua
Actor:add_gold()
```

не превращает arbitrary presentation/UE code в допустимый mutation path.

Extension API не должен обходить CommandBus или существующие mutation guards.

---

# 11. Validation

Bootstrap/pre-session validation должна проверять:

- entity kind существует;
- method name допустим;
- implementation является function;
- нет запрещённого duplicate method;
- source package/module известен;
- extension разрешён для данного слоя/runtime environment.

Ошибки должны обнаруживаться до начала gameplay session, как и остальные static/runtime registrations.

---

# 12. Introspection

Для debugging и tooling полезно иметь read-only introspection API.

Conceptually:

```lua
game.runtime.entity_extensions.describe("Actor")
```

может возвращать:

```lua
{
    {
        method = "is_player",
        source = "textsystem:authoring.actors",
    },
    {
        method = "add_gold",
        source = "rh:authoring.actors",
    },
}
```

Это не обязательно author-facing API, но сильно упрощает диагностику конфликтов и mod composition.

---

# 13. File organization

Proposal не требует жёсткой схемы файлов.

Возможный вариант:

```text
GameData/
  textsystem/
    scripts/
      authoring/
        actors.lua
        locations.lua

  rh/
    scripts/
      authoring/
        gameplay.lua
        actors.lua
        locations.lua
```

Но файл может группировать несколько entity extensions, если это удобнее предметной области.

Например:

```text
rh/scripts/authoring/economy.lua
```

может содержать:

```lua
function Actor:get_gold()
    ...
end

function Actor:add_gold(amount)
    ...
end

function Shop:get_price(...)
    ...
end
```

Физическая структура файлов не должна определять runtime semantics.

---

# 14. Migration: Actor

Actor является первым и обязательным migration case.

## 14.1 TextSystem

Текущие общие методы:

```lua
is_player()
is_npc()
```

должны объявляться через новый authoring extension mechanism.

Целевой код:

```lua
function Actor:is_player()
    return self.discriminator == "player"
end

function Actor:is_npc()
    return self.discriminator == "npc"
end
```

Без actor decorator только ради этих методов.

## 14.2 RH

RH сохраняет свои методы:

```lua
get_gold()
add_gold()
require_gold()
spend_gold()
```

но объявляет их напрямую:

```lua
function Actor:get_gold()
    return self.gold or 0
end
```

Из RH удаляются:

- `actor_decorator`;
- ручной `setmetatable`;
- `register_type("player", ...)`;
- `register_type("npc", ...)`;
- module descriptor, если он существует только ради extension registration;
- dependency на low-level actor registry.

## 14.3 Actor type registration remains separate

Важно разделить две операции:

```text
register entity/type
```

и:

```text
extend entity behavior
```

Если `player` и `npc` действительно требуют регистрации как actor discriminator, это выполняется один раз владельцем соответствующей type system.

Добавление метода:

```lua
Actor:add_gold()
```

не имеет к discriminator registration никакого отношения.

---

# 15. Future entity kinds

После Actor тот же механизм можно использовать для других gameplay entities.

Кандидаты:

```text
Location
Quest
Item
Faction
Character/Actor subtype abstractions
```

Однако новый entity kind не должен автоматически становиться extensible только потому, что существует Lua table с таким именем.

Extensible kinds должны явно регистрироваться runtime/authoring infrastructure.

Conceptually:

```lua
entity_kinds.register("Actor", ...)
entity_kinds.register("Location", ...)
```

После этого authoring environment может предоставить:

```lua
Actor
Location
```

---

# 16. Naming

Рабочее название механизма:

**Entity Authoring Extensions**

Ключевые понятия:

```text
entity kind
entity authoring prototype
extension method
extension source
effective method table
```

Следует избегать названия `class`, если система не вводит полноценную class/inheritance semantics.

`Actor`, `Location` и т. п. в authoring environment являются **domain prototypes**, а не обязательно Lua classes.

---

# 17. Implementation sketch

Один из возможных вариантов.

## 17.1 Registration proxy

```lua
local function create_entity_authoring_proxy(entity_kind, source, registry)
    return setmetatable({}, {
        __newindex = function(_, method_name, implementation)
            assert(type(method_name) == "string")
            assert(type(implementation) == "function")

            registry:register(
                source,
                entity_kind,
                method_name,
                implementation
            )
        end,

        __index = function(_, method_name)
            return registry:get_visible_method(entity_kind, method_name)
        end,
    })
end
```

Authoring environment:

```lua
env.Actor = create_entity_authoring_proxy(
    "Actor",
    current_module_id,
    entity_extension_registry
)

env.Location = create_entity_authoring_proxy(
    "Location",
    current_module_id,
    entity_extension_registry
)
```

Gameplay code:

```lua
function Actor:add_gold(amount)
    self.gold = (self.gold or 0) + amount
end
```

## 17.2 Finalization

После загрузки authoring modules:

```text
collect extensions
→ validate
→ detect conflicts
→ compose effective method tables
→ freeze
```

Runtime instances затем используют уже подготовленную immutable composition.

---

# 18. Tests

Минимальный набор tests.

## Authoring syntax

Проверить:

```lua
function Actor:test_method()
    return 42
end
```

и вызов:

```lua
actor:test_method() == 42
```

## Multiple entity kinds

```lua
function Actor:a() end
function Location:b() end
```

оба extension должны регистрироваться независимо.

## Layer composition

TextSystem method + RH method доступны на одном Actor instance.

## State write-through

```lua
function Actor:set_value(v)
    self.value = v
end
```

изменяет canonical actor state.

## Duplicate detection

Два источника определяют:

```lua
Actor:test()
```

Bootstrap должен завершиться conflict error.

## Isolation

```lua
Actor:test()
```

не должен появиться на `Location`.

## Determinism

Одинаковый package set и load order дают одинаковый effective entity API в UE и Headless.

## Persistence

Extension methods не сериализуются в save state; сериализуется только canonical entity state.

После restore entity снова получает effective methods из текущего validated runtime composition.

---

# 19. Acceptance criteria

Proposal считается реализованным, когда:

1. Authoring environment поддерживает хотя бы несколько extensible entity kinds через общий mechanism.
2. Gameplay package может объявить:

   ```lua
   function Actor:add_gold(amount)
   ```

   без прямого обращения к actor registry.
3. Тот же mechanism может использоваться, например, для `Location`.
4. TextSystem `Actor:is_player()` / `Actor:is_npc()` используют новый mechanism.
5. RH actor methods используют новый mechanism.
6. RH больше не регистрирует `player`/`npc` только ради добавления методов.
7. Gameplay extension code не использует `setmetatable()` для wrapper composition.
8. Duplicate method conflicts обнаруживаются до session start.
9. Extension composition deterministic.
10. UE и Headless получают одинаковый effective entity API.
11. Save state не зависит от extension wrapper/prototype objects.
12. Existing gameplay scenarios продолжают работать без изменения observable behavior.

---

# 20. Resulting architecture

После реализации gameplay authoring выглядит так:

```lua
-- TextSystem
function Actor:is_player()
    return self.discriminator == "player"
end

-- RH
function Actor:add_gold(amount)
    self.gold = (self.gold or 0) + amount
end

function Location:can_work_here()
    return self.work_available == true
end
```

А runtime internally выполняет:

```text
authoring declarations
        ↓
EntityExtensionRegistry
        ↓
pre-session validation
        ↓
effective methods per entity kind
        ↓
runtime entity handles
```

Таким образом, gameplay packages описывают **поведение предметной области**, а не механизм построения runtime wrappers.

Главный архитектурный принцип:

> **Any supported domain entity may be extended by higher-level gameplay layers using ordinary Lua method declarations; registration and composition are runtime concerns, not gameplay authoring concerns.**
