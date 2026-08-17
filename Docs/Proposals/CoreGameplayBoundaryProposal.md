---
title: Core and Gameplay Boundary Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.2
updated: 2026-08-17
depends_on:
  - ../Architecture/Modding.md
  - ../Architecture/StableIDSpecification.md
  - ../Architecture/LuaRuntimeContract.md
  - ../Architecture/GameDataRepositoryContract.md
decisions:
  - ../ADR/0025-lua-module-replacement-and-export-freezing.md
  - ../ADR/0026-core-and-gameplay-ownership.md
---

# Предложение по границе GV2 Core и gameplay packages

> **Предлагает:** правило ownership, по которому новая сущность однозначно относится к framework core либо к gameplay package.
> **Затрагивает:** [Modding](../Architecture/Modding.md), [Stable ID Specification](../Architecture/StableIDSpecification.md), [GameDataRepository](../Architecture/GameDataRepositoryContract.md).
> **Не является нормативным:** правило владения принято [ADR-0026](../ADR/0026-core-and-gameplay-ownership.md) и перенесено в contracts; документ сохраняется как исходное обоснование и целевая картина.

Цель — не изменить существующие runtime contracts без необходимости, а зафиксировать правило ownership, по которому новые сущности можно однозначно относить либо к framework core, либо к конкретной игре или feature package.

## 1. Мотивация

После выделения отдельного gameplay package возникла необходимость определить, какие сущности должны оставаться в `core`.

Главный риск — постепенное накопление в `core` конкретных игровых понятий:

```text
actor HP
inventory
quests
trade
dialogue
combat
locations
shops
...
```

Если такие системы добавлять непосредственно в `core`, со временем GV2 перестанет быть reusable gameplay framework и начнёт отражать модель одной конкретной игры.

Поэтому предлагается разделять:

```text
GV2 Core
= механизмы

Gameplay Package
= правила и предметная модель конкретной игры
```

## 2. Основной критерий

Для каждой новой сущности применяется следующий тест:

> Если текущий gameplay package заменить совершенно другой игрой на GV2, останется ли эта сущность необходимой с тем же смыслом?

Если **да** — сущность является кандидатом в `core`.

Если **нет** — она принадлежит gameplay package либо отдельному reusable feature package.

## 3. Основной принцип

Рекомендуется зафиксировать правило:

> **Core определяет механизмы и контракты. Gameplay packages определяют игровую семантику и контент.**

Например:

```text
Core:
как выполняются Commands

Gameplay:
какие Commands существуют
```

```text
Core:
как публикуются Events

Gameplay:
какие gameplay-факты представлены Events
```

```text
Core:
как разрешаются Runtime Instances

Gameplay:
какие свойства имеет конкретный Actor
```

## 4. Предлагаемая модель пакетов

Целевая структура допускает три уровня:

```text
GV2 Core
    │
    ├── runtime infrastructure
    ├── repository infrastructure
    ├── command/event infrastructure
    ├── instance infrastructure
    ├── save/load infrastructure
    └── presentation contracts
             │
             ▼
Reusable Feature Packages
    │
    ├── inventory
    ├── dialogue
    ├── quests
    └── другие действительно reusable features
             │
             ▼
Gameplay Package
    │
    ├── конкретные actors
    ├── items
    ├── locations
    ├── rules
    ├── screens
    └── content
```

Reusable Feature Packages не являются обязательной частью v1.

Они вводятся только тогда, когда конкретная игровая подсистема реально доказала повторную применимость.

## 5. Что должно оставаться в Core

### 5.1. Stable ID infrastructure

Core владеет:

- синтаксисом Stable ID;
- parsing и validation Stable ID;
- правилами namespaces;
- общими правилами uniqueness;
- registry допустимых infrastructure kinds, если он необходим framework.

Core не должен владеть конкретными игровыми IDs.

### 5.2. Package и module lifecycle

Core владеет:

```text
package discovery
dependency resolution
module registration
session lifecycle hooks
load ordering
validation phases
```

Конкретные gameplay modules принадлежат gameplay package.

### 5.3. GameDataRepository infrastructure

Core владеет:

```text
candidate construction
validation pipeline
override processing
atomic publication
snapshot pinning
reference resolution infrastructure
repository query API
```

Core не должен определять содержание конкретных gameplay definitions, если оно не является частью общего GV2 protocol.

## 6. Commands

Core владеет механизмом:

```text
Command DTO ingress
Command Registry
Dispatcher
structural validation infrastructure
gameplay validator infrastructure
mutation window
nested-command rules
FIFO ingress
command execution result
```

Gameplay package владеет конкретными gameplay Commands.

