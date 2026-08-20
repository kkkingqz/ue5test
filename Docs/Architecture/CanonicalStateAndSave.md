---
title: Canonical State and Save
status: draft
version: 2.0
updated: 2026-08-20
depends_on:
  - LuaRuntimeContract.md
  - RuntimeFacadeAndRegistries.md
  - StableIDSpecification.md
  - BootstrapAndSessionLifecycle.md
decisions:
  - ../ADR/0020-cpp-scope-criterion.md
  - ../ADR/0021-opaque-save-container.md
  - ../ADR/0026-core-and-gameplay-ownership.md
  - ../ADR/0027-designer-lua-authoring-layer.md
  - ../ADR/0031-entity-authoring-extensions.md
  - ../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# Canonical State and Save

> **Владеет:** canonical state, identity/lifecycle runtime instances, effective entity methods, конвертом сейва и последовательностью загрузки.
> **Не владеет:** тем, когда состояние меняется ([Commands and Events](CommandsAndEvents.md)), и содержимым definitions ([GameDataRepository](GameDataRepositoryContract.md)).
> **Инварианты:** [INV-001](Invariants.md), [INV-008](Invariants.md), [INV-015](Invariants.md)
> **Реализация:** `Scripts/runtime/state_validator.lua`, `instance_allocator.lua`, `canonical_codec.lua`, `save.lua`, `load.lua`.
> **Проверки:** `Tests/Lua/lifecycle/state_sections.lua`, `Tests/Lua/save/`.

Этот начальный контракт фиксирует границу state/save. Конкретная per-system schema будет добавляться без изменения ownership.

## Canonical root

```lua
game.state = {
  meta = {
    schema_version = 1,
    save_version = 1,
    save_id = "",
    player_actor_id = "actor@1",
    instance_counters = {},
    prng = {},
    time = {},
  },
  actors = {},
  item_instances = {},
  world = {},
  quests = {},
  mods = {},
  definitions = {},
}
```

| Section | Purpose |
|---|---|
| `meta` | Save/schema versions, save identity, player actor ID (`player_actor_id`), instance counters, PRNG streams, gameplay time |
| `actors` | Persistent player and NPC actor instances (`instance_id`, `definition_id`, `current_location_id`, ...) |
| `item_instances` | Unique item instances; stack counts live in owning containers |
| `world` | Global flags и world state; локация игрока хранится на акторе игрока, а `game.instances.world().current_location` является read-only аксессором ([ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)) |
| `quests` | Activated quest instances only |
| `mods` | Только нестандартное namespaced mod state |
| `definitions` | Sparse runtime-состояние definitions (`definitions[def_id]`), ключуется по Stable ID определения и валидируется против pinned repository ([ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)) |

Стандартные mod entities используют общие registries. `mods[mod_id]` не дублирует standard state.

## Allowed values

State содержит strings, bool, int64/finite double, dense arrays, string-key maps/objects и `game.null`; cycles/shared identity/functions/metatables/userdata/handles/definition tables запрещены.

Значимый map iteration имеет explicit sort. Runtime wrappers и caches не входят в save.

## Instance invariants and allocator

- **Единая модель акторов**: игрок является обычным Actor и хранится в `state.actors` наравне с NPC. `state.meta.player_actor_id` содержит его `instance_id`. Дублирующая модель персонажа игрока или отдельная секция `player` запрещены.
- **Persistent Allocator (`instance_allocator`)**: модуль `core:module.runtime.instance_allocator` выдаёт `instance_id` по грамматике `instance-kind "@" positive-counter` (`^[a-z][a-z0-9_]*@[1-9][0-9]*$`).
  - Next counters сохраняются в `game.state.meta.instance_counters[kind]` и не уменьшаются.
  - Повторная выдача выданной пары `(kind, counter)` невозможна.
  - При достижении лимита `MAX_COUNTER = 9007199254740991` выбрасывается типизированная ошибка `InstanceCounterExhausted`.
