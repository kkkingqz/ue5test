---
title: Command Validator Authoring Proposal
status: draft
proposal_state: accepted_for_planning
version: 1.0
updated: 2026-08-19
depends_on:
  - DesignerLuaAuthoringProposal.md
  - SimplifiedAuthoringSurfaceProposal.md
  - ../Architecture/CommandsAndEvents.md
  - ../Architecture/LuaRuntimeContract.md
  - ../Architecture/Modding.md
decisions:
  - ../ADR/0003-command-and-event-model.md
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0028-simplified-authoring-surface.md
---

# Command Validator Authoring Proposal

> **Предлагает:** минимальный designer-facing API `validate(command_ref, validator_name, validator_fn)` для независимых read-only policies поверх существующих Command.
> **Затрагивает:** [Commands and Events](../Architecture/CommandsAndEvents.md), [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md), [Modding](../Architecture/Modding.md).
> **Не является нормативным:** до принятия ADR и реализации действует текущий programmer API `game.commands.validators`.

## Контекст

Runtime уже предоставляет глобальный ordered registry валидаторов, запускает его до открытия mutation window и прекращает dispatch при первом typed refusal. Authoring layer умеет объявлять Command, Event subscription и desired presentation, но для валидаторов вынуждает gameplay package знать устройство runtime registry.

Proposal добавляет только authoring adapter поверх существующего pipeline. Он не вводит новый bus, второй registry или новый C++ boundary.

## Когда нужен Validator

Проверка остаётся внутри Command handler через `require_*()` или `fail()`, если она является частью самой операции:

```lua
commands.buy = function(item)
    player:require_gold(item.price)
    player:spend_gold(item.price)
    player:add_item(item)
end
```

Validator используется, когда отдельный package/module/mod независимо добавляет policy к уже существующей Command:

```lua
validate(commands.travel, "not_grabbed", function(target)
    if player:is_grabbed() then
        fail("travel.grabbed", { target = target })
    end
end)
```

Такой Validator не владеет реализацией `travel`, не заменяет handler и не мутирует state.

## Цели

- дать designer Lua короткий API без доступа к raw validator context;
- сохранить существующую семантику registry, ordering, freeze и first-refusal-wins;
- разрешить нескольким packages и mods независимо ограничивать одну Command;
- передавать Validator те же нормализованные аргументы, что получает handler;
- использовать `fail(error_key, params)` как typed refusal;
- запретить все observable side effects во время author validator;
- сохранить одинаковое поведение в headless и UE hosts.

## Не входит

- обязательный Validator для каждой Command;
- перенос локальных предусловий из handler;
- priority в designer-facing API;
- отдельный registry по каждой Command на первом этапе;
- новый result DTO или исключения как normal refusal;
- mutation, Event, Presentation либо external operation из Validator;
- изменения C++ или новый host-specific conformance path.

## Предлагаемый API

```lua
validate(command_ref, validator_name, validator_fn)
```

Параметры:

| Параметр | Contract |
|---|---|
| `command_ref` | `CommandDescriptor` либо canonical Stable ID kind `command` |
| `validator_name` | один strict lowercase ASCII Stable ID segment |
| `validator_fn` | Lua function, получающая нормализованные аргументы Command |

Короткая ссылка используется для Command текущего authoring module:

```lua
validate(commands.travel, "not_grabbed", function(target)
    -- read-only policy
end)
```

Canonical ID используется для cross-package policy:

```lua
validate("rh:command.travel", "curse_lock", function(target)
    if player:has_status("curse.rooted") then
        fail("curse.rooted", { target = target })
    end
end)
```

Arbitrary string, raw runtime context и function/callback name как identity запрещены.

## Идентичность Validator

Authoring adapter строит Validator Stable ID по единственной формуле:

```text
<declaring_package>:validator.<target_namespace>.<target_command_path>.<name>
```

Примеры:

```text
rh:validator.rh.travel.not_grabbed
curse_mod:validator.rh.travel.curse_lock
```

Где:

- `declaring_package` — package, объявивший policy;
- `target_namespace` и `target_command_path` извлекаются из canonical `command_id`;
- `name` — ровно один segment, переданный в `validate()`;
- module path намеренно не входит в ID, чтобы перенос файла не менял опубликованную identity.

Пара `target command + name` обязана быть уникальна внутри declaring package. Повторное объявление завершается `AuthoringValidatorDuplicate`; last-writer-wins запрещён.

## Разрешение target Command

`command_ref` нормализуется в canonical `command_id` при объявлении. До регистрации Validator adapter обязан проверить, что handler с таким ID уже зарегистрирован.

Порядок фазы `register` внутри authoring module:

1. зарегистрировать собственные Command handlers;
2. проверить target IDs и зарегистрировать Validators;
3. зарегистрировать остальные накопленные declarations;
4. передать управление общему host-side freeze.

Cross-package Validator может ссылаться только на Command из package/module, который находится раньше в разрешённом `LoadOrder`. Если target ещё не зарегистрирован, bootstrap завершается `AuthoringValidatorTargetMissing`. Неявное создание «мёртвого» Validator запрещено: зависимость или порядок модулей должны быть объявлены явно.

Declaration после freeze завершается `AuthoringValidatorDeclarationAfterFreeze`.

## Adapter к существующему registry

Текущий registry глобальный: dispatcher вызывает все зарегистрированные реализации. Первый этап не меняет эту архитектуру. Каждый author Validator компилируется в реализацию текущего programmer API с локальным фильтром:

```lua
game.commands.validators.register(validator_id, {
    validate = function(runtime_ctx)
        if runtime_ctx.command_id ~= target_command_id then
            return true
        end

        return run_author_validator(runtime_ctx, validator_fn)
    end,
})
```

Это сохраняет один authoritative registry и не требует правки dispatcher. Стоимость `O(all validators)` на dispatch принимается для v1. Индекс `command_id -> validators` допустим только после измерения реального bottleneck и отдельного обновления contract.

## Единая семантика аргументов

Handler и Validator обязаны получать результат одной функции:

```lua
decode_authoring_args(raw_args)
```

Текущую rehydration и эвристику positional/map arguments необходимо извлечь из wrapper handler в один internal helper и повторно использовать в wrapper Validator. Две похожие реализации запрещены.

Для одного request:

```text
request.args
  -> tagged reference rehydration
  -> decode_authoring_args
  -> validator_fn(...)
  -> handler_fn(...), если все Validators разрешили Command
```

Stable `command_id` подразумевает стабильную семантику аргументов. Package, заменяющий handler, обязан сохранить argument contract. Несовместимая семантика требует нового `command_id`, а не адаптации только handler-а.

## `fail()` и execution scope

Существующий `fail()` знает только active Command handler, поэтому простой вызов из Validator сейчас стал бы fault. Реализация должна заменить частный флаг общим authoring execution scope:

```text
none | command | validator | event
```

Scope хранит как минимум:

- `kind`;
- `package_id`;
- `command_id`, когда применимо;
- начальный `write_revision` для Command handler.

Wrapper обязан установить scope до вызова функции и восстановить предыдущий scope в finally-подобном пути при success, typed refusal и exception.

В scope `validator`:

```lua
fail("travel.grabbed", { target = target })
```

создаёт error ID относительно declaring package и выполняет non-local exit через тот же внутренний sentinel, что Command handler. Wrapper перехватывает только этот sentinel и возвращает runtime registry:

```lua
false, {
    code = "rh:error.travel.grabbed",
    params = { target = target },
}
```

Любое другое Lua error остаётся fault Validator и приводит к `LuaDispatchError`. Перехватывать произвольное исключение как refusal запрещено.

Проверка `fail()` after mutation сохраняется для scope `command`. В scope `validator` mutation в принципе недоступна, поэтому `write_revision` не используется как замена permission enforcement.

## Read-only permission scope

Закрытого mutation window недостаточно: часть helper APIs способна создавать очереди, registrations или presentation effects без прямой записи в canonical state. Поэтому каждый authoring helper и service entry point с observable side effect обязан проверять execution scope.

В scope `validator` разрешены:

- чтение `player`, `world`, actor/entity wrappers и definitions;
- read-only repository/query APIs;
- чистые вычисления и построение portable `params` для `fail()`;
- pure text/reference helpers, не создающие presentation effect.

Запрещены:

- любая запись в canonical gameplay state;
- `emit(...)` и прямой enqueue Event;
- `commands.*:run(...)` и `commands.*:later(...)`;
- `show_screen(...)` и другие presentation effects;
- `on(...)`, регистрация Command/Validator/Subscriber и иные late declarations;
- Gameplay Service mutation;
- async/external operation и technical input;
- raw UObject, callback или Unreal API.

Нарушение даёт programmer fault `AuthoringValidatorSideEffectDisallowed`, а не typed gameplay refusal. Проверка выполняется в общем helper boundary; набор отдельных локальных соглашений без enforcement недостаточен.

## Ordering и refusal semantics

Authoring API не принимает `priority` в v1. Все author Validators регистрируются с текущим default priority `0`.

Итоговый порядок остаётся нормативным порядком runtime registry:

1. priority по возрастанию;
2. resolved package/module load order;
3. registration order внутри module.

