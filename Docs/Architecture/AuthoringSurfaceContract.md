---
title: Lua Authoring Surface Contract
status: normative
version: 1.0
updated: 2026-08-20
depends_on:
  - RuntimeFacadeAndRegistries.md
  - CommandsAndEvents.md
  - CanonicalStateAndSave.md
  - ../UI/ScreenTemplates.md
decisions:
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0028-simplified-authoring-surface.md
  - ../ADR/0031-entity-authoring-extensions.md
  - ../ADR/0032-field-contracts-and-generic-instance-creation.md
  - ../ADR/0033-command-validator-authoring.md
  - ../ADR/0034-gameplay-service-authoring.md
  - ../ADR/0035-ui-foundation-and-composition.md
---

# Lua Authoring Surface Contract

> **Владеет:** authoring `_ENV`, декларативным Lua-синтаксисом, adapters в runtime, execution scope, package attribution и заморозкой authoring descriptors.
> **Не владеет:** command/event semantics, canonical state, Screen Document, Presentation Snapshot и значением runtime registry entries.
> **Инварианты:** [INV-003](Invariants.md), [INV-004](Invariants.md), [INV-006](Invariants.md), [INV-016](Invariants.md)
> **Реализация:** `Scripts/authoring/`, authoring descriptors package modules.
> **Проверки:** `Tests/Lua/authoring/`, `Tests/Lua/actors/field_contracts.lua`, `Tests/Lua/actions/`.

Authoring surface — designer-facing adapter над единственным runtime `game`, а не второй runtime и не альтернативный путь мутации. Definitions создаются JSON5 content pipeline до VM и остаются immutable. Низкоуровневый programmer API существует для infrastructure modules, но package gameplay использует authoring surface.

## Module class and environment

Файл под `scripts/authoring/` или descriptor с `authoring: true` исполняется как authoring module. Loader выводит его package/module identity, создаёт `authoring.gameplay(package_id, module_id)` и подставляет lexical `_ENV`. Возвращаемое значение chunk игнорируется; `return M` не требуется.

`_ENV` разрешает safe globals Lua и следующие имена:

| Surface | Назначение |
|---|---|
| `commands`, `validate` | команды и независимые read-only validators |
| `services`, `actions` | gameplay services и semantic action bindings |
| `Actor`, `Location`, `Quest`, `Item`, custom PascalCase prototype | методы и field descriptors типов сущностей |
| `field`, `instances` | schema fields и generic instance categories |
| `player`, `world`, `def`, `location`, `actor`, `actors` | dynamic accessors к definitions/runtime instances |
| `fail`, `emit`, `on` | typed refusal и gameplay facts/subscriptions |
| `text`, `action`, `button` | value constructors для presentation |
| `show_screen`/`show_route`, `show_overlay`, `close_overlay`, `show_modal`, `close_modal` | UI document intents |
| `tab`, `tab_container`/`tabs` | tab field constructors |

Неизвестное PascalCase-имя создаёт prototype данного entity kind. Запись нового global запрещена (`AuthoringGlobalWriteDisallowed`). Окружение не является security sandbox: trust model остаётся package-level.

## Declarations and runtime adapters

Authoring declarations накапливаются при загрузке module и не меняют runtime registries немедленно. На фазе `register` adapter канонизирует IDs, сортирует там, где порядок не является семантикой, оборачивает functions с package/execution context и регистрирует результат в owner registry.

| Authoring form | Runtime adaptation | Semantic owner |
|---|---|---|
| `commands.buy = function(...) ... end` | `<pkg>:command.buy` → `game.commands.handlers` | [Commands and Events](CommandsAndEvents.md) |
| `validate(command_ref, "rule", fn)` | opaque validator ID + explicit target → `game.commands.validators` | [Commands and Events](CommandsAndEvents.md) |
| `services.trade = { buy = fn }` | `<pkg>:service.trade` → `game.services` | [Commands and Events](CommandsAndEvents.md) |
| `actions.open = commands.open` | `<pkg>:action.open` → command binding in `game.actions` | [Semantic Input](../UI/SemanticInput.md) |
| `function Actor:move_to(...)` | method declaration → `game.entity_extensions` | [Canonical State and Save](CanonicalStateAndSave.md) |
| `Actor.gold = field.non_negative_integer(...)` | field descriptor → effective entity schema | [Canonical State and Save](CanonicalStateAndSave.md) |
| `on("changed", fn)` | package-scoped subscriber → `game.events.subscribers` | [Commands and Events](CommandsAndEvents.md) |
| presentation constructors | value-only Screen/UI document structures | [Screen Templates](../UI/ScreenTemplates.md), [UI Document](../UI/UIDocumentAndReconciliation.md) |

Краткие keys превращаются в `<package_id>:<kind>.<path>`. Cross-package reference использует полный Stable ID и считается ссылкой, а не владением. Опубликованный ID не зависит от имени локальной функции.

### Commands, validators and services

