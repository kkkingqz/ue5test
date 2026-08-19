---
title: Lua Runtime Contract
status: normative
version: 2.14
updated: 2026-08-18
depends_on:
  - StableIDSpecification.md
decisions:
  - ../ADR/0004-lua-state-mutation.md
  - ../ADR/0005-value-only-async-boundary.md
  - ../ADR/0007-lua-module-environment.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0025-lua-module-replacement-and-export-freezing.md
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0028-simplified-authoring-surface.md
  - ../ADR/0031-entity-authoring-extensions.md
---

# Lua Runtime Contract

> **Владеет:** жизненным циклом VM, загрузчиком модулей, фасадом `game`, моделью значений и marshalling на границе.
> **Не владеет:** правилами геймплея, формой состояния и семантикой команд — они в соответствующих contracts.
> **Инварианты:** [INV-006](Invariants.md), [INV-007](Invariants.md), [INV-008](Invariants.md)
> **Реализация:** `Source/GV2RuntimeCore/`, `Scripts/runtime/`, `Scripts/bootstrap/`.
> **Проверки:** `RunLuaMarshallerConformance`, `RunLuaRepositoryConformance`, `Tests/Lua/lifecycle/`.

Lua — authoritative gameplay runtime. Одна main Lua 5.4 VM принадлежит одной session и исполняется только на owner thread. В UE owner thread обязан быть Game Thread; standalone worker использует свой thread. Exact Lua patch закрепляется build manifest-ом.

Runtime source закреплён в repository как Lua 5.4.8. Build обязан проверять `LUA_VERSION_RELEASE_NUM == 50408`; использование system-installed Lua library запрещено. Upstream source, checksum, license location, excluded sources и локальные compiler adaptations фиксируются в `BuildManifest.json` рядом с vendored source.

## Invariants

- `lua_State*` не покидает LuaRuntime integration layer.
- Одновременное исполнение Lua entry points запрещено.
- Каждый C++ → Lua call protected и восстанавливает stack top.
- Synchronous re-entry запрещён.
- C++/Lua boundary принимает только schema-defined DTO, Stable IDs и opaque operation IDs.
- Данные пересекают boundary минимальным возможным представлением (ADR-0020): скаляр вместо структуры, ID вместо объекта, непрозрачные байты вместо разобранного дерева.
- Canonical gameplay-state boundary не пересекает ни при save, ни при диагностике, ни при измерении. Host получает от Lua только непрозрачные bytes и скаляры.
- UObject, pointers, functions, threads, userdata и metatables не пересекают boundary.
- Lua source/module graph change требует full session restart.
- Portable runtime sources не включают UObject, `FText`, UE containers или Presentation implementation.
- Active session удерживает pinned immutable repository snapshot (`FRepositoryReadHandle`); невалидный handle отклоняется до создания Lua VM (`RepositoryNotReady`); teardown (`Stop()`) освобождает handle; последующий publish в Application layer не меняет snapshot активной сессии.

## Standard libraries

Разрешены безопасные части `base`, `table`, `string`, `math`, `utf8`. Не открываются:

- `io`, `os`, `debug`, `coroutine`;
- `load`, `loadfile`, `dofile`, `package.loadlib`;
- public filesystem searchers;
- Lua bytecode из packages;
- `math.random`/`math.randomseed`.

Это portability boundary, а не security sandbox.

## Module loader

- `module_id` — canonical Stable ID kind `module`.
- Manifest объявляет module dependencies; hidden dependencies запрещены.
- Dependency graph проверяется на missing dependencies, duplicates, cycles и unreachable modules до инициализации первого gameplay module на объединённом графе всех пакетов.
- Модули поставляются пакетами (`scripts/` под корнем пакета). Имя источника атрибутируется пакетом: `@<package_id>/<relative>` (например `@core/gameplay/root.lua`, `@weather_mod/gameplay/storm.lua`).
- `module_id` резолвится по цепочке провайдеров в порядке загрузки пакетов (побеждает последний провайдер).
- Замещающий модуль получает доступ к предыдущей реализации через `require_base()`; вне инициализации замещающего модуля вызов `require_base()` завершается ошибкой `LuaModuleBaseNotAvailable` ([ADR-0025](../ADR/0025-lua-module-replacement-and-export-freezing.md)).
- Core modules загружаются первыми, затем mods в resolved order.
- `require(module_id)` выполняет source один раз и возвращает export table активного победителя.
- Таблицы экспорта модулей замораживаются после возврата загрузчиком (`__newindex` с ошибкой `LuaModuleExportFrozen`, `__metatable = false`); прямая мутация чужих экспортов запрещена ([ADR-0025](../ADR/0025-lua-module-replacement-and-export-freezing.md)).
- Дескриптор каждого модуля в манифесте объявляет `replaceable: boolean` (по умолчанию `false`); модули ядра запечатаны по умолчанию (`runtime/`, `boundary/`, `bootstrap/`, `presentation/`, `resources/`), модули геймплея и отладки замещаемы (`gameplay/`, `debug/`). Попытка заместить запечатанный модуль отклоняется ошибкой `LuaModuleSealed`.
- Попытка мод-пакета объявить новый модуль в чужом namespace (без предшествующего провайдера) отклоняется ошибкой `LuaModuleForeignNewId`.
- Хуки жизненного цикла (`register`, `validate_state` и др.) вызываются только у активного победителя.
- Module source path является provenance, не identity. Для незамещаемых модулей канонический `module_id` выводится из относительного пути к файлу под `scripts/` (`scripts/runtime/actors.lua` → `<pkg>:module.runtime.actors`, `scripts/authoring/gameplay.lua` → `<pkg>:module.authoring.gameplay`). Замещаемый модуль (`replaceable: true`) обязан объявлять свой целевой `module_id` явно в метаданных пакета ([ADR-0028](../ADR/0028-simplified-authoring-surface.md)).
- Зависимости модулей выводятся статическим анализом литеральных вызовов `require("...")`. Динамический вызов `require(variable)` в автообнаруживаемых модулях запрещён и отвергается диагностикой `DynamicRequireDisallowed`. Отношения замещения (`require_base()`) не являются литеральным `require` и объявляются в метаданных пакета (`package.json5` под секцией `modules`).
- Late registration после registry freeze запрещена. Для подписчиков (`core:module.runtime.subscriber_registry`) это структурная гарантия, не только конвенция: операция сброса (`clear()`), единственная способная разморозить registry, не является методом объекта, доступного через `game.events.subscribers` — она возвращается `create_registry()` отдельным `admin`-handle, который `event_bus.lua` держит как module-local upvalue и никогда не публикует на `game`. Ни один gameplay-модуль или мод не может её достать через глобальный `game`; `event_bus.with_isolated_subscribers(fn)` (изолированная область для `Tests/Lua/` спек) доступен только модулям, явно объявившим `core:module.runtime.event_bus` зависимостью — это узкая, видимая в манифесте поверхность, а не открытая через `game.events`. Операции, которая очищает реестр **без** восстановления, не существует: очистка размораживает реестр и вместе с ним стирает подписчиков, зарегистрированных пакетами на фазе `register`, а оставленная так — ломает следующую по порядку спеку, и отказ проявляется не там, где его причина. Поэтому очистка доступна только внутри области, которая всегда возвращает прежний набор вместе с флагом заморозки, в том числе при ошибке в теле.