- **Уникальность ID**: `instance_id` глобально уникален по всему дереву состояния; дубликаты отклоняются валидатором `LuaStateValidationInvalid`.
- **Разрешение Definition ID и ссылочные поля**: каждый `definition_id` проверяется на соответствие Stable ID grammar и обязан существовать в pinned snapshot репозитория (`game.repository.exists`). Дополнительные поля состояния, ссылающиеся на Stable ID конкретного kind (например, `current_location_id` для kind `location`), регистрируются пакетами динамически на фазе `register` через `state_validator.register_reference_field(field_name, expected_kind)`. Валидатор проверяет их тип, kind и существование дефиниции, а модуль `load.lua` использует тот же реестр для переписывания редиректов при загрузке. Ссылка на несуществующую дефиницию даёт ошибку валидации.
- **Принадлежность предметов**: каждый unique item в `state.item_instances` обязан иметь `instance_id`, `definition_id` и `owner_id`, указывающий на ровно один логический контейнер (актора или локацию/слот). Ссылки на удалённых или несуществующих акторов (`owner_id` с префиксом `actor@`) отклоняются валидатором `LuaStateValidationInvalid`.
- **Политика удаления и ссылочная целостность**: удаление сущностей через Registry обязано оставлять дерево состояния валидным:
  - `actor_registry.remove(id)` отклоняет удаление актора с ошибкой `ActorHasDependentReferences`, если на него ссылаются зависимые предметы (`state.item_instances`) или квесты. Предметы должны быть явно переданы другому владельцу либо удалены до удаления актора.
  - Удаление игрока атомарно очищает `state.meta.player_actor_id`.
- **Идентичности**: Definition, instance и UE projection identities не взаимозаменяемы.

## Runtime instances and entity methods

Persistent record хранит `instance_id`, `definition_id` и explicit state. Wrapper, method table, metatable и cache восстанавливаются и в canonical state не сохраняются.

`game.instances` содержит category registries и singleton-объекты. Actor registry предоставляет `get`, `exists`, `create`, `remove`, deterministic `ids` и `player`; каждый lookup возвращает fresh disposable wrapper. Общий instance registry регистрирует новые kinds на фазе `register`, связывает kind с state section и после freeze запрещает новые categories. Создание всегда использует persistent allocator и pinned repository definition.

`game.instances.world()` возвращает fresh wrapper над global `state.world`. Текущая локация принадлежит actor-у игрока; `world.current_location`/`current_location_id` — read-only accessors к этому actor field, а не второй state field.

`game.entity_extensions` собирает методы по entity kind в immutable effective method table. Duplicate declaration, late registration и mutation table дают `EntityExtensionDuplicateDeclaration`, `EntityExtensionRegistryFrozen` и `EffectiveMethodTableFrozen`. Wrapper разрешает method только через effective table, требует корректный `self` (`MissingReceiver`) и не использует fallback на global player. Его `instance_id`, `definition_id` и `discriminator` read-only (`ActorDiscriminatorImmutable`). Authoring syntax методов и fields принадлежит [Authoring Surface](AuthoringSurfaceContract.md); managed field без объявленной operation даёт `MissingDomainOperation` при freeze.

Для наблюдаемости Lua публикует fixed `game.runtime.get_canonical_state_hash`; host читает один скаляр через `FRuntimeSession::GetCanonicalStateHash()`. До создания `game.state` accessor возвращает `""` без fault; дерево state boundary не пересекает.

## Save container

Container включает:

- format/save schema versions и save ID;
- game/build metadata и integrity check;
- repository content hash/provider fingerprints;
- explicit versioned core sections;
- PRNG streams и gameplay time;
- enabled mods/order/versions/fingerprints;
- namespaced mod sections;
- opaque orphaned sections временно отсутствующих mods.

Container целиком принадлежит Lua. Physical encoding не является gameplay contract и не известен host-у.

