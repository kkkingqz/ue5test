---
title: Commands and Events
status: normative
version: 1.9
updated: 2026-08-18
depends_on:
  - LuaRuntimeContract.md
  - StableIDSpecification.md
decisions:
  - ../ADR/0003-command-and-event-model.md
  - ../ADR/0004-lua-state-mutation.md
  - ../ADR/0027-designer-lua-authoring-layer.md
---

# Commands and Events

> **Владеет:** конвертом команды, порядком валидаторов, mutation window, семантикой отказа, публикацией и доставкой событий, фазами и очередями.
> **Не владеет:** конкретными командами и правилами игры — они принадлежат геймплейным модулям.
> **Инварианты:** [INV-003](Invariants.md), [INV-004](Invariants.md)
> **Реализация:** `Scripts/runtime/command_dispatcher.lua`, `handler_registry.lua`, `validator_registry.lua`, `event_bus.lua`, `subscriber_registry.lua`, `mutation_window.lua`.
> **Проверки:** `Tests/Lua/commands/`, `Tests/Lua/events/`.

Command — единственная публичная форма намерения изменить gameplay-state. Event — неотменяемый факт после commit. Отдельные action bus, command bus и `before_*` events отсутствуют.

## Command envelope

```json5
{
  command_id: "core:command.location.travel",
  schema_version: 1,
  payload: {
    target_location_id: "core:location.city.market",
  },
  source: {
    kind: "ui",
    ui_instance_id: "ui@17:8",
    node_key_path: ["route", "main", "buttons", "travel_market"],
    element_id: "core:screen.main#widget.travel_market",
  },
  correlation_id: "runtime@17:51",
}
```

Command IDs используют kind `command`. Handler function name не является частью wire contract.

Command описывает действие, а не запись в поле state. `buy_item`, `travel_to`, `accept_quest`, `equip_item` являются допустимыми командами; `set_gold`, `set_health`, `append_inventory_array` — нет: они раскрывают внутреннее устройство canonical state и превращают Command API в низкоуровневый mutation protocol. Одна command может приводить к нескольким внутренним mutations.

Для `source.kind = "ui"` поля `ui_instance_id` и `node_key_path` берутся из validated UI binding record. Path обозначает logical Screen Instance/Field/item, а не physical Widget tree. `element_id` optional и является authored provenance, а не обязательной runtime identity. Blueprint не формирует command source и не передаёт authoritative `command_id` напрямую.

## Result

```lua
{ ok = true, value = { location_id = "core:location.city.market" } }
{ ok = false, error = { code = "core:error.location.locked", params = {} } }
```

Expected refusal не является runtime fault. Failed result гарантирует: state не изменён, gameplay events не опубликованы, deferred effects отброшены.

## Lifecycle

1. Validate runtime phase и source identity.
2. Validate envelope/payload schema.
3. Run ordered command validators read-only.
4. Lookup handler in `game.commands.handlers` by `request.command_id`.
5. If handler is missing: return typed refusal `{ ok = false, error = { code = "core:error.command.unknown", params = { command_id = request.command_id } } }` without opening mutation window.
6. If handler is present: open `mutation_window`, begin event context, and invoke handler function.
7. Handler mutates state через Gameplay Services или доменные методы сущностей.
8. Validate required postconditions.
9. Commit command result and enqueue gameplay facts (or rollback on refusal / discard on fault).
10. Drain EventBus FIFO/breadth-first.
11. Rebuild affected UI/presentation desired model via `game.presentation.resolve()` outside mutation window (SAS-14..16, ADR-0028).
12. Execute deferred commands from queue sequentially.

Синхронный путь вызова команд через `command_dispatcher` (`dispatch(request)`) выполняет точечный поиск обработчика в реестре `game.commands.handlers`, проверяет аргументы команды `request.args` на переносимость через общий примитив `core:module.runtime.portable_value` (DLA-04, ADR-0027), запускает валидаторы до открытия окна мутации, исполняет обработчик внутри `mutation_window` с делегированием доменным методам сущностей/Gameplay Services и возвращает структурированный результат `{ ok = true, value = ... }` либо отказ `{ ok = false, error = { code = "..." } }`. При успешном коммите автоматически вызывается зарегистрированный источник презентации (`game.presentation.resolve()`), перестраивая желаемое состояние активного экрана без участия геймплейного кода. Цепочка обработчиков и обход массива отсутствуют.