`commands[key]` принимает ровно одну function и создаёт стабильный `CommandDescriptor`. Повторное объявление даёт `CommandAlreadyDefined`, чтение неизвестного key — `UnknownCommandKey`, late declaration — `CommandDeclarationAfterFreeze`. Handler может вернуть `nil` (успех без value) или value (успех с value); только `fail()` создаёт typed refusal. `descriptor:run(...)` вызывает отдельную синхронную dispatch из idle; nested run даёт `AuthoringNestedRunDisallowed`. `descriptor:later(...)` ставит portable request в стандартную очередь.

`validate(command_ref, name, fn)` принимает descriptor или canonical command ID. Validator ID непрозрачен; target хранится отдельным полем и разрешается после загрузки всех packages. Missing target даёт `AuthoringValidatorTargetMissing`, duplicate — `AuthoringValidatorDuplicate`. Validator ordering и refusal normalization принадлежат Commands contract.

`services.name = { method = function ... }` — атомарное объявление stateless service; distributed assignment запрещён. Поля обязаны быть functions (`ServiceFieldNotFunction`), повторное/late объявление дают `ServiceDuplicateDeclaration`/`ServiceDeclarationAfterFreeze`. Реализация immutable после регистрации. `services["other:service.name"]` разрешается на freeze; missing target даёт `ServiceTargetMissing`.

### Entities, fields and references

Prototype assignment принимает function или `field.*` descriptor. Methods одного kind компилируются в immutable effective method table; конфликт declarations отклоняется, а receiver `self` обязателен. Method resolution выполняют disposable instance/definition wrappers, а не manual decorators.

Field descriptor задаёт type/constraints, `storage` (`definition` или `runtime_state`) и `write_policy` (`read_only`, `plain`, `managed`). Runtime-state field материализуется sparse при первой записи; `reset(field_name)` удаляет override. Managed field изменяется только опубликованной domain operation. Детальная форма canonical storage и reference integrity принадлежит [Canonical State and Save](CanonicalStateAndSave.md).

`player`, `world`, `actor(name)`, `actors(name)` и `def.<kind>(name)` разрешают fresh wrapper при каждом обращении; wrapper нельзя хранить в canonical state. `actor(name)` требует ровно один instance (`ActorInstanceNotFound`/`ActorInstanceAmbiguous`). `ref_definition` и `ref_instance` возвращают соответствующий disposable wrapper.

При передаче command/event arguments wrapper превращается в portable tagged reference `{ __gv2_ref = "definition|instance", id = ... }`; на входе handler/subscriber он регидрируется в fresh wrapper. Обычная строка остаётся строкой. Единый `decode_authoring_args` не знает имён полей конкретной игры.

### Presentation constructors

`text`, `action`, `button`, tab и `show_*` создают только value structures. Function/callback в action запрещён (`ActionClosureDisallowed`); user-facing raw string вместо `TextSpec` — `RawStringDisallowed`; локализуемый text не может быть UI element key (`TextDisallowedAsKey`). Точные Screen fields, layers, stable keys и apply semantics принадлежат UI contracts. Authoring adapter не создаёт параллельный Screen model.

## Execution scope and attribution

Каждый wrapped call исполняется в scope `none | command | validator | event`. Scope хранит declaring package, target ID и начальный `write_revision`, наследуется service/entity method call-ами и восстанавливается при success, refusal и exception.

- `command`: canonical mutation разрешена только открытым mutation window. `fail(key, params)` до первой записи создаёт `<declaring_package>:error.<key>`; после записи даёт `AuthoringFailAfterMutation`.
- `validator`: состояние read-only. `emit`, `show_*`, `commands.*:later` и mutating service operation дают `AuthoringValidatorSideEffectDisallowed`. `fail()` атрибутируется пакету validator-а.
- `event`: mutation window закрыт; разрешены post-commit reads, event enqueue и deferred commands по Commands contract.
- `none`: `fail()` даёт `AuthoringFailOutsideCommand`; mutation блокирует canonical state guard.

`fail()` и `emit()` внутри service/entity method атрибутируются пакету, где method объявлен, а не вызывающему package. Error `<pkg>:error.<path>` соответствует localization ID `<pkg>:text.error.<path>`.

## Freeze and failures

После module `register` authoring gate обязан:

1. адаптировать все declarations в runtime registries;
2. разрешить validator/service/action targets на полном package set;
3. проверить schema conflicts и наличие managed operations;
4. заморозить module descriptors/proxies;
5. передать управление общему [registry freeze](RuntimeFacadeAndRegistries.md#host-side-freeze-sequence).

Любой conflict, missing target, invalid Stable ID или declaration after freeze блокирует candidate session startup. Частичная authoring registration не становится доступной активной session.

## Verification

Conformance покрывает `_ENV` и запрет globals; deterministic IDs и registration; duplicate/late declarations; target resolution после всех packages; package attribution; scope restoration после refusal/error; validator side-effect guards; tagged-reference round trip; immutable service/method tables; sparse fields; отсутствие callbacks в presentation values; соответствие output структурам UI contracts.
