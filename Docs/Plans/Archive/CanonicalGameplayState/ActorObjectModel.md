---
title: Actor Object Model and Mutation Slice Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - RuntimeValidatorConsolidation.md
  - ../../../Architecture/CommandsAndEvents.md
  - ../../../Architecture/CanonicalStateAndSave.md
decisions:
  - ../../../ADR/0003-command-and-event-model.md
  - ../../../ADR/0004-lua-state-mutation.md
---

# M5 — Actor Object Model and Mutation Slice

## Результат этапа

Gameplay-код работает с акторами через disposable runtime-объекты поверх canonical state, а не напрямую с таблицами. Registry отвечает за identity и получение объекта, Actor — за локальные доменные операции, Gameplay Services — за широкие workflow. Изменение state возможно только внутри mutation window, и это проверяется автоматически.

Этап реализует принятую объектную модель. Validators и EventBus в него не входят.

## Задачи

- [x] **CGS-19 — Ввести mutation window**
  - `CommandsAndEvents` заменил структурный инвариант временным: state меняется только пока исполняется command handler.
  - Done: окно открывается и закрывается диспетчером; попытка изменить `game.state` вне окна даёт typed-ошибку; проверка не требует передачи дерева в host и работает одинаково в обоих host-ах.
  - Evidence: `Scripts/runtime/mutation_window.lua`, `Scripts/runtime/command_dispatcher.lua`, `GV2RuntimeSession.cpp`, `GV2LuaLifecycleConformance.cpp` (`TestMutationWindowRejectsDirectMutationOutsideWindow`, `TestMutationWindowAllowsMutationInsideCommandHandler`), CTest (21/21), UE automation (45/45).

- [x] **CGS-20 — Перевести игрока в общую модель акторов**
  - Зависимости: CGS-15, CGS-19.
  - Игрок хранится в `state.actors`, `state.meta.player_actor_id` содержит его `instance_id`. Секция `player` остаётся только для данных, не являющихся состоянием Actor.
  - Действующая проверка «player не может лежать в `state.actors`» инвертируется: теперь ошибкой является дублирование модели игрока и отсутствующий или висячий `player_actor_id`.
  - Done: negative case на висячий `player_actor_id`; negative case на дублирование полей Actor в секции `player`; conformance обновлён вместе с правилом.
  - Evidence: `Scripts/runtime/state_validator.lua`, `GV2LuaLifecycleConformance.cpp` (`TestPlayerActorModelDanglingPlayerActorIdRejected`, `TestPlayerActorModelDuplicateActorInPlayerSectionRejected`, `TestPlayerActorModelValidPlayerInActors`), CTest (21/21), UE automation (45/45).

- [x] **CGS-21 — Реализовать ActorRegistry**
  - Зависимости: CGS-20.
  - Registry живёт под `game.instances.actors` и не добавляет новое поле фасада.
  - API: `get`, `exists`, `create`, `remove`, `ids`, `player`, выборка по дискриминатору.
  - `create` выполняет один путь: аллокация `instance_id` → построение plain state → валидация обязательных полей → вставка в `state.actors` → возврат wrapper. `wrap` не выдаёт identity и не создаёт state.
  - Done: `ids` возвращает detached список в canonical byte order; registry не содержит gameplay-правил и не предоставляет мутирующих методов вида `add_gold(id, n)`; время жизни registry равно времени жизни session и он не переживает replacement.
  - Evidence: `Scripts/runtime/actor_registry.lua`, `GV2LuaLifecycleConformance.cpp` (`TestActorRegistryIdsReturnsCanonicalSortedOrder`, `TestActorRegistryCreateAndRemoveInCommandHandler`), CTest (21/21), UE automation (45/45).

- [x] **CGS-22 — Реализовать Actor wrapper и дискриминатор**
  - Зависимости: CGS-21.
  - Wrapper disposable: содержит только ссылку на существующую таблицу state, не копирует её и не кэшируется.
  - Дискриминатор (`player`/`npc`) берётся из definition, а не дублируется в state: state хранит `definition_id`, registry выбирает wrapper по definition. Kind definition — `actor`; `core:npc.*` недопустим, поскольку kind `npc` отсутствует в kind registry.
  - В `GameData/core` добавляется schema `actor` с полем дискриминатора и минимум два definition.
  - Done: wrapper не попадает в state (проверяется существующей валидацией metatables); одинаковый `instance_id` при повторном `get` не гарантирует одну и ту же таблицу wrapper; изменение corpus отражено в pinned content hash в том же change set.
  - Evidence: `GameData/core/schemas/actor_v1.schema.json5`, `GameData/core/definitions/actors.json5`, `Scripts/runtime/actor_registry.lua`, `GV2LuaLifecycleConformance.cpp` (`TestActorWrapperDisposableAndDiscriminatorDerivedFromDefinition`), CTest (21/21), UE automation (45/45).