**Кодек и `save_version` (SAV-04, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** Каноническая кодировка (`core:module.runtime.canonical_codec`, `M.serialize`/`M.deserialize`) — единственная реализация, общая для хэширования состояния (`state_hasher`) и container-а. Она объявляет `M.VERSION` — версию самой кодировки (набор тегов, framing длин/счётчиков, представление float), независимую от `save_version` container-а (который версионирует секции и формат конверта, а не байтовую кодировку значений). Изменение кодировки — breaking change для существующих сейвов, поэтому:

- Изменение `canonical_codec`, меняющее байтовый результат `M.serialize` хотя бы для одного значения, обязано поднять и `M.VERSION`, и `meta.save_version` в одном change set.
- Golden-прогоны (`state_hash` в `Tests/Fixtures/GoldenRuns/`) и pinned canonical-строка в `Tests/Lua/save/canonical_codec.lua` дают немедленный, невозможный не заметить сигнал: любое изменение кодировки без синхронного поднятия версий ломает CI на этом же коммите.
- `save_version` может расти отдельно от `M.VERSION` (например, миграция секции без изменения физической кодировки значений) — но не наоборот: `M.VERSION` не растёт без `save_version`.

**Реализованный конверт (SAV-08/18, PKG-21).** `core:module.runtime.save` (`Scripts/runtime/save.lua`, `M.build_envelope(state, save_id, repository_content_hash, script_set_hash, packages)`) собирает конверт из полей: `format_version`, `codec_version` (`canonical_codec.M.VERSION`), `save_version` (`M.SAVE_VERSION`), `save_id`, `repository_content_hash` (provenance, не условие загрузки), `script_set_hash` (хэш состава скриптов сессии), `packages` (состав и порядок загруженных пакетов), `section_versions` (SAV-18, свежая копия `migrate.CURRENT_SECTION_VERSIONS` — живая `game.state` всегда на текущих версиях секций, поэтому конверт не читает версии из самого state), `integrity` и `payload`. `payload` — `canonical_codec.serialize(state)`; `integrity` — `state_hasher.sha256(payload)`, что численно равно `state_hasher.hash_state(state)`. Сам конверт сериализуется тем же `canonical_codec.serialize`, не отдельным форматом. `save_id` и `repository_content_hash` — параметры вызова, а `script_set_hash`/`packages` при отсутствии явных аргументов считываются из `game.runtime`. При загрузке `load.lua` проверяет наличие зафиксированных пакетов, предотвращая тихий откат на базовые модули. Оставшиеся поля списка выше (PRNG streams, gameplay time, namespaced mod sections, orphaned sections) относятся к будущим задачам.

## Export boundary

Canonical gameplay-state не пересекает C++/Lua boundary (ADR-0021). Lua сериализует state в непрозрачную последовательность байт, сама считает integrity check и сама владеет версиями секций.

Host предоставляет slot-scoped storage primitive:

```text
read_slot(save_slot_id) -> bytes | not_found | unreadable
write_slot(save_slot_id, bytes) -> ok | failure
```

Host обязан:

1. Разрешать `save_slot_id` в физический путь и запрещать любую другую адресацию.
2. Записывать во временный файл и атомарно подменять slot.
3. Сохранять предыдущую копию.
4. Возвращать typed result, не интерпретируя содержимое.

Host не разбирает bytes, не проверяет их структуру и не знает формата. Lua не выполняет filesystem I/O и не получает пути. Save write failure не меняет предыдущий valid slot.

Обнаружение повреждения, отказ применять несовместимый container и все migrations принадлежат Lua и обязаны быть покрыты conformance-тестами.

**Реализованная запись (SAV-06/10, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** `write_slot` реализован как `GV2RuntimeCore::FFilesystemSaveSlotStorage`, единая реализация для обоих host-ов (`Docs/Architecture/BuildAndTooling.md` "Save slot storage primitive"), доступная Lua через единственный биндинг `game.save_slots.write(slot_id, bytes) -> ok, err_code` (`core:module.runtime.save.M.save` — единственный вызывающий). `read_slot` пока не забинжен в Lua — он появится вместе с Cold Start Load (M4).

## Safe point

Save разрешён только когда:

- runtime phase `Idle`;
- command/event queues empty;
- session `Ready`, not `Failed`;
- gameplay-significant technical inputs processed;
- no lifecycle transition active.

**Реализованная проверка (SAV-09, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** `core:module.runtime.save.M.is_safe_point()` проверяет `game.runtime.phase == "idle"` (что само по себе исключает `ExecutingCommand`, `PumpingEvents` и `Failed` — единственные другие значения фазы), `game.commands.get_queue_length() == 0` и `game.events.get_queue_length() == 0`. `M.save()` вызывает эту проверку первым шагом и возвращает `false, "SaveNotAtSafePoint"` без единого обращения к storage primitive, если она не проходит — реентерабельный вызов `save()` изнутри обработчика команды/события всегда видит не-`idle` фазу и отклоняется тем же путём.

## Load

Load всегда создаёт replacement session. Preflight выполняет **текущая** active session: она читает bytes через storage primitive и проверяет их сама, до запроса teardown. Вторая параллельная VM для preflight не создаётся — инвариант одной VM сохраняется.

1. Текущая session читает slot и проверяет header, integrity, версии и mod metadata.
2. Failed preflight отклоняет запрос: current session остаётся Ready, teardown не выполняется.
3. Application resolve-ит required packages и выбирает current repository.
4. Old session уничтожается только после успешного preflight.
5. Replacement session декодирует container во временное дерево.
6. Explicit core/mod migrations выполняются в deterministic order.
7. Stable ID redirects разрешаются.
8. Runtime instances восстанавливаются, invariants проверяются.
9. Canonical state назначается только после полного успеха.
10. Строится initial presentation, session коммитится как Ready.

Migration failure не изменяет source slot. После failure replacement candidate уничтожается и создаётся recovery menu.

**Реализован только холодный старт (SAV-12–17, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** Из шагов выше существует только эквивалент шагов 5, 8, 9 — и только когда приложение стартует с нуля (`FRuntimeSession::StartFromSave`), а не когда уже есть активная session. Replacement session (шаги 1–4, 10 — preflight текущей сессией, teardown, коммит нового session как Ready) не реализован (README.md прямо это оговаривает: без него пункт меню «загрузить другое сохранение» работать не будет). Explicit core/mod migrations (шаг 6) — M5, не реализованы. Реализованный порядок: slot читается host-примитивом до создания VM (`SaveSlotNotFound`/`SaveSlotUnreadable` — configuration failure нулевой VM-стоимости) → `core:module.runtime.load.decode_and_prepare` целиком в Lua (preflight header/codec/save_version/integrity, редиректы шага 7 разрешаются и переписываются, referential integrity) → структурная валидация дерева → хук `restore_instances` (шаг 8) → модульный `validate_state` → присвоение canonical state (шаг 9) → `start`. Провал любой стадии оставляет сессию без назначенного состояния — эквивалент recovery surface на уровне `FRuntimeFault`, без UI-слоя (тот принадлежит `GV2SessionCoordinator`, не затронут этим планом).

## Missing mods

State неизвестного/disabled mod сохраняется opaque в container и не передаётся чужому module. При возвращении mod section доступна только после version/fingerprint compatibility check.

**Отсутствие ID — всегда ошибка (SAV-16, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md), RH-12).** Broken required reference даёт `SaveReferenceRetired` для tombstone либо `SaveReferenceUnknown` для отсутствующего ID; silent substitution и частичное восстановление запрещены. Смена namespace ломает старую ссылку. Redirect из `core` в игровой пакет запрещён направлением зависимостей. Per-field recovery требует отдельного решения. Объём гарантии между релизами определён в [Compatibility Policy](CompatibilityPolicy.md).

## Migrations

Migration принадлежит Lua, работает с temporary tree и имеет `(section_id, from_version, to_version)`. Она deterministic, side-effect-free, не вызывает Bridge/events/effects и не меняет исходный container. Downgrade не поддерживается.

**Реализовано (SAV-18–20, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** `core:module.runtime.migrate.CURRENT_SECTION_VERSIONS` — версия каждой canonical-секции для текущего build-а; записывается в конверт как `section_versions` (`core:module.runtime.save.build_envelope`) при каждом сохранении. При загрузке `migrate.plan_migrations(envelope.section_versions)` строит детерминированно упорядоченный (по `section_id`) список pending-миграций и немедленно, до вызова хоть одного module hook, отклоняет `MigrationDowngradeUnsupported`, если хотя бы одна секция сейва новее, чем понимает build. Фаза `migrate_state` (между декодированием и `restore_instances`, `BootstrapAndSessionLifecycle.md`) даёт каждому модулю шанс забрать секции, которые он умеет мигрировать, помечая их обработанными; `migrate.verify_complete()` после прохода всех модулей отклоняет `MigrationMissing:<section_id>:<from>-><to>`, если хоть одна pending-запись осталась непомеченной — молчаливый пропуск невозможен. Provал на любой из этих стадий происходит до присвоения `game.state` (SAV-17: состояние присваивается только после полного успеха) и никогда не касается уже записанного слота — миграция работает только с decoded-в-память деревом, а не с исходными байтами container-а или файлом слота.

## Tooling

Внешняя инспекция сейва требует того же Lua runtime: инструмент строится как Lua-host, а не как отдельный C++-парсер контейнера. Вторая реализация формата запрещена — она немедленно разойдётся с единственной канонической.

## Required follow-up

До production content нужны typed schemas для каждой root section, save compatibility matrix, orphan policy по reference kinds, corruption/backup recovery fixtures и size limits platform policy.
