---
title: Bootstrap and Session Lifecycle
status: normative
version: 4.1
updated: 2026-09-15
depends_on:
  - SystemContextAndComponents.md
  - GameDataRepositoryContract.md
  - LuaRuntimeContract.md
  - RuntimeFacadeAndRegistries.md
decisions:
  - ../ADR/0006-repository-reload-and-session-pinning.md
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0011-blueprint-screen-templates.md
  - ../ADR/0042-presentation-authority-and-publication.md
  - ../ADR/0043-presentation-apply-boundary.md
  - ../ADR/0044-session-replacement-and-registry-sealing.md
---

# Bootstrap and Session Lifecycle

> **Владеет:** порядком cold start, состояниями сессии и приложения, хуками модулей и их фазовыми ограничениями, teardown.
> **Не владеет:** содержимым состояния ([Canonical State and Save](CanonicalStateAndSave.md)) и семантикой команд ([Commands and Events](CommandsAndEvents.md)).
> **Инварианты:** [INV-005](Invariants.md), [INV-006](Invariants.md)
> **Реализация:** `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Source/GV2/Private/Application/`, `Scripts/bootstrap/`.
> **Проверки:** `Tests/Lua/lifecycle/`, `GV2.Runtime.ContentCore.Session*`.

Ни один partially built repository, state, runtime или presentation не становится public. Commit выполняется на Game Thread после token/generation validation.

## Core invariants