Например:

```text
rh:command.shop.buy
rh:command.location.travel
rh:command.quest.accept
```

не должны принадлежать `core`.

Принцип:

> **Core определяет язык Commands. Gameplay package определяет словарь Commands.**

## 7. Events

Core владеет:

```text
EventBus
event envelope
subscriber registry
delivery ordering
event lifecycle
```

Gameplay package владеет конкретными gameplay facts.

Например:

```text
rh:event.shop.purchased
rh:event.actor.rewarded
rh:event.location.entered
rh:event.quest.accepted
```

Core не должен содержать Events, описывающие правила конкретной игры.

## 8. Validators

Core владеет механизмом регистрации и исполнения Validators.

Core также может содержать validators, проверяющие framework-level invariants.

Gameplay validators должны находиться в gameplay package.

Например:

```text
can_afford_item
can_enter_location
can_accept_quest
```

не являются обязанностью GV2 Core.

## 9. Gameplay Services

Core может содержать только framework-level services.

Gameplay Services, реализующие конкретные workflows, принадлежат gameplay package.

Например:

```text
TradeService
TravelService
QuestService
CombatService
```

не должны становиться частью core только потому, что используются многими gameplay systems одной игры.

Если позже какая-либо service станет действительно reusable между разными играми, она может быть вынесена в отдельный feature package.

## 10. Runtime Instances

Core должен владеть общей инфраструктурой Runtime Instances:

```text
instance identity
persistent counters
instance resolution
registry lifecycle
wrapper lifecycle rules
deterministic enumeration
```

Например:

```text
game.instances
```

является framework-level API.

## 11. ActorRegistry

Сам механизм `ActorRegistry` может оставаться в core, если Actor рассматривается как базовая runtime entity GV2.

Core-level ActorRegistry отвечает только за:

```text
identity
lookup
creation protocol
removal protocol
wrapper construction
deterministic enumeration
```

Он не должен содержать gameplay rules.

Нежелательно:

```lua
game.instances.actors:add_gold(...)
game.instances.actors:damage(...)
game.instances.actors:buy_item(...)
```

Предпочтительно:

```lua
local actor = game.instances.actors.get(id)
actor:add_gold(...)
```

где конкретные свойства и методы wrapper определяются gameplay layer.

## 12. Actor definitions

Kind `actor` может оставаться framework-level понятием, если он используется runtime infrastructure.

Но конкретная schema Actor не должна автоматически считаться частью core.

Например поля:

```text
base_hp
gold
strength
player/npc
portrait
faction
```

являются gameplay semantics, если GV2 runtime сам по себе не требует их существования.

Поэтому рекомендуется разделять:

```text
Core:
actor как definition/runtime category

Gameplay:
конкретная actor schema
```

## 13. Runtime discriminator Actor

Если Actor wrappers различаются по поведению, discriminator должен находиться в Actor definition, а не дублироваться в Runtime Instance state.

Пример:

```json
{
    "id": "rh:actor.aria",
    "data": {
        "runtime_type": "rh:npc"
    }
}
```

Runtime state хранит только:

```lua
{
    definition_id = "rh:actor.aria"
}
```

ActorRegistry получает definition из pinned repository snapshot и выбирает соответствующий wrapper.

## 14. Mod extension Actor wrappers

Gameplay packages и mods должны иметь документированную точку регистрации:

```text
runtime discriminator
        ↓
Lua wrapper class/factory
```

Core Registry не должен быть жёстко прошит только под `Player` и `NPC`.

Точная форма API определяется отдельным contract.

Например концептуально:

```lua
game.instances.actors.register_type(
    "rh:npc",
    NPC
)
```

При отсутствии регистрации для используемого discriminator validation/session bootstrap должен завершаться typed diagnostic, а не молча создавать неподходящий wrapper.

## 15. Item

Kind `item` может оставаться в общем kind registry, если это сознательно принято как базовая категория GV2.

Однако конкретная schema Item должна принадлежать gameplay package, если содержит такие поля как:

```text
price
damage
weight
icon
equipment_slot
rarity
```

GV2 Core не должен предполагать, что любой Item имеет цену, вес или возможность экипировки.

## 16. Location

Аналогичное правило применяется к `location`.

Core может знать Stable ID kind `location`, если это часть общего framework model.

Но schema вида:

```text
title_text_id
screen_ids
travel_cost
neighbours
```

должна принадлежать gameplay package, если runtime GV2 сам не требует этих свойств.

## 17. Quests и другие gameplay entities

Наличие Stable ID kind не означает, что Core обязан владеть gameplay semantics этого kind.