Манифест ядра располагается по пути `bootstrap/manifest.lua` (или `manifest.lua`) в каталоге `scripts/` пакета `core`. Мод-пакеты и игровые пакеты генерируют канонический манифест `scripts/manifest.lua` автоматически инструментом сборки (`Tools/Content/generate_manifest.py`). Манифест возвращает data-only table `{ entry_module_id, modules[] }` и сам не является module. Каждый descriptor обязан содержать canonical `module_id`, package-relative `source`, полный список direct `dependencies` и опциональное булево поле `replaceable`.

UE и headless hosts рекурсивно собирают UTF-8 `.lua` files под `scripts/` каждого подключённого пакета через `GV2ContentHostSupport::DiscoverPackagesScripts`, сортируют только для deterministic ingestion и передают полный source set в portable runtime с именами `@<package_id>/<relative>`. File order не является load semantics. Runtime строит цепочки провайдеров, вычисляет эффективные зависимости как объединение прямых зависимостей всей цепочки и проверяет граф на ошибки (missing source, unlisted source, duplicate ID внутри одного пакета, dependency cycle, sealed module override, foreign new ID, unreachable module) до исполнения первого модуля.

Loader выполняет modules в порядке топологической сортировки зависимостей. Для каждого замещающего модуля цепочки загрузчик временно предоставляет предыдущий замороженный экспорт через `require_base()`. Возвращённая таблица экспорта каждого провайдера оборачивается в immutable proxy (`__index`, `__newindex` с ошибкой `LuaModuleExportFrozen`, `__pairs`, `__metatable = false`). В глобальном реестре `GV2.LoadedModules` регистрируется только активный победитель цепочки. `require(module_id)` разрешён во время module initialization только для direct dependency текущего descriptor; это делает hidden import deterministic manifest violation. Module сохраняет imports в lexical locals и не вызывает `require` из runtime handlers.

## Source layout and dependency direction

```text
Scripts/
  bootstrap/       manifest и composition root
  boundary/        fixed host entry points и DTO ingress/outbound adapters
  runtime/         portable kernel: Stable ID, dispatcher, lifecycle, EventBus
  gameplay/        canonical state, feature commands/services/queries
  presentation/    desired UI/presentation builders; no gameplay mutation
  resources/       resource_id metadata/intents; no physical media loading
  debug/           development content через обычные runtime contracts
```

`bootstrap/main.lua` является единственной composition root. `boundary` может зависеть от runtime/application services, но gameplay, presentation и resources запрещено импортировать `boundary`. Host ports передаются им composition root-ом; direct C++/UE call из gameplay запрещён. Presentation читает state/query views и не меняет canonical state. Debug modules не регистрируют test-only host entry points.

Gameplay группируется по feature (`gameplay/inventory/commands.lua`, `service.lua`, `queries.lua`), а не в repository-wide файлы всех commands/services. Module graph не обязан содержать пустые feature modules до появления concrete mechanics.

`resources` хранит только canonical `resource_id`, строит value-only reference/prepare DTO и обрабатывает typed TechnicalInput. Asset locator, streaming handle, decoded media и locale resolution принадлежат host. Текущий `resources/service.lua` реализует canonical reference и deterministic deduplicated prepare request; запуск async operation появится только через injected boundary port. `TextSpec` принадлежит presentation/localization flow, а не media resources.

## Global environment

Все modules используют одну VM и общий runtime `_G`, но module source не создаёт globals. Standard modules хранят private values в lexical locals и возвращают export table.

Runtime-owned globals: `game`, safe standard libraries и loader functions. Extension data/functions публикуются только под:

```lua
game.mods[mod_id]
```

Например `game.mods.weather_mod`. Замена runtime-owned field — `GameApiFieldConflict`.

### Authoring modules and `_ENV` (ADR-0028)

