---
title: Canonical State and Save
status: draft
version: 2.1
updated: 2026-09-12
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
  - ../ADR/0044-session-replacement-and-registry-sealing.md
  - ../ADR/0045-atomic-save-slot-generation-publication.md
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
    seed_hex = "0000000000000000",
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
| `meta` | Save/schema versions, save identity, full-width root seed (`seed_hex`), player actor ID (`player_actor_id`), instance counters, PRNG streams, gameplay time |
| `actors` | Persistent player and NPC actor instances (`instance_id`, `definition_id`, `current_location_id`, ...) |
| `item_instances` | Unique item instances; stack counts live in owning containers |
| `world` | Global flags и world state; локация игрока хранится на акторе игрока, а `game.instances.world().current_location` является read-only аксессором ([ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)) |
| `quests` | Activated quest instances only |
| `mods` | Только нестандартное namespaced mod state |
| `definitions` | Sparse runtime-состояние definitions (`definitions[def_id]`), ключуется по Stable ID определения и валидируется против pinned repository ([ADR-0027](../ADR/0027-designer-lua-authoring-layer.md)) |

Стандартные mod entities используют общие registries. `mods[mod_id]` не дублирует standard state.

## State composition ownership

`core:module.runtime.state_composition` целиком владеет созданием root, вызовом module state hooks, merge/collision policy и назначением temporary tree. C++ передаёт ordered winning module IDs, typed scalar start inputs и optional opaque load bytes, вызывает один protected phase и получает только success/fault. Section names, contribution tables и decoded state boundary не пересекают.

Фиксированные поля `meta` (`schema_version`, `save_version`, `save_id`, `seed_hex`, `player_actor_id`) устанавливает composition owner; module contributions не переопределяют их. Обычная contribution является map известных canonical root sections в table без metatable. Ключ верхнего section и каждый ключ внутри section уникальны по всему ordered module pass; collision даёт `LuaModuleDefaultStateInvalid`. Для engine-owned nested maps `meta.instance_counters`, `meta.prng` и `meta.time` разрешено объединение только по уникальным child keys; overwrite также запрещён.

В `mods` module может записывать только ключ собственного namespace/module ID. Actual canonical section set и special nested-map classification находятся в одном Lua descriptor, используемом и composition, и validation; C++ enum/string list отсутствует. Новый section добавляется вместе с descriptor, validation и Lua spec. Partial tree при fault не присваивается `game.state`.

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

## Deterministic random streams

`game.random` принадлежит Lua. Host передаёт root `seed_hex` до первого default/state/start hook; C++ не реализует PRNG и не изменяет `meta.prng`. `seed_hex` — ровно 16 lowercase ASCII hex characters, то есть полный uint64 без преобразования через JSON number/double.

Stream identity — Stable ID kind `random_stream`, например `core:random_stream.gameplay`. Первое обращение к новому stream вычисляет:

```text
digest = SHA-256("gv2-prng-v1\0" + seed_hex + "\0" + stream_id)
state  = первые 16 bytes digest как четыре big-endian uint32
```

All-zero state заменяет последний word на `00000001`. Это defensive rule, а не обещание отсутствия hash collisions. Состояние хранится в `meta.prng[stream_id]` как algorithm tag `xoshiro128ss-v1` и четыре lowercase 8-hex words; load восстанавливает эти words и не reseed-ит существующий stream.

Переход и output используют `xoshiro128**` над unsigned 32-bit arithmetic:

```text
result = rotl32(s1 * 5, 7) * 9
t = s1 << 9
s2 ^= s0; s3 ^= s1; s1 ^= s2; s0 ^= s3; s2 ^= t; s3 = rotl32(s3, 11)
```

Каждая операция маскируется до 32 bits. `next_u32(stream_id)` возвращает `result` как nonnegative int64; `next_unit` возвращает `result / 2^32`. `next_int(min, max)` использует rejection sampling и отклоняет пустой диапазон или span больше `2^32`; modulo bias запрещён.

Independent vectors для первых пяти `next_u32`:

| `seed_hex` | `stream_id` | Initial words | Outputs |
|---|---|---|---|
| `0000000000000000` | `core:random_stream.gameplay` | `b11c2782 47dc733c af0684fb b5b1f64c` | `e020c7cb f94e13e0 a66c9e06 086a074f 81883abc` |
| `ffffffffffffffff` | `core:random_stream.gameplay` | `9370d948 28ef89cd 478ed7a1 af65c35b` | `0d9c8816 8a60ae26 1241ad6f 99d5616e 73735263` |

Изменение derivation, algorithm, word encoding или range mapping является breaking save/replay change: требует нового algorithm tag, migration либо typed refusal и обновления independent vectors.