Например Core может поддерживать:

```text
quest
effect
operation
```

как namespace categories, если они уже входят в публичный framework contract.

Но конкретные schemas, rules и handlers должны находиться в gameplay package либо feature package.

## 18. Schemas

Рекомендуется принять следующее правило:

> **Schema принадлежит Core только тогда, когда её структура необходима самому GV2 runtime или host boundary.**

Если schema описывает правила конкретной игры, она принадлежит gameplay package.

Это означает, что следует отдельно пересмотреть существующие:

```text
actor_v1
item_v1
location_v1
```

и определить, какие поля действительно являются framework requirements.

## 19. Framework schemas

Хорошими кандидатами на Core являются schemas, задающие общий protocol между gameplay и host.

Например:

```text
screen
text
resource
module
command envelope
event envelope
diagnostic
error
```

при условии, что их структура действительно интерпретируется framework/runtime.

## 20. Presentation

Core владеет presentation protocol:

```text
desired presentation DTO
screen field binding rules
resource references
text references
semantic input
widget/block protocol
```

Gameplay package владеет конкретными screens и их назначением.

## 21. System Screens и Gameplay Screens

Допускаются Core screens только для framework/system UI.

Например потенциально:

```text
core:screen.error
core:screen.recovery
core:screen.loading
```

если ими владеет сам framework.

Конкретные игровые экраны принадлежат gameplay package:

```text
rh:screen.inventory
rh:screen.character
rh:screen.location
rh:screen.shop
```

## 22. Demo content

Demo content не должен постепенно становиться частью production core.

Если GV2 требует демонстрационных данных, рекомендуется отдельный package:

```text
sample
demo
example_game
```

вместо размещения demo Actors, Items или Screens в `core`.

## 23. Text и Resources

Core должен владеть schema/protocol для:

```text
text
resource
```

если эти types являются частью общего presentation/content boundary.

Но конкретные:

```text
названия персонажей
описания предметов
иконки
портреты
музыка
```

принадлежат соответствующему gameplay package.

## 24. Errors и Diagnostics

Framework errors должны использовать `core:error.*`.

Например:

```text
core:error.command.unknown
core:error.command.nested_execution
core:error.instance.not_found
```

Gameplay failures должны использовать namespace gameplay package:

```text
rh:error.shop.not_enough_gold
rh:error.location.closed
```

Аналогичное правило рекомендуется для diagnostics.

## 25. Save/Load

Core владеет:

```text
save protocol
state codec contract
storage interaction
version envelope
load lifecycle
migration mechanism
```

Gameplay package владеет:

```text
содержанием своего canonical state
gameplay-specific migration logic
module-specific validation
```

Core не должен интерпретировать gameplay fields сохранения.

## 26. Headless

Headless Host является частью framework infrastructure.

Он должен использовать те же:

```text
Content Core
repository
Lua runtime
Commands
state/save contracts
```

что UE Host.

Headless не должен зависеть от конкретного gameplay package кроме загрузки этого package как обычного content/runtime dependency.

## 27. Namespace `core`

Рекомендуется уточнить нормативное значение namespace `core`.

Предлагаемая формулировка:

> **Namespace `core` принадлежит GV2 framework/runtime и используется только для framework-level definitions, modules, protocols, errors, diagnostics и других сущностей, смысл которых не зависит от конкретной игры.**

Основной gameplay package должен использовать собственный namespace.

Например:

```text
rh:actor.aria
rh:item.iron_sword
rh:location.tavern
rh:command.shop.buy
```

## 28. Dependency direction

Предлагается закрепить:

```text
GV2 Core
   ↑
Feature Packages
   ↑
Gameplay Package
   ↑
Mods
```

Зависимость в обратную сторону запрещена.

Core не должен:

- импортировать gameplay Lua modules;
- ссылаться на `rh:*` IDs;
- знать gameplay-specific schemas;
- предполагать наличие inventory, trade, combat, quests и других features.

## 29. Feature Packages

Если gameplay subsystem оказывается reusable, её следует выносить не в Core, а в отдельный optional feature package.

Например:

```text
gv2.inventory
gv2.dialogue
gv2.quest
```

если подобное разделение позже будет признано полезным.

Главный критерий:

> Отсутствие feature package не должно мешать работе GV2 Core.

## 30. Что предлагается пересмотреть в текущем проекте

Отдельным change set рекомендуется проверить следующие существующие сущности.

### Кандидаты на перенос из Core в gameplay package

```text
actor schema, если она содержит player/npc/base_hp
item schema, если она требует price/icon или другие gameplay fields
location schema, если она определяет конкретную модель location
inventory screen
gameplay-specific texts/resources
gameplay-specific services
gameplay Commands/Events/Validators
```