Файлы подкаталога `scripts/authoring/` пакетов (или модули с флагом `authoring: true`) выделяются в класс **authoring-скриптов** ([ADR-0028](../ADR/0028-simplified-authoring-surface.md)):
- Загрузчик создаёт дескриптор модуля `mod = authoring.gameplay(package_id)` до исполнения файла и устанавливает лексическое окружение `_ENV` с предварительно связанными именами (`commands`, `player`, `world`, `def`, `location`, `actor`, `actors`, `fail`, `emit`, `on`, `text`, `button`, `action`, `show_screen`).
- `_ENV` связан через `__index` с `_G` для доступа к стандартным функциям (`pairs`, `ipairs`, `type`, `assert`, `math`, `string`, `table`, `error`, `pcall`).
- Запись в глобальные переменные внутри authoring-скрипта запрещена: попытка присваивания в `_ENV` завершается ошибкой `AuthoringGlobalWriteDisallowed`.
- Директива `return M` не требуется: возвращаемое значение чанка игнорируется, экспортной таблицей становится созданный загрузчиком дескриптор `mod`.
- Обработчики команд поддерживают неявный успех: `return nil` расценивается как `{ ok = true }`, `return val` — как `{ ok = true, value = val }`. Отказом считается исключительно объект, возвращённый функцией `fail(...)`.

Окружение authoring-модулей не является security sandbox и предназначено для синтаксической чистоты правил игры. Модули программиста (`scripts/runtime/`, `scripts/services/`) продолжают использовать стандартное окружение и `require`.

## Stable `game` façade

```lua
game = {
  state = <canonical mutable table>,
  repository = <read-only query service>,
  commands = <dispatcher service>,
  events = <post-commit EventBus service>,
  instances = <instance resolver>,
  services = <gameplay services>,
  ui = <presentation intent service>,
  bridge = <typed UE operation service>,
  random = <deterministic PRNG service>,
  time = <gameplay clock>,
  log = <structured logger>,
  runtime = <read-only session metadata>,
  mods = <module extension table>,
  null = <explicit-null sentinel>,
}
```

Список полей фасада закрыт. Registry конкретного вида runtime instances не добавляет новое поле верхнего уровня, а живёт под `game.instances` (например, `game.instances.actors`); иначе каждая следующая категория сущностей расширяет фасад. Registry отвечает за identity, lookup, creation, removal, deterministic enumeration и выдачу runtime-объекта, но не за gameplay rules.

`game.repository` — единственное имя repository API; alias `game.data` отсутствует. Таблица защищена от подмены и расширения (`__newindex`, `__metatable = false`).

API предоставляет ровно четыре функции — `get`, `require`, `list`, `exists`. `get` никогда не выбрасывает Lua error и возвращает вторым значением typed table с полем `code`; `require` выбрасывает Lua error, первым токеном которого является тот же стабильный code.

Возвращаемые таблицы являются detached deep copy и содержат только валидированные payload-поля (`id`, `data`, `tags`, `deprecated`, `extensions`). Provenance, package identity, load index, shadowed providers и пути к исходным файлам через boundary не проходят: gameplay-правила не должны зависеть от раскладки файлов и порядка загрузки пакетов, а аудит источников принадлежит authoring-слою. Полные семантика вызовов, коды ошибок и обоснование изоляции provenance — в [GameDataRepository Contract](GameDataRepositoryContract.md).

### `game.commands.handlers` (CHR-01..07)

`game.commands.handlers` — реестр обработчиков команд (`core:module.runtime.handler_registry`). Обработчики регистрируются модулями ядра и модов на фазе `register` и связываются с точным идентификатором команды `command_id` (Stable ID категории `command`).

Публичный API реестра:
- `register(command_id, handler_fn, options)` — регистрация обработчика команды. `command_id` обязан быть валидным Stable ID категории `command` (проверяется `stable_id.is_kind`, иначе ошибка `InvalidCommandId`), `handler_fn` — исполняемой функцией (иначе `InvalidCommandHandler`). Возвращает зарегистрированную функцию.
- `get(command_id)` — возвращает зарегистрированную функцию обработчика или `nil`.
- `exists(command_id)` — возвращает `true`, если обработчик зарегистрирован.
- `ids()` — возвращает детерминированный отсортированный массив всех зарегистрированных `command_id`.
- `freeze()` / `is_frozen()` — заморозка реестра в конце фазы `register`. Регистрация после freeze отклоняется ошибкой `CommandHandlerRegistryFrozen`.

Правила перекрытия и защита целостности:
- **Отказ на дубликат**: повторная регистрация того же `command_id` без явного флага `override` отклоняется ошибкой `CommandHandlerDuplicateRegistration` и блокирует запуск сессии. Правило «поздний пакет побеждает» без явного указания для команд не применяется.
- **Явный override**: замена существующего обработчика разрешена только при передаче `options.override = true`.
- **Защита от ложного override**: передача `options.override = true` для незарегистрированного ранее `command_id` отклоняется ошибкой `CommandHandlerOverrideMissing`.
- **Защита от прямой мутации**: прямая запись полей в таблицу реестра запрещена метатаблицей (`CommandHandlerRegistryDirectAssignmentDisallowed`).

При диспетчеризации (`command_dispatcher.dispatch`) обработчик находится за один lookup `game.commands.handlers.get(request.command_id)`. При отсутствии обработчика диспетчер возвращает типизированный отказ `{ ok = false, error = { code = "core:error.command.unknown", params = { command_id = request.command_id } } }` без открытия окна мутации и без доставки событий.

### `game.commands.validators` (GEW-01)

