---
title: Canonical Gameplay State Implementation Plan
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - ../../../Architecture/CanonicalStateAndSave.md
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../Architecture/BootstrapAndSessionLifecycle.md
decisions:
  - ../../../ADR/0004-lua-state-mutation.md
  - ../../../ADR/0020-cpp-scope-criterion.md
  - ../../../ADR/0021-opaque-save-container.md
---

# План реализации Canonical Gameplay State

> **Архив.** План выполнен полностью (M1–M5) и больше не является источником задач. Нормативное поведение перенесено в [Canonical State and Save](../../../Architecture/CanonicalStateAndSave.md), [Commands and Events](../../../Architecture/CommandsAndEvents.md) и [Lua Runtime Contract](../../../Architecture/LuaRuntimeContract.md). Документ сохраняется как implementation record.

## Цель

Canonical gameplay-state существует, создаётся при bootstrap session, проверяется на инварианты, наблюдается одним скаляром в run digest и меняется единственным разрешённым путём.

План материализует [Canonical State and Save](../../../Architecture/CanonicalStateAndSave.md) в части state и не начинает save/load.

## Состояние на 2026-08-15

Все этапы (M1–M5) полностью выполнены:
- Module lifecycle hooks вызываются вместо `install()`;
- `game.state` собирается из вкладов модулей и валидируется до `Ready`;
- Аллокатор `instance_id` (`core:module.runtime.instance_allocator`) выдаёт уникальные ID;
- Канонический хэш state считается на чистом Lua (`core:module.runtime.state_hasher`), публикуется скаляром хосту и включён в `FRunDigest`;
- Правила валидации консолидированы в `Scripts/runtime/state_validator.lua`;
- Единая объектная модель акторов (`game.instances.actors`) с disposable wrappers и динамическими дискриминаторами;
- Каноническое окно мутации `mutation_window` защищает состояние от несанкционированных записей;
- Реестр Gameplay Services (`game.services`) замораживается после фазы `register`;
- Первый command handler (`core:command.actor.reward`) выполняет доменные мутации с возвратом структурированных результатов;
- Conformance-тесты закрывают все инварианты на обоих хостах.

## Объектная модель

Принята следующая модель, и этапы M4–M5 её материализуют:

- canonical state остаётся простым сериализуемым деревом; runtime-объекты являются disposable обёртками над теми же таблицами и второй копии state не создают;
- registry отвечает за identity, lookup, creation, removal и deterministic enumeration, но не за gameplay rules;
- Actor реализует локальные доменные операции, Gameplay Service — workflow над несколькими сущностями;
- Command выражает намерение и описывает действие, а не запись в поле state;
- игрок является обычным Actor, а не отдельной моделью персонажа;
- изменение state допустимо только внутри mutation window — пока исполняется command handler.

Четыре последних пункта потребовали правки контрактов: `CanonicalStateAndSave` (единая модель акторов), `CommandsAndEvents` (временный инвариант мутации и правило про действия вместо полей) и `LuaRuntimeContract` (registry живёт под `game.instances`, а не расширяет фасад). Изменения внесены до начала M4.

## Границы

Входят:

- module lifecycle hooks в объёме, необходимом для state;
- canonical state root, его секции и правила допустимых значений;
- persistent instance identity и счётчики;
- канонический хэш state, вычисляемый Lua, и его включение в run digest;
- минимальный mutation slice: реестр Gameplay Services, одна mutating service и один command handler.

Не входят:

- command validators, EventBus и post-commit facts;
- runtime phases `Idle`/`ExecutingCommand`/`PumpingEvents`/`Saving` и очереди команд/событий;
- save/load, контейнер, `migrate_state` и `restore_instances`;
- `game.random`, `game.time`, `game.log` — в `meta` резервируются слоты, генераторы приходят отдельно;
- изменения presentation path и `build_initial_projection`.

## Milestones

- [x] M1 — [State Root and Module Lifecycle](StateRootAndModuleLifecycle.md): state создаётся и валидируется при bootstrap.
- [x] M2 — [Instance Identity](InstanceIdentity.md): persistent счётчики и `instance_id`.
- [x] M3 — [State Observability](StateObservability.md): канонический хэш state в run digest.
- [x] M4 — [Runtime Validator Consolidation](RuntimeValidatorConsolidation.md): одна реализация правил валидации state.
- [x] M5 — [Actor Object Model and Mutation Slice](ActorObjectModel.md): акторы, registry и первое реальное изменение state.

## Критический путь

```text
State Root and Module Lifecycle
→ Instance Identity
→ State Observability
→ Runtime Validator Consolidation
→ Actor Object Model and Mutation Slice
```

Хэш стоит после M2 намеренно: `meta` со счётчиками входит в state, поэтому включение хэша раньше означало бы переписать golden-манифесты дважды.

M4 стоит перед M5 по той же логике. M5 меняет правило хранения игрока, а это правило сейчас записано сразу в нескольких местах; консолидировать сначала дешевле, чем править одно и то же дважды и рисковать расхождением.

## Общие правила выполнения

1. Canonical state не пересекает C++/Lua boundary (ADR-0021). Наблюдаемость обеспечивается одним скаляром, а не передачей дерева.
2. Новый код принадлежит Lua, если не выполнено ни одно условие ADR-0020. Для этого плана C++ допустим только в узком месте публикации скаляра.
3. Mutation вне активного command handler является нарушением contract и покрывается тестом, а не только review.
4. Каждая задача добавляет negative case, если меняет validation или failure semantics.
5. Изменение наблюдаемого результата требует осознанного обновления golden в том же change set.
6. Новое observable behavior синхронно отражается в соответствующем contract.

## Итоговый Definition of Done

- [x] Session не достигает `Ready` при невалидном state.
- [x] Выданный `instance_id` не переиспользуется в пределах save lineage.
- [x] Одинаковый state даёт одинаковый хэш независимо от порядка вставки ключей, и этот хэш виден в run digest.
- [x] Правила валидации state существуют в одной реализации.
- [x] Игрок и NPC используют одну модель; wrapper никогда не попадает в state.
- [x] Команда, меняющая state, меняет run digest; golden ломается осознанно.
- [x] Мутация вне mutation window обнаруживается тестом.
- [x] Contracts соответствуют фактическому поведению.