- Не более одной active session и одной Lua VM. До teardown A candidate B может содержать только native immutable inputs/authorities; runtime session и VM B ещё не существуют.
- VM создаётся и уничтожается вместе с session.
- Cold start создаёт full menu session с обычным lifecycle core и enabled mod modules и empty gameplay roots.
- Initial repository строится до первой VM. Application может позднее опубликовать новый current snapshot, но active session остаётся pinned до restart.
- После registration один private Lua lifecycle owner выполняет единый registry sealing gate по [Runtime Facade and Registries](RuntimeFacadeAndRegistries.md#host-side-freeze-sequence).
- До `Ready` semantic input, commands, events, effects и save закрыты, кроме lifecycle-owned initial projection path.
- Async result проверяет owner, session generation и operation token.
- Failure candidate session заканчивается обязательным cleanup до `Destroyed`.

## Application states

```text
Uninitialized → Bootstrapping → MenuActive
MenuActive/GameActive → Transitioning → MenuActive/GameActive
Any active state → ShuttingDown → Terminated
Bootstrapping/Transitioning → Failed → Transitioning or ShuttingDown
```

`MenuActive`/`GameActive` допустимы только когда active session `Ready`. `Failed` не удерживает повреждённую session.

## Session states

```text
Creating → Registering → BuildingState → RestoringInstances
→ Starting → PreparingPresentation → Ready

Any build phase → Failed → Stopping → Destroyed
Ready → Stopping → Destroyed
```

`Registering` включает module `register` hooks и обязательный sealing checkpoint; state `BuildingState` недостижим при незавершённом sealing. Public readiness — один bool `is_ready`. Он становится true только при `publish-ready` после successful initial presentation apply и сбрасывается перед `commit-to-replace` или обычным teardown.

Каждая portable protected phase возвращает закрытый `FRuntimePhaseResult`: `Completed` либо `Fault(FRuntimeFault)`. Отмена не является return value Lua hook и проверяется native orchestrator-ом между phases. UE transition routine проецирует исполненную фазу в существующий `EGV2SessionState`; отдельная модель states, которую production path не вызывает, запрещена.

## C++ lifecycle façade

`UGV2RuntimeSubsystem` живёт в `UGameInstance` scope и предоставляет Blueprint только состояние lifecycle и typed requests. Он не является владельцем gameplay-state и не вызывает произвольные Lua functions.

Минимальный Blueprint-facing API:

- `GetSessionState()` возвращает application/session state и `is_ready` без mutable internal references.
- `SubmitUiInteraction(binding_handle, input_values)` принимает opaque UI binding handle и schema-defined values; команда определяется только current binding registry.
- `RequestSession(descriptor)`, `RequestSave(slot_id)` и `RequestLoad(slot_id, revision)` возвращают opaque operation ID; `CancelSessionRequest(operation_id)` возвращает typed `Accepted | TooLate | Stale`.
- lifecycle requests используют отдельные typed methods/descriptors, а не generic `CallLuaFunction(name, args)`.

Public façade не содержит test-only runtime methods:

- compatibility adapters `StartSession()`/`EndSession()` могут только делегировать typed lifecycle protocol и не владеют вторым transition path;
- `GetActiveScreen()` возвращает только текущую reconstructable presentation instance;
- `SubmitUiInteraction(...)` является единственным публичным путём пользовательского input;
- создание Screen из C++ параметров, вызов Lua builder из automation и методы с семантикой `ForTest` запрещены.

**Presentation candidate (ADR-0043 D1).** Screen Registry, Image Resource Catalog, UI schemas, Theme и GameShell identity принадлежат одному private `FGV2SessionContentSnapshot`, построенному из того же `FResolvedPackageSet`, что repository и Lua sources. Candidate строится один раз, не читает runtime global config и содержит независимый resolved Screen Registry value. До `publish-ready` он не виден как active snapshot; partial publication запрещена.

Failure repository/snapshot build не создаёт VM B и возвращает `RepositoryNotReady`, `ScreenRegistryNotReady` или `ImageCatalogNotReady`. При cold start без A показывается `UGV2RecoveryScreenWidget`; при replacement до `commit-to-replace` продолжает работать A. Recovery surface не создаёт bindings и использует только programmatic core-minimal theme. Binding records session-scoped и инвалидируются при новой generation.

Start sequence: `GameInstance` start → Screen Registry ready → package modules register and freeze registries → package-owned `start` hook may create its initial gameplay state exclusively through a registered Command Dispatcher command → presentation source resolves the resulting state and publishes an initial Screen request → coordinator забирает pending screen → registry resolution → prepared field/binding candidate → registered `WBP_ScreenBase` child → atomic field apply → binding revision commit → активный экран отображается во viewport. Screen replacement выполняется после выхода из Lua. C++ не знает ни стартовой команды пакета, ни `screen_id`, ни Widget class.

Интерактивный Editor использует data-driven development profile `UGV2RuntimeSettings.EditorPackageRoots` из `DefaultGame.ini`: production profile `core + textsystem + rh` открывает `textsystem:screen.location` из начального RH gameplay-state. Один и тот же resolved package set (`FResolvedPackageSet`, ADR-0043 D1) обязан использоваться для repository build, для загрузки package Lua sources, для обнаружения `ui_field`/`ui_value` схем и для построения Screen Registry/Image Catalog/Theme candidate snapshot; расхождение этих наборов, включая повторное самостоятельное discovery канонического замыкания любым из них, запрещено (`PAH-R3`) — второй вывод того же факта является вторым авторитетом, даже когда сегодня совпадает с первым. Обнаружение схем и остальных частей snapshot происходит один раз за сессию, синхронно внутри `StartSession()`, до перехода в `Ready` — сессия владеет своим snapshot так же, как `PinnedRepository` (его частью), и он не переживает `EndSession()`. Commandlet, unattended automation, Headless и Shipping игнорируют Editor profile и используют обычный package set; automation, которой нужен fixture, подключает `sample` явно. Automatic debug fixture запрещён в Shipping и не добавляет отдельный test API.

Coordinator получает один уже разрешённый `FResolvedPackageSet`, pinned repository и repository identity через typed descriptor/context. Отсутствующий set является ошибкой host bootstrap: coordinator/candidate не выполняют fallback discovery и не могут заменить переданное множество каноническим каталогом. Упрощённый overload без set допустим только под `WITH_DEV_AUTOMATION_TESTS` как fixture и не входит в production call inventory.

`FGV2SessionCoordinator` является private UE owner active/candidate session. Он создаёт для каждой generation отдельную portable runtime session, Bridge context, ingress queue, UI binding registry и operation registry. Ни один из этих объектов не переживает уничтожение owning session. `GV2RuntimeCore` не зависит от UObject/UMG и назначает вызывающий Game Thread owner thread-ом VM; standalone host использует тот же lifecycle на своём worker thread.

Все Blueprint/UE requests сначала попадают в coordinator-owned bounded FIFO ingress. Coordinator проверяет state/generation и запускает Lua entry point только когда `bExecutingLua=false`. Submit из работающего entry point может только добавить следующий item в очередь; nested execution запрещён. Переполнение возвращает typed technical rejection и не расходует accepted input sequence. Lua outbound publications принимаются как copied DTO и применяются после возврата текущего protected entry point; synchronous Blueprint ↔ Lua re-entry запрещён.

## Session start descriptor and results

Downstream API использует закрытые типы `ESessionStartMode`, `FSessionStartDescriptor`, `ESessionOperationOutcome`, `FGV2OperationFault`, `FGV2SessionOperationResult` и `ESessionCancellationResult`; stringly-typed mode/outcome запрещены.

```text
mode: Menu | NewGame | LoadSave
save_slot_id: required only for LoadSave; otherwise absent
save_slot_revision: Current | Previous; required only for LoadSave
repository_version: exact pinned snapshot identity
repository_content_hash: exact pinned repository identity
seed_hex: exactly 16 lowercase ASCII hex characters
reason: diagnostic string
```

`seed_hex` — gameplay input uint64, а session generation — lifetime token; они не взаимозаменяемы. `Restart` повторяет committed descriptor. `LoadSave` восстанавливает сохранённые PRNG streams и не reseed-ит их значением descriptor.

Terminal operation outcome имеет закрытое множество `Completed | Failed | Cancelled | Superseded`. `Failed` несёт typed fault (`FGV2OperationFault`); остальные outcomes (`Completed`, `Cancelled`, `Superseded`) не маскируются как success и никогда не несут fault. Запись `Failed` без типизированного fault запрещена сигнатурой compile-time (`RecordFailure(...)`, перегрузка `RecordOutcome(..., ESessionOperationOutcome)` удалена). Публичное чтение исхода через `UGV2RuntimeSubsystem::GetSessionOperationOutcome` возвращает `ESessionOperationOutcome` и `FGV2OperationFault` (для Blueprint) либо `TOptional<FGV2SessionOperationResult>` (в C++). Каталог канонических кодов ошибок сессии объявляется в `FGV2SessionFaultCodes` с программным перечислителем `GetAllDeclaredFaultCodes()`.
По [Compatibility Policy](CompatibilityPolicy.md) расширение сигнатуры чтения исхода типизированным fault до версии `1.0.0` является классифицированным breaking change публичного C++/Blueprint downstream API: метод больше не отдаёт только `ESessionOperationOutcome` в обход typed fault, а compatibility alias без fault запрещён правилом полноты диагностики.
Operation ID value-only и не содержит callback/Lua reference.

### Ограничение истории операций и retention semantics

История terminal operation outcomes ограничена детерминированным FIFO-вытеснением по `OperationId` с лимитом `DefaultMaxRetainedOutcomes = 160`.
Размер истории обоснован операционным профилем сессии игры:
- Сессионные переходы: \(N_{\text{trans}} \le 16\) за полный цикл (Cold Start Menu $\to$ New Game $\to$ до 10 загрузок/чекпоинтов $\to$ Restart $\to$ Shutdown).
- Запросы сохранения (`RequestSave`): при минимальном интервале автосохранения \(T_{\text{auto}} = 60\,\text{с}\) за 2 часа непрерывной игры совершается 120 автосохранений плюс до 24 ручных сохранений (\(N_{\text{save}} \le 144\)).
- Полное расчётное число операций за 2-часовую сессию: \(16 + 144 = 160\).
- При объёме записи ~48–64 байт лимит 160 записей фиксирует расход памяти $\le 10\,\text{КБ}$ на всё время жизни `GameInstance`, предотвращая неконтролируемый рост долгоживущего процесса.
- Downstream-потребители (UI-нотификации, индикаторы сохранения, Blueprint/Automation polling) опрашивают исход в пределах первых тиков ($W \le 16$ операций), поэтому лимит 160 даёт запас по времени $\ge 10\times$ даже для редкого опроса.

Момент вытеснения: при фиксации terminal outcome (`RecordOutcome` / `RecordFailure`), если число сохранённых записей достигает лимита, из карты немедленно удаляется запись с наименьшим `OperationId` (самая ранняя), а наивысший вытесненный ID обновляется (`HighestEvictedOperationId = max(HighestEvictedOperationId, EvictedId)`).

Наблюдаемое различие между вытесненной и неизвестной операцией:
- Запрос исхода через `QueryOutcome(OpId)` возвращает статус `ESessionOperationQueryStatus`:
  - `Found`: запись находится в ограниченной истории, результат (`Outcome`, `Fault`) возвращается;
  - `Evicted`: операция была зафиксирована и завершена, но её запись была вытеснена по превышению лимита истории (\(1 \le \text{OpId} \le \text{HighestEvictedOperationId}\) и отсутствует в карте). Метод `IsOperationEvicted(OpId)` возвращает `true`;
  - `InProgress`: операция выделена/поставлена в очередь (`ActiveOperation`, `PendingSlot` или safe-point save), но ещё не завершена;
  - `Unknown`: идентификатор никогда не выделялся данным хостом (\(\text{OpId} = 0\) либо \(\text{OpId} \ge \text{NextOperationId}\)). Метод `IsOperationEvicted(OpId)` возвращает `false`.
- Вытесненная операция не маскируется под `Unknown` или тихий пропуск.
- Ручные невызываемые методы очистки (такие как `FGV2SessionTransitionPolicy::Reset()`) запрещены; retention является строго автоматическим и ограниченным.

## Session replacement protocol

[ADR-0044](../ADR/0044-session-replacement-and-registry-sealing.md) задаёт две границы.

### До `commit-to-replace`

Ready-сессия A целиком остаётся public и исполнима. Host может разрешить package set/repository, построить native `FGV2SessionContentSnapshot` candidate B и захватить выбранные save bytes. Для `LoadSave` read-only preflight выполняет VM A над захваченным buffer; runtime session/VM B ещё не создаётся. Preflight не меняет state, registries, queues, bindings или UI A.

Ошибка, отмена, supersede либо изменение requested repository identity оставляют A без перестроения и повторной публикации. Последняя проверка repository identity выполняется непосредственно перед границей.

### `commit-to-replace`

Только `FGV2SessionCoordinator` может пересечь границу через private move-only transition token со стадиями `Preflight | Replacing | Preparing | Committed | Aborted`. Он атомарно закрывает input A и readiness, инвалидирует bindings, удаляет проекцию, выполняет reverse teardown и уничтожает VM A. После этого rollback к A запрещён. Token нельзя копировать, создать вне coordinator или перевести в terminal stage дважды.

### После `commit-to-replace`

Создаётся единственная runtime session/VM B. B получает те же pinned repository/native candidate и, для load, тот же immutable byte buffer, который проверила A. После sealing registries, сборки state и start initial document готовится с явно переданным `FGV2PresentationPrepareContext` B:

```cpp
using FDocumentSink = TFunction<bool(
    const FGV2UiDocumentViewModel&,
    const FGV2PresentationPrepareContext&)>;
```

Ambient `GetContentSnapshotForPrepare()` или другой выбор между A/B запрещён. `publish-ready` одним owner routine публикует snapshot, projection, bindings, generation и status B. Failure/cancellation после необратимой границы уничтожает B и показывает UE-native recovery; A не воскрешается.

## Module lifecycle

Каждый module экспортирует table с canonical `module_id`, например `weather_mod:module.storm_rules`:

```lua
return {
  id = "weather_mod:module.storm_rules",
  register = function(ctx) end,
  create_default_state = function(ctx) end,
  migrate_state = function(ctx, tree, from_version, to_version) end,
  restore_instances = function(ctx, tree) end,
  validate_state = function(ctx, tree) end,
  start = function(ctx) end,
  build_initial_projection = function(ctx) end,
  stop = function(ctx, reason) end,
  unregister = function(ctx) end,
}
```

Order: core modules, затем mods по resolved load order. `stop`/`unregister` выполняются в reverse order. Первая user-hook error прекращает следующие user hooks, но не обязательный C++ cleanup.

В текущей реализации жизненного цикла сессии вызываются хуки `register`, `create_default_state` (только вне cold-start load, целиком через `core:module.runtime.state_composition.compose_default_state`, CFC-05A — C++ не знает секций и правил склейки), `migrate_state` (только на cold-start load, см. ниже), `validate_state` и `start`. `restore_instances` подключён (SAV-17, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)) и вызывается только на cold-start load, между `migrate_state` и модульным хуком `validate_state` — свежее defaulted-состояние восстанавливать нечего. Хуки `stop` и `unregister` подключены (CFC-07) и вызываются в строго обратном порядке загрузки модулей (`reverse resolved module order`) при остановке сессии; ошибка в пользовательском хуке прекращает вызов последующих user-хуков, но не обязательный C++ cleanup. Хук `build_initial_projection` остаётся объявленным контрактом для этапа проекции. Отсутствие хука в модуле не является ошибкой.

