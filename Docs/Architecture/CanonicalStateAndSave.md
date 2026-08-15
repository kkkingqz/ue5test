---
title: Canonical State and Save
status: draft
version: 1.2
updated: 2026-08-15
depends_on:
  - LuaRuntimeContract.md
  - StableIDSpecification.md
  - BootstrapAndSessionLifecycle.md
decisions:
  - ../ADR/0020-cpp-scope-criterion.md
  - ../ADR/0021-opaque-save-container.md
---

# Canonical State and Save

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
}
```

| Section | Purpose |
|---|---|
| `meta` | Save/schema versions, save identity, player actor ID (`player_actor_id`), instance counters, PRNG streams, gameplay time |
| `actors` | Persistent player and NPC actor instances (`instance_id`, `definition_id`, ...) |
| `item_instances` | Unique item instances; stack counts live in owning containers |
| `world` | Global flags и location/screen state; `current_location_id` — Stable ID kind `location`, валидируется `state_validator.lua` против pinned repository (план [GameplayEventsAndWorld](../Plans/GameplayEventsAndWorld/README.md), GEW-05) |
| `quests` | Activated quest instances only |
| `mods` | Только нестандартное namespaced mod state |

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
- **Разрешение Definition ID**: каждый `definition_id` проверяется на соответствие Stable ID grammar и обязан существовать в pinned snapshot репозитория (`game.repository.exists`). Ссылка на несуществующую дефиницию даёт ошибку валидации.
- **Принадлежность предметов**: каждый unique item в `state.item_instances` обязан иметь `instance_id`, `definition_id` и `owner_id`, указывающий на ровно один логический контейнер (актора или локацию/слот). Ссылки на удалённых или несуществующих акторов (`owner_id` с префиксом `actor@`) отклоняются валидатором `LuaStateValidationInvalid`.
- **Политика удаления и ссылочная целостность**: удаление сущностей через Registry обязано оставлять дерево состояния валидным:
  - `actor_registry.remove(id)` отклоняет удаление актора с ошибкой `ActorHasDependentReferences`, если на него ссылаются зависимые предметы (`state.item_instances`) или квесты. Предметы должны быть явно переданы другому владельцу либо удалены до удаления актора.
  - Удаление игрока атомарно очищает `state.meta.player_actor_id`.
- **Идентичности**: Definition, instance и UE projection identities не взаимозаменяемы.

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

## Safe point

Save разрешён только когда:

- runtime phase `Idle`;
- command/event queues empty;
- session `Ready`, not `Failed`;
- gameplay-significant technical inputs processed;
- no lifecycle transition active.

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

## Missing mods

State неизвестного/disabled mod сохраняется opaque в container и не передаётся чужому module. При возвращении mod section доступна только после version/fingerprint compatibility check. Broken required references на missing definitions следуют explicit per-field recovery policy; silent substitution запрещён.

## Migrations

Migration принадлежит Lua, работает с temporary tree и имеет `(section_id, from_version, to_version)`. Она deterministic, side-effect-free, не вызывает Bridge/events/effects и не меняет исходный container. Downgrade не поддерживается.

## Tooling

Внешняя инспекция сейва требует того же Lua runtime: инструмент строится как Lua-host, а не как отдельный C++-парсер контейнера. Вторая реализация формата запрещена — она немедленно разойдётся с единственной канонической.

## Required follow-up

До production content нужны typed schemas для каждой root section, save compatibility matrix, orphan policy по reference kinds, corruption/backup recovery fixtures и size limits platform policy.