`game.commands` — зарезервированное поле фасада для command dispatcher service; `validators` — реестр валидаторов команд. Регистрация выполняется на фазе `register` (`registry.register(id, validator_impl, options)`, `options.priority` — опциональный целочисленный приоритет, по умолчанию `0`) и закрывается вместе с остальными registries тем же host-side freeze шагом, что и `game.services`. Id обязан быть canonical Stable ID kind `validator`; поздняя регистрация после freeze — ошибка.

Итоговый порядок исполнения (`registry.ordered()`): priority по возрастанию, затем package load order, затем registration order — [Commands and Events](CommandsAndEvents.md) "Command validators". Один global `register`-проход над уже разрешённым module `LoadOrder` (core-модули, затем mods) делает package load order и registration order одной composite-последовательностью: отдельного numeric package-index не вводится, пока не появится второе измерение (реальные mods).

Запуск (GEW-02): `core:module.runtime.command_dispatcher` вызывает `registry.ordered()` целиком до открытия mutation window (до шага 4 Lifecycle, `Invoke one handler`). Каждый validator получает read-only `{ state, repository, payload, command_id }` (`state`/`repository` — те же живые ссылки, что видит handler, без отдельного copy) и возвращает:

- `true` — allow, handler выполняется как обычно;
- `false, { code = "core:error....", params = {} }` — typed refusal: handler пропускается, `{ ok = false, error = refusal }` становится command result, dispatch завершается успешно (не fault);
- ничего не возвращает, а вместо этого бросает Lua error (в том числе попытка `state.x = ...`, которую `mutation_window` отклоняет, поскольку окно ещё не открыто) — ошибка пробрасывается через `pcall` наружу dispatch-а и становится `LuaDispatchError`: host получает `DispatchCommand(...) == false` + `Fault`, что явно отличимо от typed refusal (`DispatchCommand(...) == true`, `ok=false` в command result).

Первый отказавший validator прекращает цепочку (остальные не вызываются). Отдельного enforcement-кода на запрет мутации/событий/операций нет: mutation window уже закрыт для любого кода вне handler-а, а попытка вызвать `game.events.enqueue` вне контекста команды отклоняется `EventEnqueueOutsideCommandContext`.

GEW-03 фиксирует итоговый refusal envelope: `{ code = "core:error....", params = {} }`, `code` — обязательно canonical Stable ID kind `error`. Validator может вернуть `false` без второго значения (даёт `core:error.command.validation_refused`), `false, { code = "..." }` (params по умолчанию `{}`) или `false, { code = "...", params = {...} }`. Refusal с невалидным/отсутствующим `code` или нетабличным `params` — баг validator-а: `normalize_refusal` бросает `InvalidValidatorRefusal`, что становится тем же `LuaDispatchError`, каким становится любая другая ошибка внутри `validate()`.

### `game.events` (GEW-06, GEW-07, GEW-08, GEW-09, GEW-10, GEW-11)

`game.events` — сервис публикации неотменяемых gameplay-фактов (`core:module.runtime.event_bus`).
- `enqueue(spec)` / `emit(spec)` — постановка события в очередь текущего контекста команды (`ExecutingCommand`) или очереди pump (`PumpingEvents`). Принимает spec или готовый конверт (`core:module.runtime.event_envelope`). Значения валидируются на переносимость и глубоко копируются в read-only структуру. Вызов вне активной команды или pump отклоняется ошибкой `EventEnqueueOutsideCommandContext`.
- `subscribers.register(id, event_id, handler, options)` (и alias `subscribe`) — регистрация подписчика на фазе `register` (`core:module.runtime.subscriber_registry`, GEW-10). Валидирует `id` (kind `subscriber`), `event_id` (kind `event`), `handler` (функция или таблица с `handle_event`/`on_event`) и `options.priority`. Замораживается вместе с остальными реестрами (`freeze()`). Декларативные фильтры отсутствуют: условия проверяются императивно внутри обработчика.
- Обработчики событий исполняются при закрытом mutation window (GEW-11): попытка прямой записи в `game.state` отклоняется `MutationWindowClosed`. Разрешено чтение `state`/`repository`, постановка событий (`game.events.enqueue`) и отложенных команд (`game.commands.enqueue`).
- Доставка фактов происходит strictly post-commit (`commit_command_context`) по принципу FIFO и breadth-first для вложенных событий. При отказе команды (`ok == false` или отказ валидатора) все накопленные события сбрасываются (`rollback_command_context`) и не доставляются. При runtime fault события отбрасываются (`discard_command_context`).
- `set_pump_limit(limit)`, `get_pump_limit()`, `reset_pump_limit()` — управление лимитом итераций цикла pump (`DEFAULT_PUMP_LIMIT = 1000`). Превышение лимита переводит `game.runtime.phase` в `"failed"` и выбрасывает `EventPumpLimitExceeded`.
- `get_published_events()` и `clear_published_events()` — методы инспекции и очистки доставленных событий для тестов и спек.

### `game.commands.enqueue` (GEW-12)

`game.commands.enqueue(request)` — постановка команды в очередь отложенного исполнения.
- Доступна во время `ExecutingCommand` и `PumpingEvents`.
- Валидирует `request.command_id` (Stable ID kind `command`) и параметры.
- Очередь имеет фиксированную ёмкость (`MAX_COMMAND_QUEUE_SIZE = 100`). Превышение ёмкости отклоняется `CommandQueueFull` без расхода sequence.
- Отложенные команды исполняются последовательно после завершения текущего event pump, каждая в собственном изолированном `mutation_window` с полной цепочкой валидаторов, handler-а и доставки порождённых фактов. Синхронный вызов команд внутри обработчиков запрещён (`CommandDispatchReentrant`, `CommandDispatchDuringEventPump`).