**Сессионная загрузка и replacement реализованы (CFC-10).** `FRuntimeSession::StartFromSave(SessionGeneration, PinnedRepository, Sources, Storage, SaveSlotId, OutFault)` читает слот через `ISaveSlotStorage::ReadSlot` и делегирует выполнение перегрузке `StartFromSaveBytes(..., CapturedBytes, ...)` — отсутствующий (`SaveSlotNotFound`) или нечитаемый (`SaveSlotUnreadable`) слот завершает старт как configuration failure нулевой VM-стоимости. Для активной сессии `FRuntimeSession::PreflightSaveBytes` выполняет non-mutating preflight байтов в контексте VM A через `game.runtime.preflight_save_bytes` (проверка конверта, целостности, редиректов, referential integrity и планирования миграций секций без мутации состояния/реестров/PRNG). При успехе preflight координатор переходит в `BeginReplace`, разрушает VM A (`GLiveVmCount == 0`), создает VM B и инициализирует её из захваченного неизменяемого буфера байтов `StartFromSaveBytes` с иммунитетом к перезаписи слота на диске и восстановлением PRNG streams. Дерево состояния получается целиком одним вызовом `core:module.runtime.load.decode_and_prepare(container_bytes)` вместо `create_default_state`; провал на любой стадии (`SaveContainerCorrupt`, `SaveFormatVersionUnknown`, `SaveVersionDowngradeUnsupported`, `SaveIntegrityMismatch`, `SaveReferenceRetired`, `SaveReferenceUnknown`, `MigrationDowngradeUnsupported`, `MigrationMissing:...`) оставляет сессию незапущенной — ни одно состояние не присваивается частично.