Authoring adapter сохраняет declaration order Validators внутри module. Первый typed refusal останавливает цепочку, handler не вызывается. Programmer API сохраняет `options.priority` для редких инфраструктурных сценариев; designer API не открывает этот knob без измеренной необходимости.

## Mods и override

Validator является отдельной policy и не принадлежит target handler. Поэтому замена handler более поздним package не удаляет Validators, объявленные другими packages.

Это безопасно только при соблюдении двух правил:

- handler override сохраняет argument contract опубликованного `command_id`;
- удаление/отключение mod применяется через replacement session и новый набор frozen registries.

Validator не получает provenance target handler и не может зависеть от UObject либо внутренней реализации Command.

## Ошибки authoring boundary

Минимальный стабильный набор diagnostic identifiers:

| Ошибка | Условие |
|---|---|
| `InvalidAuthoringValidatorCommand` | `command_ref` не descriptor и не canonical Command ID |
| `InvalidAuthoringValidatorName` | `name` не один допустимый Stable ID segment |
| `InvalidAuthoringValidatorFunction` | третий аргумент не function |
| `AuthoringValidatorTargetMissing` | target handler отсутствует на фазе регистрации |
| `AuthoringValidatorDuplicate` | duplicate identity внутри package |
| `AuthoringValidatorDeclarationAfterFreeze` | declaration сделана после закрытия authoring phase |
| `AuthoringValidatorSideEffectDisallowed` | Validator попытался выполнить observable side effect |

Диагностика обязана содержать declaring package, validator ID и target command ID, если они уже известны. Ошибка внутри `validator_fn` атрибутируется package, объявившему Validator, а не владельцу target Command.

## Изменения реализации

Минимальный набор:

- `Scripts/authoring/context.lua` — declarations Validators, общий execution scope, wrapper и расширение `fail()`;
- `Scripts/authoring/commands.lua` — переиспользуемое разрешение `CommandDescriptor`/canonical ID, если оно не останется private helper context;
- единый `decode_authoring_args(raw_args)` вместо inline decoding в handler wrapper;
- scope guards в уже существующих authoring helpers и service boundaries;
- Lua specs, выполняемые обоими hosts.

`Scripts/runtime/validator_registry.lua`, `command_dispatcher.lua` и C++ не должны меняться на первом этапе, если tests не обнаружат отсутствующий runtime contract.

## Документация и ADR

Перед реализацией требуется ADR, принимающий:

- public `validate(command_ref, validator_name, validator_fn)`;
- формулу author Validator Stable ID;
- расширение `fail()` на Validator scope;
- enforced read-only authoring execution scope.

В том же change set с кодом необходимо синхронизировать:

- `Architecture/CommandsAndEvents.md`;
- `Architecture/LuaRuntimeContract.md`;
- `Architecture/Modding.md`;
- `Concepts/GameplayModel.md`;
- `Guides/AddCommand.md`;
- `Status/ImplementationStatus.md`;
- индексы ADR и затронутых каталогов.

## Acceptance criteria

Одна Lua spec suite, например `Tests/Lua/authoring/command_validators.lua`, обязана запускаться через общий путь в headless и UE hosts и покрывать:

- регистрацию local и cross-package Validators;
- allow без изменения результата Command;
- `fail()` как typed refusal с ID declaring package;
- arbitrary Lua error как dispatch fault;
- несколько Validators и deterministic first-refusal-wins;
- отсутствие вызова handler при refusal;
- missing target, duplicate, malformed reference/name/function и late declaration;
- запрет state mutation, Event, `run`, `later`, Presentation, registration и external operation;
- полное совпадение decoded arguments у Validator и handler для positional, empty и map forms;
- tagged reference rehydration;
- сохранение argument contract при handler override;
- очистку execution scope после success, refusal и exception;
- freeze semantics;
- одинаковый observable result и diagnostics в headless и UE.

Дополнительная C++-копия этих проверок не создаётся.

## Риски и ограничения

| Риск | Решение v1 |
|---|---|
| Глобальный scan Validators | принять `O(all validators)`, измерить до оптимизации |
| `fail()` случайно скрывает bug | перехватывать только внутренний sentinel |
| Validator делает side effect без state mutation | enforced execution-scope guards на helper/service boundaries |
| Target typo создаёт бесполезную policy | проверять наличие handler до freeze |
| Mod override меняет аргументы | считать argument semantics частью stable Command contract |
| Priority превращается в скрытую gameplay dependency | не открывать priority в designer API |
| ID зависит от структуры файлов | не включать module path в Stable ID |

## Итог

Предлагаемый v1 остаётся тонким authoring adapter поверх существующего Command pipeline. Он решает реальный mod/package composition use case, не создаёт новый subsystem и сохраняет возможность позже оптимизировать lookup по измерениям, не меняя designer-facing API.