Command handler не вызывает другой handler synchronously; вложенный вызов `dispatch` отклоняется типизированной ошибкой `CommandDispatchReentrant`. Отложенные команды ставятся в очередь `game.commands.enqueue` (также валидирующую аргументы через `portable_value`) и исполняются последовательно после завершения текущего цикла событий.

## Command validators

Validators заменяют cancellable `before_*` events.

- Read-only state/repository/payload.
- No mutation, event emission, presentation effect или external operation.
- Возвращают allow либо typed refusal.
- Stable order: priority, package load order, registration order.
- Core и mods регистрируются до registry freeze.
- Exception/error validator-а — runtime fault, не normal refusal.

Registry (`game.commands.validators`, `registry.register(id, validator_impl, options)`/`freeze()`/`ordered()`) реализован — [LuaRuntimeContract](LuaRuntimeContract.md#gamecommandsvalidators-gew-01) (GEW-01).

Запуск (GEW-02) реализован в `core:module.runtime.command_dispatcher`: перед шагом 4 Lifecycle (`Invoke one handler`) диспетчер вызывает `game.commands.validators.ordered()` целиком, вне `mutation_window.execute_in_window` — окно ещё не открыто, поэтому попытка валидатора записать в `game.state` отклоняется тем же `MutationWindowClosed`, каким отклонялась бы прямая мутация вне handler-а; отдельного permission-enforcement кода не требуется. Каждый validator получает read-only `{ state, repository, payload, command_id }` и возвращает `true` (allow) либо `false, refusal` (typed refusal — становится `command_result` без вызова handler-а, dispatch завершается успешно). Uncaught error внутри `validate()` (включая попытку мутации) пробрасывается через `pcall` как Lua error и становится `LuaDispatchError` — сбой самого dispatch, отличимый от typed refusal по возвращаемому host-результату (`FRuntimeSession::DispatchCommand` возвращает `false`+`Fault`, а не `true` с `ok=false`).

Семантика отказа (GEW-03) нормализуется одной функцией `normalize_refusal(refusal)` внутри `command_dispatcher.lua`: `refusal.code` обязан быть canonical Stable ID kind `error` (проверяется `stable_id.is_kind`), иначе это баг валидатора, а не typed refusal — `normalize_refusal` сама бросает `InvalidValidatorRefusal`, и через тот же `pcall`/`LuaDispatchError` путь становится fault, а не тихо пропущенным некорректным отказом. Отсутствующий `refusal.params` по умолчанию становится `{}`; отсутствующий `refusal` целиком (validator вернул только `false`) даёт `core:error.command.validation_refused`. Итоговый `command_result.error` всегда `{ code = "core:error....", params = {} }`. Первый отказавший validator останавливает цепочку: `run_validators` возвращает управление сразу после первого `false`, остальные validators не вызываются.

## Mutation authority

Canonical state изменяется только пока исполняется command handler. Это mutation window: вне его любое изменение `game.state` является contract violation и обязано обнаруживаться тестом, а не только review.

Инвариант временной, а не структурный. Внутри окна допустимы как Gameplay Services, так и доменные методы runtime-объектов (например, Actor API): различие между ними — вопрос декомпозиции, а не прав. Gameplay Service используется для workflow, затрагивающего несколько сущностей или подсистем; доменный метод — для локальной операции над одной сущностью.

Event handlers, UI controllers и technical-input handlers могут enqueue command, но не меняют canonical state напрямую.

Gameplay Service обязан:

- проверять локальные invariants;
- не выполнять filesystem/UE calls;
- возвращать structured result;
- enqueue facts через текущий command context;
- не публиковать events до command commit.

## Designer authoring command layer

Для игрового кода и модов ядро предоставляет авторский слой команд (`core:module.authoring.commands`, `core:module.authoring.context`, `core:module.authoring.tagged_ref`, [ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)):

1. **Дескриптор и прокси `commands` (`DLA-05`)**:
   - `local M = authoring.gameplay(package_id)` создаёт авторский контекст.
   - `M.commands[key] = function(...) ... end` накапливает объявления команд при загрузке модуля.
   - `__newindex` принимает только функцию, отклоняет повторные объявления того же ключа (`CommandAlreadyDefined`) и объявления после завершения фазы регистрации (`CommandDeclarationAfterFreeze`).
   - `__index` по неизвестному ключу во время исполнения бросает ошибку `UnknownCommandKey` (никогда не возвращает `nil`).
   - Дескриптор команды стабилен для ключа: `action(M.commands.buy, ...)` корректен при создании на этапе загрузки модуля.
   - Краткий ключ канонизируется в `<package_id>:command.<key>`.

2. **Отложенная регистрация (`DLA-06`)**:
   - На фазе `register` вызывается `M.register(ctx)`, который детерминированно сортирует команды по `command_id`, оборачивает обработчики и регистрирует их в каноническом реестре `game.commands.handlers.register`.
   - После этого прокси замораживается (`is_frozen = true`).

3. **Канонизация аргументов и помеченные ссылки (`DLA-07`)**:
   - На границе вызова команды высокоуровневые объекты разворачиваются в переносимые помеченные ссылки:
     - `ActorWrapper` $\rightarrow$ `{ __gv2_ref = "instance", id = ... }`
     - `DefinitionHandle` $\rightarrow$ `{ __gv2_ref = "definition", id = ... }`
   - Скаляры (строки, совпадающие со Stable ID, числа, булевы флаги) остаются неизменными.
   - На входе в тело обработчика помеченная ссылка превращается в свежую disposable-обёртку, а обычная строка остаётся строкой.
   - Аргументы проверяются примитивом `portable_value.validate`.
   - Конструктор `M.action(cmd_desc, ...)` канонизирует аргументы по тому же правилу, формируя DTO семантического действия `{ command_id = ..., args = ... }`.

4. **Семантика `fail()` и правило мутации (`DLA-08`)**:
   - `M.fail(key, params)` формирует типизированный отказ `{ ok = false, error = { code = "<package_id>:error.<key>", params = ... } }`.
   - Если до вызова `fail()` произошла хотя бы одна мутация состояния (`mutation_window.write_revision()` увеличился относительно момента входа в команду), вызов немедленно бросает исключение `AuthoringFailAfterMutation`.

5. **Методы исполнения `:run()` и `:later()` (`DLA-09`)**:
   - `cmd:run(...)` выполняет синхронную диспетчеризацию через `game.runtime.dispatch_command` в собственном окне мутации. Попытка вызвать `:run()` из активного обработчика отклоняется ошибкой `AuthoringNestedRunDisallowed`.
   - `cmd:later(...)` помещает канонизированный переносимый DTO в очередь отложенных команд `game.commands.enqueue`.

## Runtime phases

| Phase | New command | Event enqueue | Save |
|---|---|---|---|
| Idle (`"idle"`) | Allowed | System-only | Allowed |
| ExecutingCommand (`"executing_command"`) | Queue only (синхронный вызов -> `CommandDispatchReentrant`) | Current context only | No |
| PumpingEvents (`"pumping_events"`) | Queue only (синхронный вызов -> `CommandDispatchDuringEventPump`) | Allowed | No |
| Saving (`"saving"`) | No | No | One operation |
| Failed (`"failed"`) | No (`SessionStateFailed`) | No | Recovery only |

Фазы отслеживаются в `game.runtime.phase` (GEW-09):
- Переход в `executing_command` происходит при вызове `dispatcher.dispatch()`. Попытка вложенного синхронного вызова команды отклоняется ошибкой `CommandDispatchReentrant`.
- Переход в `pumping_events` происходит во время доставки событий подписчикам. Попытка синхронного вызова команды из обработчика события отклоняется ошибкой `CommandDispatchDuringEventPump`.
- По завершении обработки команды и событий фаза возвращается в `idle`.
- Превышение лимита pump (`DEFAULT_PUMP_LIMIT = 1000`, настраивается через `game.events.set_pump_limit(N)`) или сбой обработчика события переводит `game.runtime.phase` в `failed` и выбрасывает `EventPumpLimitExceeded`, блокируя последующие вызовы команд ошибкой `SessionStateFailed`.

## Gameplay EventBus

Event IDs используют kind `event`:

```text
core:event.location.leave
core:event.location.enter
core:event.item.add
```

Event envelope создаётся и валидируется через `core:module.runtime.event_envelope` (`M.create(spec)`):
- `event_id` — обязательный canonical Stable ID категории `event` (проверяется `stable_id.is_kind`);
- `schema_version` — положительное целое число >= 1 (по умолчанию `1`);
- `payload` — immutable detached deep copy таблицы переносимых значений;
- `correlation_id` — опциональная строка;
- `causation_id` — опциональная строка;
- `source` — опциональная immutable таблица с provenance (`kind = "command"` / `"system"` и контекст);
- `sequence` / `timestamp` — опциональные числовые координаты.

Конверт и все его вложенные таблицы защищены от мутаций read-only proxy (`EventEnvelopeImmutable` при попытке записи). Payload допускает только переносимые значения (boolean, UTF-8 string, finite number, dense 1..N array, string-key map, `game.null`). Попытка передать функцию, userdata, таблицу `game.state`, доменный объект/wrapper, циклическую ссылку, не-конечное число (NaN/infinity) или смешанные ключи отклоняется ошибкой `InvalidEventPayload`.

Жизненный цикл и пост-коммит доставка (GEW-07) управляются через `core:module.runtime.event_bus` (`game.events.enqueue`/`emit`):
- Во время исполнения команды (`ExecutingCommand`) события накапливаются во временном буфере контекста команды (`current_context.pending_events`).
- **Post-Commit публикация**: если команда завершилась успешно (`ok ~= false`), диспетчер вызывает `event_bus.commit_command_context()`, переводя события в очередь доставки EventBus.
- **Откат фактов при отказе**: если команда вернула типизированный отказ (`ok == false`) или была отклонена валидатором, диспетчер вызывает `event_bus.rollback_command_context()` — все накопленные факты отбрасываются и не доставляются.
- **Сброс при сбое**: при возникновении ошибки исполнения (`LuaDispatchError`/fault) вызывается `event_bus.discard_command_context()` — события отбрасываются без публикации.
- **Запрет публикации в обход диспетчера**: вызов `game.events.enqueue` вне активного контекста команды или фазы pump отклоняется ошибкой `EventEnqueueOutsideCommandContext`. Валидаторы исполняются до открытия контекста команды и не могут публиковать события.

- Event доставляется только после successful command commit.
- Queue FIFO; events, созданные handler-ами, обрабатываются breadth-first.
- Subscriber order: priority, package load order, registration order.
- Подписки регистрируются на фазе `register` (`game.events.subscribers.register(id, event_id, handler, options)`) и замораживаются вместе с остальными реестрами (GEW-10). Декларативные фильтры отсутствуют: условия проверяются императивно внутри обработчика.
- В авторском слое ([ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)) публикация событий выполняется через `mod.emit(name, payload)`, который разворачивает переданные обёртки сущностей в канонические помеченные ссылки (`__gv2_ref`) и ставит событие в очередь. Подписка `mod.on(name, handler)` откладывает регистрацию до фазы `register` и автоматически регидрирует помеченные ссылки обратно в свежие обёртки сущностей при вызове обработчика. Подписчик получает свежую обёртку, читающую **текущее committed состояние** игры.
- Обработчик события исполняется при **закрытом mutation window** (GEW-11). Прямая запись в `game.state` отклоняется ошибкой `MutationWindowClosed`. Обработчик может читать `state`/`repository`, ставить в очередь новые события (`game.events.enqueue`) и отложенные команды (`game.commands.enqueue`).
- Отложенные команды (`game.commands.enqueue`) накапливаются в очереди ограниченной ёмкости (`MAX_COMMAND_QUEUE_SIZE = 100`) без расхода sequence при переполнении (`CommandQueueFull`). Они исполняются последовательно после завершения текущего pump, каждая в собственном отдельном `mutation_window` со своими валидаторами, handler и pump собственных событий (GEW-12).
- Pump имеет configured limit (`DEFAULT_PUMP_LIMIT = 1000`); limit breach переводит session в `Failed`.

## Error semantics

- Validation/refusal: state unchanged, session остаётся Ready.
- Handler fault до mutation: command fails; session policy может вернуть Failed без corrupted state.
- Handler fault после mutation began: `InvariantViolation`, session `Failed`, no rollback.
- Event handler fault: originating command остаётся committed, current pump прекращается, remaining events discard, session `Failed`.
- UI сохраняет successful command result и отдельно показывает system failure/recovery surface.

## Resource-dependent command

Command не ждёт async UE operation. Если обязательный resource не prepared, validator возвращает typed `resource_not_ready`. Presentation controller запускает prepare operation; completion приходит как technical input и повторно enqueue-ит command с актуальной identity. Commit выполняется только после повторной полной validation.

## Conformance

Tests покрывают schema rejection, validated UI source construction, validator order/refusal, no-mutation-on-failure, single handler, queued nested commands, commit-before-events, FIFO/breadth-first ordering, handler order, event pump limit, post-mutation fault, event fault и resource prepare retry.