**Миграции секций реализованы (SAV-18–20, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** `core:module.runtime.migrate.CURRENT_SECTION_VERSIONS` — версия каждой canonical-секции, которую понимает текущий build. `decode_and_prepare` вызывает `migrate.plan_migrations(envelope.section_versions)` сразу после resolve/rewrite ссылок: секция новее текущей — `MigrationDowngradeUnsupported` немедленно, до единого вызова какого-либо module hook; секция старше — попадает в список pending, сохранённый в `game.runtime.pending_section_migrations`. Фаза `migrate_state` (тот же `(ctx, tree)` calling convention, что у `restore_instances`/`validate_state`) даёт каждому модулю возможность забрать из этого списка секции, которые он понимает, пометив запись `handled = true`; после прохода всех модулей `core:module.runtime.migrate.verify_complete()` отклоняет типизированной `MigrationMissing:<section_id>:<from>-><to>` любую запись, оставшуюся непомеченной — молчаливый пропуск невозможен. Отдельной фазы `Saving` не потребовалось: запись сейва (`core:module.runtime.save.M.save`) синхронна целиком — один вызов кодека и один вызов host-примитива без промежуточной точки, где мог бы наблюдаться промежуточный статус (SavePath.md, SAV-09 Evidence).

### Phase restrictions

| Hook/phase | State mutation | Events/effects/I/O |
|---|---|---|
| `register` | No | Registration API only |
| `create_default_state`/`migrate_state` | Temporary tree only | No |
| `restore_instances`/`validate_state` | Temporary/session-local reconstruction | No external effects |
| `start` | Internal initialization; package может выполнить один идемпотентный initial Command Dispatcher command | Gates closed, кроме lifecycle-owned initial command path |
| `build_initial_projection` | Read-only state | Returns declarative snapshot |
| Ready runtime | Commands/services only | Normal rules |
| `stop`/`unregister` | No gameplay mutation | Local cleanup only |

## Cold start

1. Initialize platform/application services.
2. Discover and resolve core/enabled packages into one `FResolvedPackageSet` (ADR-0043 D1/D5) — единственный вход ниже.
3. Build and atomically publish repository from that same resolved set.
4. On repository error, do not create Lua VM; show UE-native recovery surface.
5. Create full Menu session pinned to repository; build private candidate `FGV2SessionContentSnapshot` (UI schemas, Screen Registry, Image Catalog, Theme, GameShell layer identity) from the same resolved package set — не отдельное discovery.
6. Register modules and выполнить единый registry freeze gate.
7. Build empty menu state, restore runtime objects, validate, start.
8. Apply initial menu presentation.
9. Commit session `Ready` and Application `MenuActive`; publish candidate snapshot atomically with this commit — не раньше и не по частям.

## New/load session build

1. Пока A остаётся Ready, resolve-ить exact package/repository identity и построить только private native `FGV2SessionContentSnapshot` candidate B. Resolved Screen Registry является независимым compiled value candidate-а, а не mutable authoring DataAsset.
2. Для `LoadSave` один раз прочитать выбранную slot revision в request-owned immutable buffer и выполнить read-only Lua preflight в VM A. Для cold start без A preflight является первой protected фазой новой VM после её создания.
3. Повторно проверить repository identity и пересечь `commit-to-replace`; полностью уничтожить A. До этого шага VM B запрещена.
4. Создать generation, runtime session B, Bridge и service set; передать typed start inputs, pinned repository, sources и optional captured bytes.
5. Вызвать module `register`, затем единый descriptor-driven registry sealing gate.
6. Собрать temporary state целиком в Lua: defaults для Menu/NewGame либо decode/migrate captured bytes для LoadSave.
7. Restore instances, validate invariants и назначить canonical state только после полного успеха.
8. Выполнить start hooks при закрытых external gates.
9. Построить initial UI document и применить его с явно переданным candidate context B.
10. Выполнить `publish-ready` и enable input; snapshot/projection/bindings/status становятся видимы одним commit.

## Lifecycle requests

Coordinator выполняет ровно один переход за раз через закрытую transition policy (`FGV2SessionTransitionPolicy`) и независимый transition oracle (`FGV2SessionTransitionOracle`). Request kind выводится из закрытого enum `ESessionTransitionKind` (`Menu | NewGame | LoadSave | Shutdown`), а каждый kind обрабатывается exhaustive switch без `default`. Эквивалентный запрос присоединяется к текущему активному или отложенному запросу (`join`). Единственный конфликтный pending слот использует last-wins семантику: вытесненный запрос завершается как `Superseded`. Запрос `Shutdown` обладает наивысшим приоритетом и очищает отложенные запросы.

По всему процессу гарантируется строго не более 1 живой Lua VM (`INV-010`), что контролируется атомарным счетчиком `GLiveVmCount`. Попытка создания второй одновременной VM отвергается типизированной ошибкой `LuaVmExceededLimit`. Дискретные фазы старта сессии (`Registering`, `BuildingState`, `RestoringInstances`, `Starting`, `PreparingPresentation`) вызывают фазовый callback, информирующий о прогрессе.

Cancellation принимается только между границами фаз; синхронный вызов Lua не прерывается, а отмена применяется после возврата из защищённого вызова (safe checkpoint). До `commit-to-replace` (BeginReplace) принятая отмена сохраняет активную сессию A в неизменном виде (`PreCommitCancellationKeepsPriorSession`). После пересечения границы необратимости cancellation останавливает сессию B, помечает outcome как `Cancelled` и переводит приложение в recovery, но не восстанавливает A. Все переходы состояний сессии и приложения выполняются исключительно через transition policy (`TryTransitionSessionState`, `TryTransitionApplicationState`); прямые присваивания public lifecycle state в обход неё запрещены gate-валидатором `validate_session_transition_ownership.py`.

## Replacement sequences

- **Menu → Game:** подготовить native game candidate при живой menu A; после `commit-to-replace` уничтожить A, показать UE-native loading surface и создать VM B.
- **Game → Menu:** подготовить native menu candidate при живой game A; после границы уничтожить A и создать новую full menu session.
- **Restart:** copy committed start descriptor, destroy current session, create replacement.
- **Load another save:** preflight target slot, then full replacement session; never mutate active state in place.
- **Content reload:** build/publish new Application current snapshot, then controlled restart so replacement session pins it.
- **Shutdown:** clear pending, block new input/operations, reverse cleanup, release repository/platform services.

## Teardown order

Пункты 1–8 начинаются только по owner-решению coordinator: при replacement это и есть `commit-to-replace`; UE adapter не выполняет их заранее.

1. `is_ready=false`; block input, commands, events, save and new operations.
2. Invalidate UI binding registry, чтобы queued или уже захваченные Widget events стали stale.
3. Stop effects and remove UI/Actor projections.
4. Cancel supported operations; invalidate all remaining tokens.
5. Call module `stop` in reverse order.
6. Call `unregister` in reverse registration order.
7. Force-clear command/event/service/instance registries, ingress и Bridge adapters.
8. Destroy Lua VM and session memory; state becomes `Destroyed`.

## Mandatory tests

Tests cover cold start, required Image Catalog/Screen Registry failure before VM, repository failure before VM, full menu lifecycle, NewGame/LoadSave order, one-VM invariant, last-wins pending slot, joined request, phase cancellation, stale completion/input discard, registry freeze, restore gates, readiness single commit, bounded ingress backpressure, FIFO/no synchronous re-entry, teardown hook failure, recovery menu, load-another-save preflight, shutdown priority и content-reload restart.