## 31. Что рекомендуется оставить в Core

Предварительный список:

```text
Stable ID infrastructure
Package lifecycle
Module lifecycle
GameDataRepository infrastructure
schema validation mechanism
Command infrastructure
Validator infrastructure
EventBus
runtime phases
mutation window
instance identity/allocation
instance resolver infrastructure
ActorRegistry mechanism
save/load infrastructure
state codec interface
Headless integration
presentation protocol
screen protocol
text protocol
resource protocol
framework errors
framework diagnostics
```

Этот список не считается закрытым.

Каждая сущность всё равно должна проходить основной критерий из §3.

## 32. Не требуется немедленный перенос всего существующего

Предложение определяет целевую ownership boundary.

Не требуется одним change set перемещать все существующие definitions, schemas и Lua modules.

Рекомендуется:

1. зафиксировать правило;
2. новые сущности сразу создавать в правильном package;
3. существующие переносить отдельными малыми change sets;
4. не смешивать architectural cleanup с новой gameplay functionality без необходимости.

## 33. Критерий принятия нового Core API

Новая сущность может быть добавлена в Core только если можно ответить «да» на следующие вопросы:

1. Нужна ли она более чем одной потенциальной игре на GV2?
2. Может ли GV2 runtime обоснованно требовать её существования?
3. Не выражает ли она правила текущей игры?
4. Можно ли реализовать текущую игру без помещения этой сущности в Core?
5. Не лучше ли реализовать её как optional feature package?

Если ответы указывают на gameplay-specific semantics, сущность не должна попадать в Core.

## 34. Краткая формула ownership

```text
Core
= HOW

Gameplay
= WHAT
```

Более точно:

```text
Core:
как загружать
как идентифицировать
как валидировать
как исполнять
как сохранять
как передавать presentation

Gameplay:
что существует
что означает
что разрешено
что происходит
что видит игрок
```

## 35. Открытые вопросы и варианты решения

Раздел добавлен при review. Четыре места требуют выбора до реализации: без него §11, §14 и §15 не сводятся в рабочий change set.

### 35.1. Точка расширения обёртки актора (§11, §14)

Если schema актора уезжает в gameplay, ядро перестаёт знать его поля — и `get_gold` в ядре становится невозможен по определению, а не по договорённости. Точка расширения из необязательной становится обязательной. Вопрос в её форме.

| Вариант | Суть | Цена |
|---|---|---|
| **A. Реестр методов** | `register_methods("rh:npc", { add_gold = fn })`; ядро подмешивает их в обёртку по discriminator | Минимум; повторяет три существующих реестра, freeze достаётся даром. Нет наследования: метод для нескольких discriminator регистрируется несколько раз |
| **B. Реестр фабрик (как в §14)** | Gameplay отдаёт `function(actor_state) -> wrapper`, ядро только выбирает фабрику | Ядро теряет контроль над инвариантами, которые сейчас держит: неизменяемость `instance_id`/`definition_id`/`discriminator` и делегирование в `actor_state`. Каждый пакет обязан воспроизвести их сам |
| **C. Фабрика-декоратор** | Ядро строит базовую обёртку с инвариантами, gameplay оборачивает: `register_type("rh:npc", function(base) return setmetatable({…}, { __index = base }) end)` | Чуть больше A; инварианты идентичности остаются в ядре, поверх — полная свобода |

**Рекомендация: C.** Идиома `__index = base` уже принята в проекте для замещения модулей ([ADR-0025](../ADR/0025-lua-module-replacement-and-export-freezing.md)) — получается один приём на два случая вместо двух разных. Вариант A остаётся разумным меньшим стартом: он выражается через C позже без слома.

Вариант B отклоняется: инварианты идентичности — ровно то, что ядро обязано удерживать, и раздавать их каждому пакету значит гарантировать расхождение.

### 35.2. Незарегистрированный discriminator (§14)

§14 требует typed diagnostic при отсутствии регистрации. Это ломает контент, который вчера работал, и склеивает перенос схем с введением точки расширения в один неделимый change set — вопреки §32.

| Вариант | Суть | Цена |
|---|---|---|
| **A. Жёсткий отказ сразу** | Bootstrap падает диагностикой | Нет промежуточного «работает, но неправильно». Перенос схем и extension point становятся одним большим change set, откат дорогой |
| **B. Дефолтная обёртка, отказ позже** | Незарегистрированный discriminator получает базовую обёртку | Разбивается на малые change sets. Молчаливый дефолт — ровно то, от чего §14 предостерегает; нечем не забыть |
| **C. Дефолтная обёртка плюс сокращающийся список** | Дефолт работает, но известные незарегистрированные discriminator перечислены в гейте и список обязан только сокращаться; устаревшая запись — ошибка проверки | Одна временная конструкция |