До CFC-07A seed transport, `game.random` и stream state ещё не подключены; это `STATUS-023`, а не альтернативный алгоритм.

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
read_slot(save_slot_id, revision: Current | Previous) -> bytes | not_found | unreadable
write_slot(save_slot_id, bytes) -> ok | failure
```

Host обязан:

1. Разрешать `save_slot_id` в физический путь и запрещать любую другую адресацию.
2. Читать только явно выбранную revision; отсутствующий `Previous` возвращает `NotFound`, hidden fallback запрещён.
3. Публиковать immutable generation через один atomic head commit по [ADR-0045](../ADR/0045-atomic-save-slot-generation-publication.md).
4. Сохранять в `Previous` непосредственно предшествующие committed bytes.
5. Сериализовать операции одним application-owned storage owner; второй writer того же root получает `Busy`.
6. Возвращать typed result, не интерпретируя содержимое.

Host не разбирает bytes, не проверяет их структуру и не знает формата. Lua не выполняет filesystem I/O и не получает пути. Save write failure до head commit не меняет `Current` или `Previous`; cleanup failure после commit не отменяет успешную запись. Гарантия первой поверхности — process crash на Linux filesystem с temp/target на одном volume; power-loss durability не заявляется.

Обнаружение повреждения, отказ применять несовместимый container и все migrations принадлежат Lua и обязаны быть покрыты conformance-тестами.

**Текущее состояние реализации.** Portable storage уже выполняет opaque write/read и atomic replacement одного current-файла, а cold-start load читает slot через host. Application wiring, `Previous`/generation head и active-session preflight остаются gaps, перечисленными в [Implementation Status](../Status/ImplementationStatus.md); нормативный protocol выше не маскируется более слабой текущей реализацией.

## Safe point

Save разрешён только когда:

- runtime phase `Idle`;
- command/event queues empty;
- session `Ready`, not `Failed`;
- gameplay-significant technical inputs processed;
- no lifecycle transition active.

**Реализованная проверка (SAV-09, план [SaveAndLoad](../Plans/Archive/SaveAndLoad.md)).** `core:module.runtime.save.M.is_safe_point()` проверяет `game.runtime.phase == "idle"` (что само по себе исключает `ExecutingCommand`, `PumpingEvents` и `Failed` — единственные другие значения фазы), `game.commands.get_queue_length() == 0` и `game.events.get_queue_length() == 0`. `M.save()` вызывает эту проверку первым шагом и возвращает `false, "SaveNotAtSafePoint"` без единого обращения к storage primitive, если она не проходит — реентерабельный вызов `save()` изнутри обработчика команды/события всегда видит не-`idle` фазу и отклоняется тем же путём.

Gameplay/UI не вызывает storage напрямую. Lua-authored command может только поставить `request_save(slot_id)` в outbound control queue. Host принимает его после successful command dispatch и выхода из Lua, ждёт safe point и вызывает fixed `save_to_slot`; при отказе команды buffered request отбрасывается. Save outcome возвращается TechnicalInput/operation result и не является gameplay Event.

## Load

Load всегда создаёт replacement session по [ADR-0044](../ADR/0044-session-replacement-and-registry-sealing.md). `request_load(slot_id, revision)` выбирает `current` или `previous` явно и проходит тот же post-dispatch buffering, что save; C++ не знает command ID, который создал request.

1. Application resolve-ит required packages/repository и один раз читает выбранную revision в request-owned immutable byte buffer.
2. **Текущая** active VM A выполняет `preflight_save_bytes(buffer)`: header, integrity, versions, mod metadata и referential checks без изменения state/registries/queues/PRNG.
3. Failed preflight или изменение repository identity отклоняет request: A и её UI остаются Ready, teardown не выполняется.
4. Coordinator выполняет `commit-to-replace` и полностью уничтожает A; только затем создаётся VM B.
5. B получает ровно captured buffer шага 1, декодирует его во временное дерево, выполняет deterministic migrations и Stable ID redirects.
6. Runtime instances восстанавливаются, invariants проверяются; canonical state назначается только после полного успеха.
7. Сохранённые PRNG stream states продолжаются; seed start descriptor не вызывает reseed.
8. Initial presentation B готовится против candidate snapshot B и публикуется вместе с `Ready`.

Повторное чтение slot между preflight и B запрещено. Migration/start/presentation failure после `commit-to-replace` не изменяет source slot, уничтожает B и ведёт в native recovery; уже уничтоженная A не восстанавливается.

**Текущее состояние реализации.** Cold-start `FRuntimeSession::StartFromSave` выполняет decode/migrate/restore/validate/assign внутри новой VM и не публикует partial state. Active-session preflight, captured-buffer replacement и UE product load ещё не реализованы; они остаются явными status gaps, а не альтернативной lifecycle semantics.

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