## Value model

| Lua | Boundary |
|---|---|
| boolean | bool |
| UTF-8 string | string |
| integer | signed int64 |
| finite number | double |
| 1..N dense table | schema array |
| string-key table | schema map/object |
| `game.null` | explicit null |
| `nil` | absent field |

Schema различает array/map. Empty container без schema boundary не пересекает. Cycles/shared identity не сохраняются. Conversion — deep copy.

Возвращённая Lua copy остаётся обычной mutable таблицей: frozen proxy и identity guarantees отсутствуют, повторный query не гарантирует ту же таблицу.

### C++ marshalling

`FGV2LuaMarshaller` предоставляет единый C++ путь marshalling для обоих value-типов (`GV2RuntimeCore::FValue` и `GV2ContentCore::FValue`) в/из Lua: null (`game.null`), bool, signed int64, finite double, UTF-8 string, array и string-key object. Каждое пересечение boundary создаёт detached deep copy. Параллельный marshalling path отсутствует.

Marshaller обязан:

- отличать absent field (`nil`) от explicit null (`game.null`);
- сохранять канонический порядок ключей объектов при конвертации;
- отклонять sparse/mixed tables, cycles, excessive depth/size (`MaxDepth = 64`, `MaxNodes = 10000` -> `PortableValueLimitExceeded`), пустые имена полей (`PortableValueFieldInvalid`) и non-finite numbers (`PortableValueNonFinite`);
- не сохранять Lua stack indices, registry references или userdata внутри DTO;
- не использовать JSON string serialization как runtime call protocol;
- преобразовывать Blueprint-facing structs в boundary values только через declared schema adapters.

`FText` не пересекает Lua boundary. Lua публикует `TextSpec`; Presentation выполняет localization/formatting и только после этого передаёт `FText` Widget-у.

## Canonical state

`game.state` — обычная mutable Lua table, содержащая шесть канонических секций: `meta`, `actors`, `item_instances`, `world`, `quests`, `mods`. Список канонических секций объявлен ровно один раз — в модуле `core:module.runtime.state_validator` (`Scripts/runtime/state_validator.lua`), который является единственным владельцем правил валидации дерева состояния (`validate_state_tree`), структуры секции `meta` и конструирования начального дерева (`create_empty_canonical_state`). Host (C++) не дублирует список секций или правила валидации и обращается к модулю `state_validator`.

Допустимые значения: `string`, `bool`, `int64`, finite `double`, dense arrays, string-key maps и `game.null`. Каноническое дерево оборачивается прокси-защитой `mutation_window.guard_state`. Модуль `core:module.runtime.mutation_window` ведёт монотонный счётчик `write_revision`, увеличиваемый в `__newindex` guarded-прокси при любой мутации (прямое присваивание, доменный метод, Gameplay Service, `table.insert`/`table.remove`). Функция `unwrap_state` исключена из публичного экспорта модуля (`ADR-0027`): ни один геймплейный модуль, доменный метод или сервис не получает прямого доступа к сырой канонической таблице состояния.

Нормативно state меняется только внутри active Command Handler через доменные методы сущностей или зарегистрированные Gameplay Services. Любая попытка мутации `game.state` вне активного окна исполнения команды отклоняется ошибкой `MutationWindowClosed: cannot mutate canonical state outside of active command handler`. Переносимость значений аргументов команд, очереди и событий проверяется единым модулем ядра `core:module.runtime.portable_value`.

State не содержит functions, metatables, userdata, UObject, operation handles, non-finite numbers (NaN/inf), sparse arrays, cycles, shared references, queues, subscriptions или definition copies. Валидация выполняется целиком в Lua VM перед переходом в `Ready` через `state_validator.validate_state_tree`.

State не покидает VM в разобранном виде. Сериализация, integrity check и версии секций принадлежат Lua; host предоставляет только slot-scoped byte storage ([Canonical State and Save](CanonicalStateAndSave.md)). Для наблюдаемости состояния в тестах и run digest Lua регистрирует fixed outbound entrypoint `game.runtime.get_canonical_state_hash`, а host получает один скаляр через узкий accessor `FRuntimeSession::GetCanonicalStateHash()`. Отсутствие `game.state` возвращает пустую строку `""` без генерации ошибки; само дерево состояния boundary никогда не пересекает (ADR-0020, ADR-0021).

Uncaught error после начала mutation не запускает универсальный rollback. Session переходит в `Failed`; поэтому validation выполняется до первой mutation.

## Runtime instances

- Persistent state хранит `instance_id`, `definition_id` и explicit instance state.
- Runtime wrappers/methods/metatables перестраиваются dynamically и не сохраняются в canonical state.
- **Actor Registry (`game.instances.actors`)**:
  - `game.instances.actors.register_type(discriminator, decorator)` регистрирует фабрику-декоратор обёртки для указанного `discriminator` на фазе `register` ([ADR-0026](../ADR/0026-core-and-gameplay-ownership.md)). Регистрация замораживается после фазы `register` (`ActorTypeRegistryFrozen`); повторная регистрация отклоняется (`ActorTypeDuplicateRegistration`); `decorator` обязан быть функцией, возвращающей таблицу (`InvalidActorDecorator`, `ActorDecoratorInvalid`).
  - `game.instances.actors.types()` возвращает отсортированный детерминированный список зарегистрированных дискриминаторов.
  - `game.instances.actors.get(instance_id)` возвращает disposable `ActorWrapper` либо `nil`.
  - `game.instances.actors.exists(instance_id)` возвращает boolean.
  - `game.instances.actors.create(definition_id, overrides)` аллоцирует `instance_id` через `instance_allocator` и создаёт экземпляр в `state.actors`.
  - `game.instances.actors.remove(instance_id)` удаляет актора, предварительно проверяя отсутствие зависимых ссылок (например, `owner_id` в `state.item_instances`).
  - `game.instances.actors.ids(filter_fn)` возвращает детерминированный отсортированный список идентификаторов с возможностью фильтрации по дискриминатору/предикату.
  - `game.instances.actors.player()` возвращает wrapper для текущего игрока (`state.meta.player_actor_id`).