**Рекомендация: C.** Приём уже применялся в этом проекте (`KNOWN_ENGINE_TO_GAME_EDGES` в плане PackageSupport) и решает ровно это противоречие: мягкий переход при невозможности забыть.

### 35.3. Схема существующего kind в пакете (§15, §16, §18)

[Modding](../Architecture/Modding.md) сейчас говорит: «New kind требует declarative schema binding» и «мод не переопределяет основную schema существующего kind». Перенос `item`/`actor`/`location` в gameplay упирается в эту формулировку.

| Вариант | Суть | Цена |
|---|---|---|
| **A. Разрешить binding для любого kind** | Ядро перестаёт привязывать схему, пакет привязывает свою; конфликт `(kind, schema_version)` остаётся fatal | Минимальная правка формулировки, механизм резолюции не меняется |
| **B. Разделить kind на framework и open** | Framework kind обязан иметь схему в ядре, open kind — обязан получить её от пакета; отсутствие схемы у open kind — диагностика | Явнее; «kind есть, схемы нет» перестаёт быть дырой. Новая сущность в contract |
| **C. Урезать схемы ядра до framework-минимума** | В ядре остаётся только то, что требует runtime; игровые поля уезжают в extension blocks пакета | Не трогает Modding вовсе, extension-механизм уже есть и уже namespaced |

**Рекомендация: A как общее правило, C отдельно для `actor`.**

Причина различия конкретная. Если ядро выбирает обёртку по discriminator (§13, §14), то **`discriminator` — framework-требование, а не игровая семантика**: без него runtime не может собрать инстанс. А `base_hp` и `name_text_id` — игровые. Это и есть ответ на вопрос §18 «какие поля действительно являются framework requirements» для actor: ровно одно.

Для `item` и `location` таких полей не обнаруживается — runtime не требует от них ничего, — поэтому их схемы уезжают целиком по варианту A.

### 35.4. Отношение к решению плана RhGamePackage

План RhGamePackage зафиксировал обратное: «Схемы остаются в `core`». План заархивирован, а архивный документ по правилам раздела не нормативен и источником задач не является — прямого конфликта нормативных источников нет.

Но §18 и §27 меняют не план, а действующие формулировки: значение namespace `core` и правило владения схемой. Это уровень ADR, а не proposal.

**Рекомендация:** вынести §17, §18 и §27 в ADR, отметив, что решение о схемах в плане RhGamePackage принималось до появления критерия §18 и им заменяется. Остальные разделы остаются proposal-ом как целевая картина.

### 35.5. Чего не хватает документу

- **Гейта на новые сущности.** §32 полагается на дисциплину «новые сущности сразу в правильном пакете». Проверяется машинно и дёшево — запретом определений kind `actor`, `item`, `location` в `GameData/core/definitions/`, по образцу `core_decoupling_gate_contract`.
- **Оговорки про замороженный корпус.** `Tests/Fixtures/PortableContentCore/valid/core` содержит `core:item.*`, `core:actor.*` и `core:location.*` со своими схемами. Это фикстура, изображающая пакет по имени `core`, а не движок; трогать её не нужно, иначе ломается golden.
- **Порядка работ.** Предлагаемая очередь: §22 (демо из ядра) → §11 (`get_gold`/`add_gold` из ActorRegistry) → §18 (схемы). Первые два дешёвые и бесспорные, третий дорогой и требует ADR.
- **Оценки стоимости §18.** Перенос `actor_v1`/`item_v1`/`location_v1` тянет `state_validator.DEFINITION_REFERENCE_FIELDS`, боевые фикстуры, UE-провайдер и разметку замещаемости. Это не «малый change set», и сказать это стоит рядом с §32.

## 36. Ожидаемый результат

После принятия proposal:

- `core` остаётся маленьким и стабильным;
- gameplay-specific semantics не просачиваются в framework;
- основной gameplay package может эволюционировать независимо;
- mods используют те же gameplay extension points;
- reusable systems можно выделять в optional feature packages;
- GV2 остаётся применимым к другим data/script-driven играм;
- принадлежность новой сущности определяется единым простым правилом.

## Основной инвариант proposal

> **GV2 Core владеет универсальными механизмами выполнения игры. Конкретная gameplay-модель, её сущности, правила, schemas, Commands, Events и screens принадлежат gameplay packages, если их смысл не требуется самому framework.**