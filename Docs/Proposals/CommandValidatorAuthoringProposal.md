---
title: Command Validator Authoring Proposal
status: draft
proposal_state: accepted_for_planning
version: 1.1
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

Формула включает путь target command, поэтому для команды с составным путём (`rh:command.location.travel`) итоговый ID не разбирается обратно: по строке нельзя определить, где кончается путь команды и начинается `name`. Поэтому **ID Validator считается непрозрачным**: связь с target хранится отдельным полем записи реестра и доступна через интроспекцию, а инструменты (`refs`, `rename`) обязаны использовать это поле, а не разбор строки.

## Разрешение target Command

`command_ref` нормализуется в canonical `command_id` при объявлении. До регистрации Validator adapter обязан проверить, что handler с таким ID уже зарегистрирован.

**Разрешение target выполняется в две фазы.** Объявления Validators накапливаются во время фазы `register` и не требуют, чтобы target handler уже был зарегистрирован. Существование target проверяется однократно на общей заморозке, когда все handlers всех модулей уже зарегистрированы.

Порядок фазы `register` внутри authoring module:

1. зарегистрировать собственные Command handlers;
2. накопить объявления Validators, нормализовав `command_ref` в canonical `command_id`;
3. зарегистрировать остальные накопленные declarations;
4. на общей host-side freeze разрешить target каждого Validator и зарегистрировать его в runtime registry.

Одноэтапная проверка «target обязан существовать в момент объявления» отвергнута: модули обнаруживаются автоматически, поэтому она сделала бы порядок файлов частью контракта, а переименование файла — источником отказа bootstrap. Технической потребности в раннем разрешении нет: Validator не обращается к handler до вызова.

Если после регистрации всех модулей target отсутствует, bootstrap завершается `AuthoringValidatorTargetMissing` с указанием declaring package и target ID. Неявное создание «мёртвого» Validator по-прежнему запрещено; правило «зависимость пакета объявляется явно» остаётся, но относится к манифесту, а не к моменту проверки.

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

Две похожие реализации запрещены. Но текущую эвристику **нельзя извлекать как есть**: она содержит захардкоженный список игровых имён параметров внутри ядра:

```lua
-- Scripts/authoring/context.lua, подлежит удалению
local primary_arg = rehydrated.target_location_id
    or rehydrated.location_id
    or rehydrated.target
    or rehydrated.item_id
    or rehydrated.item
    or rehydrated.destination
```

`item`, `location` и `destination` в `core` нарушают [INV-016](../Architecture/Invariants.md), и существующий гейт этого не ловит: `validate_core_boundary.py` проверяет определения, схемы и идентификаторы, а не имена переменных в Lua. Извлечение списка в общий helper удвоило бы число зависящих от него мест и превратило временную эвристику в разделяемый контракт.

Поэтому `decode_authoring_args` определяется без знания имён:

| Форма `raw_args` | Результат |
|---|---|
| Массив (`#raw_args > 0`) | Позиционные аргументы в порядке индексов |
| Пустая таблица | Вызов без аргументов |
| Иная таблица | Один аргумент — таблица целиком |

Позиционный аргумент, являющийся валидным Stable ID и разрешимым в pinned repository, передаётся как definition wrapper. Правило основано на форме значения, а не на имени параметра, поэтому ядру не нужно знать ни одного игрового понятия.

Единственный вызов, полагавшийся на список имён, — `textsystem` передаёт `{ target_location_id = conn_id }`; он переводится на позиционную форму. Потребители (`Location:is_connected`, `Location:require_connected`, `Actor:move_to`) уже принимают и строку, и handle, поэтому наблюдаемое поведение не меняется.

### Целевая форма: объявленный контракт аргументов

Позиционное декодирование остаётся выводом по форме значения, то есть догадкой. Целевая форма — объявленный контракт аргументов команды на словаре дескрипторов из [ADR-0032](../ADR/0032-field-contracts-and-generic-instance-creation.md):

```lua
commands.travel = {
    args = { target = field.ref_definition("location") },
    handler = function(target) … end,
}
```

Тогда декодирование становится детерминированным, Validator и handler получают одно и то же по построению, а проверка аргументов выполняется до вызова обоих — то есть закрывается и разрыв, оставленный [RHActorsSimplification](../Plans/RHActorsSimplification/README.md) при удалении `validate_amount`. Это отдельное направление со своим ADR и в v1 не входит; здесь фиксируется как цель, чтобы позиционное декодирование не закреплялось как окончательное.

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

Закрытого mutation window недостаточно: часть helper APIs способна создавать очереди или presentation effects без прямой записи в canonical state.

При этом объём необходимых охранников ограничен и перечислим. Validator исполняется **внутри** `dispatch`, где `is_dispatching = true`, а окно мутации закрыто, поэтому запись в canonical state и вложенный `run(...)` уже отклоняются существующими механизмами, а поздние регистрации — заморозкой реестров. Новые проверки execution scope требуются ровно в четырёх точках:

| Точка | Что сейчас её не останавливает |
|---|---|
| `emit(...)` | Публикация факта не требует ни окна мутации, ни dispatch |
| `show_screen(...)` | Presentation effect не является мутацией состояния |
| `commands.*:later(...)` | Постановка в очередь не является вложенным dispatch |
| Мутирующие точки входа Gameplay Service | Не проходят через authoring helpers |

Требование «каждый helper обязан проверять scope» заменяется этим перечнем: каждая точка получает охранник и спеку на отказ. Добавление нового helper с observable side effect обязано расширять перечень в том же change set.

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

## Область действия Validator

Validator применяется к каждому dispatch своей Command независимо от источника запроса. В частности, `commands.*:later(...)` ставит в очередь настоящий dispatch, поэтому отложенный вызов проходит ту же цепочку Validators. Внутренним вызовом Validator обойти нельзя.

Отдельно фиксируется существующий порядок: `run_validators` выполняется **до** проверки наличия handler, поэтому для неизвестной Command сначала отработают все Validators, а затем вернётся отказ `core:error.command.unknown`. Гарантия «Validator всегда имеет target handler» обеспечивается проверкой на заморозке, а не диспетчером.

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

**Сегодня замена handler происходит молча.** Authoring adapter регистрирует обработчик как `register(cmd_id, wrapped_handler, { override = exists })`: более поздний пакет заменяет существующий обработчик без единого сигнала. Предложение впервые делает эту операцию значимой — Validators чужих пакетов переживают замену и продолжают ограничивать Command, реализацию которой они больше не видели.

Поэтому замена обработчика переводится на явное объявление: команда, допускающая замену, помечается заменяемой, иначе повторная регистрация отклоняется. Это та же форма, что выбрана для замещения Lua-модулей ([ADR-0025](../ADR/0025-lua-module-replacement-and-export-freezing.md)) и для повторного объявления поля ([ADR-0032](../ADR/0032-field-contracts-and-generic-instance-creation.md)): запечатано по умолчанию, заменяемо по явному признаку.

Замена обработчика не принадлежит механизму Validators и выполняется отдельной задачей плана.

Validator не получает provenance target handler и не может зависеть от UObject либо внутренней реализации Command.

## Ошибки authoring boundary

Минимальный стабильный набор diagnostic identifiers:

| Ошибка | Условие |
|---|---|
| `InvalidAuthoringValidatorCommand` | `command_ref` не descriptor и не canonical Command ID |
| `InvalidAuthoringValidatorName` | `name` не один допустимый Stable ID segment |
| `InvalidAuthoringValidatorFunction` | третий аргумент не function |
| `AuthoringValidatorTargetMissing` | target handler отсутствует на общей заморозке, после регистрации всех модулей |
| `AuthoringValidatorDuplicate` | duplicate identity внутри package |
| `AuthoringValidatorDeclarationAfterFreeze` | declaration сделана после закрытия authoring phase |
| `AuthoringValidatorSideEffectDisallowed` | Validator попытался выполнить observable side effect |
| `CommandNotReplaceable` | повторная регистрация handler для Command, не объявленной заменяемой |

Диагностика обязана содержать declaring package, validator ID и target command ID, если они уже известны. Ошибка внутри `validator_fn` атрибутируется package, объявившему Validator, а не владельцу target Command.

## Изменения реализации

Минимальный набор:

- `Scripts/authoring/context.lua` — удаление списка игровых имён параметров из декодирования аргументов, declarations Validators, общий execution scope, wrapper и расширение `fail()`;
- `GameData/textsystem/scripts/presentation/location_presenter.lua` — перевод единственного вызова, полагавшегося на список имён, на позиционную форму;
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
- одинаковый observable result и diagnostics в headless и UE;
- запуск Validators при отложенном вызове `commands.*:later(...)`;
- отсутствие в `core` знания об именах игровых параметров: позиционная, пустая и табличная формы декодируются без списка имён;
- отказ `CommandNotReplaceable` при повторной регистрации handler для Command, не объявленной заменяемой;
- сохранение Validators чужих пакетов при законной замене handler.

Дополнительная C++-копия этих проверок не создаётся.

## Риски и ограничения

| Риск | Решение v1 |
|---|---|
| Глобальный scan Validators | принять `O(all validators)`, измерить до оптимизации |
| `fail()` случайно скрывает bug | перехватывать только внутренний sentinel |
| Validator делает side effect без state mutation | enforced execution-scope guards на helper/service boundaries |
| Target typo создаёт бесполезную policy | проверять наличие handler на заморозке, после регистрации всех модулей |
| Mod override меняет аргументы | считать argument semantics частью stable Command contract; замена handler требует явного объявления заменяемости |
| Priority превращается в скрытую gameplay dependency | не открывать priority в designer API |
| ID зависит от структуры файлов | не включать module path в Stable ID |
| ID не разбирается обратно | считать ID непрозрачным, связь с target хранить полем записи |
| Позиционное декодирование остаётся догадкой | зафиксировать объявленный контракт аргументов как целевую форму |

## Итог

Предлагаемый v1 остаётся тонким authoring adapter поверх существующего Command pipeline. Он решает реальный mod/package composition use case, не создаёт новый subsystem и сохраняет возможность позже оптимизировать lookup по измерениям, не меняя designer-facing API.