- **Disposable Actor Wrapper & Decorator Extension**:
  - Ядро формирует базовую обёртку вокруг сырой таблицы `actor_state`, предоставляющую доступ к полям состояния, `instance_id`, `definition_id`, `discriminator` и вспомогательному методу `get_state()`.
  - Идентификационные поля (`instance_id`, `definition_id`, `discriminator`) защищены от перезаписи и переопределения (`ActorDiscriminatorImmutable`).
  - Пакет регистрирует декоратор `decorator(base) -> wrapper`, надстраивающий доменные методы сущности (например `is_player()`, `get_gold()`, `add_gold(amount)`) через `{ __index = base }`.
  - Незарегистрированный `discriminator` отклоняется типизированной ошибкой (`ActorTypeNotRegistered`), за исключением переходного списка известных нерегистраций ядра.
  - Сам wrapper никогда не попадает в `game.state` и не кэшируется между вызовами.
- **World Domain Object (`game.instances.world`)** (план [GameplayEventsAndWorld](../Plans/Archive/GameplayEventsAndWorld/README.md), GEW-04):
  - Мир — singleton runtime instance, а не registry: `game.instances.world` — функция (`core:module.runtime.world`), а не таблица с методами `get`/`create`/`remove`.
  - `game.instances.world()` возвращает свежий disposable wrapper над `state.world` при каждом вызове; wrapper не кэшируется, повторный вызов не гарантирует ту же таблицу.
  - `__index`/`__newindex` wrapper-а делегируют напрямую в `state.world`; отдельного mutation-window enforcement wrapper не вводит, поскольку `state.world` уже является guarded-прокси через `game.state`.
  - Wrapper имеет metatable, поэтому существующая generic-проверка `state_validator.lua` (`getmetatable(val) ~= nil` → error) отклоняет любую попытку сохранить его внутрь canonical state; отдельного правила валидации для мира не потребовалось.
  - **Текущая локация (`state.world.current_location_id`)** (GEW-05): опциональное поле, значение — Stable ID kind `location`. `state_validator.lua` валидирует его отдельным правилом (по аналогии с `meta.player_actor_id`): grammar (строка), kind `location` через `stable_id.is_kind` (проверяется до обращения к repository — Stable ID неверного kind отклоняется без repository lookup), и pinned-repository resolution через `game.repository.exists` (dangling-ссылка на несуществующую локацию отклоняется). Поле читается через `game.instances.world().current_location_id` — обычное чтение таблицы, mutation window не требуется.

## Gameplay Services

- Реестр `game.services` (`core:module.runtime.service_registry`) предоставляет доступ к чистым Lua-сервисам предметной области.
- Сервисы регистрируются во время lifecycle-фазы `register` через `game.services.register(id, service_impl)`.
- По завершении фазы `register` реестр замораживается (`freeze()`). Поздняя регистрация сервиса после freeze отклоняется с ошибкой `ServiceRegistryFrozen`.
- Прямая запись в таблицу реестра запрещена (`ServiceRegistryDirectAssignmentDisallowed`).
- Gameplay Services предназначены для выполнения многосущностных workflow (например, торговля, передача предметов между акторами, квестовые цепочки), возвращают структурированный результат `{ ok = true, value = ... }` или `{ ok = false, error = { code = "..." } }`, не вызывают filesystem/UE API и не подменяют доменные методы единичных сущностей.

## Semantic Actions

- Реестр `game.actions` (`core:module.runtime.action_registry`) обеспечивает декларативное связывание семантических действий (`kind: "action"`) с целевыми командами геймплея (`kind: "command"`).
- Связывание действий производится во время фазы `register` через `game.actions.bind(action_id, command_binding)` (или через прокси авторского окружения `actions[key] = binding`).
- По завершении фазы `register` реестр замораживается (`freeze()`). Попытка привязки действия после freeze отклоняется с ошибкой `ActionRegistryFrozen`.
- Попытка повторной привязки уже зарегистрированного действия отклоняется с ошибкой `DuplicateActionBinding`.
- Запрос непривязанного действия через `game.actions.require(action_id)` завершается типизированной ошибкой `ActionNotBound`.
- В авторском слое презентации помощник `action(action_id, args)` прозрачно разрешает семантическое действие в целевой `command_id` с объединением аргументов.

## Entity Extensions (`game.entity_extensions`)