- [x] **CGS-23 — Определить удаление и висячие ссылки**
  - Зависимости: CGS-21.
  - `remove` обязан оставлять дерево валидным.
  - Done: зафиксирована политика для ссылок на удалённого актора из `item_instances.owner_id`, `world` и `quests`: явный отказ удаления либо явный каскад; висячий `instance_id` после `remove` невозможен и покрыт negative case.
  - Evidence: `Docs/Architecture/CanonicalStateAndSave.md`, `Scripts/runtime/state_validator.lua`, `Scripts/runtime/actor_registry.lua`, `GV2LuaLifecycleConformance.cpp` (`TestActorDeletionRejectsDependentItemOwnerReferences`), CTest (21/21), UE automation (45/45).

- [x] **CGS-24 — Ввести реестр Gameplay Services**
  - Зависимости: CGS-19.
  - Service регистрируется на фазе `register` и замораживается вместе с остальными registries.
  - Done: service возвращает structured result, не выполняет filesystem/UE calls, поздняя регистрация после freeze отклоняется; service используется для workflow над несколькими сущностями, а не как обёртка над одним доменным методом.
  - Evidence: `Scripts/runtime/service_registry.lua`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h`, `GV2LuaLifecycleConformance.cpp` (`TestServiceRegistryRegistrationAndLookup`, `TestServiceRegistryRejectsLateRegistrationAfterFreeze`, `TestGameplayServiceWorkflowOverMultipleEntities`), CTest (21/21), UE automation (45/45).

- [x] **CGS-25 — Реализовать первый command handler**
  - Зависимости: CGS-22, CGS-24.
  - `Scripts/gameplay/root.lua` перестаёт быть заглушкой: `core:command.actor.reward` меняет золото актора через доменный метод.
  - Handler остаётся тонким: связывает command с workflow и не содержит правил.
  - Done: успешная команда возвращает `{ ok = true, value = ... }`; отказ возвращает `{ ok = false, error = { code = "core:error...." } }` со стабильным Stable ID kind `error` и не меняет state; вложенный вызов handler из handler отклоняется typed-ошибкой, поскольку очередь команд в этап не входит.
  - Evidence: `Scripts/gameplay/root.lua`, `Scripts/runtime/actor_registry.lua`, `Scripts/runtime/command_dispatcher.lua`, `GV2LuaLifecycleConformance.cpp` (`TestActorRewardCommandHandlerSuccessAndFailure`, `TestCommandDispatcherRejectsNestedDispatch`), CTest (21/21), UE automation (45/45).

- [x] **CGS-26 — Закрыть модель conformance-тестами**
  - Зависимости: CGS-25.
  - Done: мутация вне mutation window обнаруживается из module initialization, из presentation-модуля и из обработчика вне command path; отказавшая команда не меняет state и не меняет хэш; digest после успешной команды отличается от digest без неё; golden обновлён осознанно в этом же change set.
  - Evidence: `GV2LuaLifecycleConformance.cpp` (`TestMutationOutsideWindowRejectedFromPresentationModule`, `TestFailedCommandPreservesCanonicalStateAndHash`, `TestRunDigestDiffersAfterSuccessfulCommand`), CTest (21/21), UE automation (45/45).

- [x] **CGS-27 — Синхронизировать документацию этапа**
  - Зависимости: CGS-19–CGS-26.
  - Done: `LuaRuntimeContract` описывает `game.instances.actors` и disposable wrappers; `CanonicalStateAndSave` описывает фактическую форму записи актора; `CommandsAndEvents` отмечает, какая часть lifecycle реализована; `GlossaryAndNaming` содержит термины Actor, ActorRegistry и mutation window; `ImplementationStatus` обновлён.
  - Evidence: `Docs/Architecture/LuaRuntimeContract.md`, `Docs/Architecture/CanonicalStateAndSave.md`, `Docs/Architecture/CommandsAndEvents.md`, `Docs/Architecture/GlossaryAndNaming.md`, `Docs/ImplementationStatus.md`.

## Проверка milestone

- [x] Игрок и NPC используют одну модель; дублирующей модели игрока нет.
- [x] Registry отвечает только за identity и получение объекта.
- [x] Wrapper не попадает в canonical state ни при каких условиях.
- [x] Мутация вне mutation window обнаруживается автоматически в обоих host-ах.
- [x] Команда меняет state, и это видно в run digest.
- [x] Validators, EventBus, phases и очереди не появились в рамках этапа.