- Реестр `game.entity_extensions` (`core:module.runtime.entity_extension_registry`) управляет централизованной регистрацией, валидацией конфликтов и композицией методов сущностей (`Actor`, `Location`, `Quest`, `Item` и пользовательские PascalCase-типы) ([ADR-0031](../ADR/0031-entity-authoring-extensions.md)).
- Методы объявляются в авторских модулях естественным синтаксисом `function EntityKind:method_name(...)`. Метаметод `__newindex` прокси прототипа в `_ENV` делегирует регистрацию в `game.entity_extensions.register(source_module, package_id, entity_kind, method_name, fn)`.
- Попытка повторного объявления одного и того же метода из разных пакетов или модулей отклоняется с ошибкой `entity_extension.method_conflict`.
- На фазе `register` вызовом `freeze()` компилируется неизменяемая `effective method table` (`get_effective_methods(entity_kind)`). Поздняя регистрация после `freeze()` отклоняется с `EntityExtensionRegistryFrozen`, прямая модификация скомпонованной таблицы — с `EffectiveMethodTableFrozen`.
- Доступ к методам осуществляется прозрачно через обёртки экземпляров (`ActorWrapper`) и обёртки определений (`wrap_definition`), без необходимости ручного создания функций-декораторов `register_type`.
- При вызове `fail(key, params)` из метода сущности код ошибки автоматически атрибутируется пространством имён пакета, в котором метод был **объявлен** (`<declaring_package_id>:error.<key>`).

## Designer authoring layer

Для упрощения написания игрового кода и модов ядро предоставляет авторский слой ([ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)):

- **Модули ядра**: `core:module.authoring.context` (`Scripts/authoring/context.lua`), `core:module.authoring.commands` (`Scripts/authoring/commands.lua`), `core:module.authoring.properties` (`Scripts/authoring/properties.lua`), `core:module.authoring.tagged_ref` (`Scripts/authoring/tagged_ref.lua`), `core:module.authoring.presentation` (`Scripts/authoring/presentation.lua`).
- **Фасад пакета**: `local M = authoring.gameplay(package_id)` создаёт дескриптор авторского модуля с прокси команд `M.commands`, типизированным отказом `M.fail(key, params)` и динамическими аксессорами.
- **Динамические аксессоры**:
  - `M.player` — динамический прокси, разрешающий `game.instances.actors.player()` на каждое обращение (не кэширует обёртку).
  - `M.world` — динамический прокси к `game.instances.world()`; `world.current_location` и `world.current_location_id` являются read-only аксессорами к активному игроку (`state.world` не дублирует локацию).
  - `M.actor(name)` — аксессор уникального экземпляра по имени определения (`0` экземпляров $\rightarrow$ `ActorInstanceNotFound`, `1` $\rightarrow$ fresh wrapper, `2+` $\rightarrow$ `ActorInstanceAmbiguous`).
  - `M.actors(name)` — список свежих обёрток для всех экземпляров указанного определения.
  - `M.def.<kind>(name)` и shortcut `M.location(name)` — доступ к обёртке определения с поддержкой динамических runtime-полей и декораторов (`register_definition_type`).
- **Исполнение команд**: `:run(...)` для синхронного вызова в собственном окне мутации (вложенный вызов из активного обработчика запрещён: `AuthoringNestedRunDisallowed`) и `:later(...)` для помещения в очередь `game.commands.enqueue`.
- **Свойства и политики схем**:
  - `storage`: `"definition"` (чтение из данных определения в репозитории) или `"runtime_state"` (хранение в `game.state.definitions[def_id]`).
  - `write_policy`: `"read_only"` (запись запрещена), `"plain"` (валидация типа, ограничений, enums перед записью в состояние; отказ оставляет состояние и `write_revision` неизменными), `"managed"` (прямая запись запрещена, ошибка указывает доступные доменные операции из `operations`).
  - **Sparse-материализация**: чтение отсутствующего runtime-поля определения возвращает schema `default` (не занимая места в состоянии); любая запись материализует значение в `game.state.definitions[def_id]`; метод `:reset(field_name)` снимает override и возвращает sparse default.
  - **Фаза freeze**: проверка наличия всех объявленных в схеме доменных операций на зарегистрированных декораторах типов (`MissingDomainOperation`).
  - **Ссылки**: `ref_definition` (возвращает дескриптор/таблицу определения из репозитория) и `ref_instance` (возвращает свежую обёртку `ActorWrapper`).
  - **Ссылочная целостность**: `game.instances.actors.remove(id)` проверяет отсутствие входящих ссылок из других сущностей состояния (`ActorHasDependentReferences`).
  - **Локация**: хранится на акторе (`ref_definition<location>`), переход между локациями (операция `player:move_to(target)` слоя `textsystem`, вызываемая командой перемещения игры) обновляет локацию актора игрока.
- **События и подписчики**:
  - `M.emit(event_name, payload)` разворачивает переданные обёртки сущностей в канонические помеченные ссылки (`__gv2_ref`) и ставит событие в очередь.
  - `M.on(event_name, handler_fn)` регистрирует подписчик, который при срабатывании автоматически регидрирует помеченные ссылки в свежие обёртки сущностей, читающие **текущее committed состояние**.
- **Презентация и действия**:
  - `M.text(key, args, style)` создаёт каноническую структуру `TextSpec`.
  - `M.action(command_desc, ...)` создаёт привязку команды `{ command_id = ..., args = ... }`, запрещая произвольные замыкания (`ActionClosureDisallowed`).
  - `M.button(text_spec, action_result, key_opt)` создаёт запись кнопки, запрещая сырые строки в тексте (`RawStringDisallowed`).
  - `M.show_screen({ template, description, buttons })` валидирует спецификацию экрана и публикует экранный запрос, запрещая сырые строки в пользовательских текстах (`RawStringDisallowed`).
  - Код отказа `fail(key)` (`<pkg>:error.<path>`) строго мапится на локализационный идентификатор текста `<pkg>:text.error.<path>`.
  - Утилита `Tools/Content/collect_texts.py` выполняет статический сбор литералов `text("...")` и `fail("...")` и синхронизирует `texts.json5` и `.po` каталоги.

## Protected execution

Каждый entry point:

1. Captures stack top и execution context.
2. Sets `bExecutingLua=true`.
3. Calls `lua_pcall` с traceback handler.
4. Validates return DTO по declared schema.
5. Clears context, restores stack, sets `bExecutingLua=false`.
6. Dispatches следующий queued ingress item отдельным entry point.

Expected gameplay refusal возвращает typed Result. Wrong schema, forbidden phase и uncaught error становятся structured runtime fault.

### VM host and entry points

`GV2RuntimeCore::FRuntimeSession` — portable STL-only session-scoped façade, а его private implementation владеет VM и единолично видит Lua C API. Façade предоставляет host-ам фиксированные typed entry points для bootstrap/lifecycle, Semantic Input, direct simulation `CommandRequest`, TechnicalInput и shutdown; generic `Call(function_name, json)` отсутствует.

Portable runtime реализует создание/уничтожение VM, selective opening `base`/`table`/`string`/`math`/`utf8`, удаление запрещённых base/math functions, manifest-driven module loader и protected entry points. UE semantic input и headless `CommandRequest` сходятся во fixed Lua command dispatcher до gameplay validation.

Lua source является repository content и не встраивается строковым литералом в C++. UE adapter и headless host читают module tree как bytes и передают `FRuntimeSource[]` в `GV2RuntimeCore`. Portable core не открывает filesystem и не знает UE project path. Source name используется только как manifest locator и sanitized traceback provenance.

Текущий vertical slice использует private module `core:module.presentation.screen_requests` как Lua-side constructor/slot Screen request. C++ не вызывает этот builder и не передаёт ему параметры. Portable C++ принимает request через fixed boundary entry point `game.ui.take_pending_screen`, проверяет `screen_id` и generic `fields[]` envelopes и копирует их как `FScreenRequest`; UE schema adapters готовят typed fields без concrete field names в runtime façade. Полный layered UI-document через `game.ui.publish_snapshot` расширит route/overlay/modal lifecycle без добавления test-only entry points или замены field boundary.

`core:module.debug.start` распознаёт `core:command.debug.start` и публикует один pending value-only Screen request через presentation module. `core:module.boundary.ingress` устанавливает fixed command/Semantic Input entry points, а `core:module.boundary.outbound` — Screen outbound entry point. Debug handler является development content, но использует обычный dispatcher и не экспортируется в `game`.

UE coordinator обязан вызывать `take_pending_screen` отдельным protected entry point только после возврата `dispatch_semantic_input`. Полученный request полностью копируется до вызова Presentation sink; UMG creation и Blueprint events запрещены во время Lua execution. Headless host может отправить тот же `CommandRequest` и забрать тот же request без Unreal Engine.

UE adapter выполняет `FGV2UiControlValue → portable Value` conversion до вызова runtime. Lua C API и portable implementation не зависят от Unreal Engine.

Native bindings также фиксированы и schema-defined. Минимальная surface:

- `game.ui.publish_snapshot(snapshot)` публикует complete Presentation Snapshot;
- `game.bridge.start_operation(operation_kind, request)` запускает registered typed UE operation;
- `game.log.write(level, record)` пишет structured diagnostic.

Binding не вызывает Blueprint delegate синхронно. Outbound DTO валидируется и копируется, затем ставится в coordinator-owned outbound queue. Presentation/operation work начинается только после восстановления Lua stack и выхода из текущего entry point.

## Determinism

- `game.random` использует named PRNG streams; stream states сохраняются.
- `game.time` предоставляет gameplay clock; wall clock отсутствует в gameplay API.
- Значимый map iteration использует `game.runtime.sorted_keys(map)`.
- `pairs()` разрешён, но его order не влияет на authoritative result.
- Async completion order становится explicit ordered technical input.
- Headless timing/worker scheduling не входит в authoritative inputs.

Одинаковые repository, state, commands, technical inputs, PRNG и gameplay clock дают семантически одинаковые state и events.

## Technical ingress

Bridge operation принимает DTO и возвращает `operation_id`:

```lua
local result = game.bridge.start_operation(
  "core:operation.resource.prepare",
  { resource_ids = ids }
)
```

C++ не принимает и не хранит Lua callback. Completion создаёт DTO:

```json5
{
  operation_id: "runtime@17:42",
  operation_kind: "core:operation.resource.prepare",
  ok: true,
  value: {},
}
```

DTO попадает в bounded ingress queue, проверяется по session generation/owner/token и передаётся фиксированному runtime dispatcher. Lua может иметь transient internal map `operation_id → handler`, но function остаётся внутри Lua.

## GC и diagnostics

- Incremental GC выполняется с diagnostic per-frame budget.
- Full GC допустим после module load, restore, return to menu и перед VM destruction.
- Runtime отслеживает heap current/peak, GC duration, entry duration, allocations, registry refs и stack balance.
- Hard CPU/memory quotas не являются частью v1.

Structured fault содержит code, module ID, entry point, phase, session generation, optional operation ID, correlation ID и sanitized source trace без absolute paths.

## Conformance

Tests покрывают VM ownership, exact patch, private Lua C API, recursive source ingestion без embedded runtime literal или per-file C++ list, manifest/source uniqueness, missing/unlisted source, declared dependencies, hidden import rejection, dependency cycles/reachability, export table, restricted libraries, no module globals, game namespace conflict, DTO isolation, UTF-8/Cyrillic round-trip, null/absent, numeric bounds, sparse/cyclic/depth rejection, absence of JSON call protocol, state mutation policy, no rollback, deterministic replay, protected calls/stack restoration, deferred outbound publication, no re-entry, operation completion/stale discard, GC cleanup и restart-only reload.
