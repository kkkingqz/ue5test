---
title: World Domain Object Tasks
status: archived
version: 1.2
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/CanonicalStateAndSave.md
  - ../../../Architecture/LuaRuntimeContract.md
---

# M2 — World Domain Object

## Результат этапа

Над секцией `state.world` появляется доменный объект по той же модели, что Actor: disposable wrapper без второй копии данных. Мир получает первое реальное содержимое — текущую локацию.

## Задачи

- [x] **GEW-04 — Реализовать World wrapper**
  - Доступ через `game.instances.world`; фасад не расширяется, поскольку мир — singleton runtime instance.
  - Wrapper disposable: держит ссылку на `state.world`, не копирует её, не кэшируется и не попадает в state.
  - Done: повторный доступ не гарантирует ту же таблицу wrapper; wrapper отклоняется валидатором state, если его попытаться сохранить; время жизни равно времени жизни session.
  - Evidence: `Scripts/runtime/world.lua` (`core:module.runtime.world`, зарегистрирован в `Scripts/bootstrap/manifest.lua`/`main.lua`) реализует `M.get_world()` — функция `game.instances.world()`, возвращающая свежий metatable-wrapper над `state.world` при каждом вызове (кэш не ведётся); `__index`/`__newindex` делегируют чтение/запись напрямую в `state.world` (mutation window уже применена на уровне `game.state`, отдельного enforcement не требуется). Отклонение при сохранении обеспечивается существующей общей проверкой `state_validator.lua` (`getmetatable(val) ~= nil` → error), новый код валидации не потребовался. Портируемый conformance-тест `GV2RuntimeCore::Testing::RunWorldDomainObjectConformance()` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/Testing/GV2WorldDomainObjectConformance.h`, `.../Private/GV2WorldDomainObjectConformance.cpp`) проверяет: `game.instances.world` — функция; два последовательных вызова возвращают разные таблицы; запись через wrapper видна в `game.state.world` и через второй wrapper; дерево state с сохранённым wrapper отклоняется `state_validator.validate_state_tree`. Тест исполняется на обоих хостах: `gv2-headless --self-test` (exit code 13 при провале) и UE Automation `GV2.Runtime.Lua.WorldDomainObjectConformanceCrossHost` — оба зелёные, golden digest не изменился, `ctest` 36/36, UE `GV2.Runtime` 49/49 (`TEST COMPLETE. EXIT CODE: 0`).

- [x] **GEW-05 — Ввести текущую локацию**
  - `world` получает поле текущей локации игрока; значение — `definition_id` kind `location`.
  - В `GameData/core` добавляются минимум две локации, между которыми возможен переход.
  - Done: ссылка на несуществующую локацию отклоняется валидацией state; изменение corpus отражено в pinned content hash в том же change set; чтение текущей локации доступно из presentation без мутации.
  - Evidence: `GameData/core/definitions/locations.json5` получил вторую локацию `core:location.city.tavern` (плюс `core:text.location.tavern.title` в `texts.json5`), доводя corpus до 2 локаций/7 core-текстов. `Scripts/runtime/state_validator.lua` добавляет проверку `state.world.current_location_id`: grammar (строка), kind `location` через `stable_id.is_kind` (новая зависимость `core:module.runtime.state_validator` → `core:module.runtime.stable_id` в `Scripts/bootstrap/manifest.lua`), и pinned-repository resolution через `game.repository.exists` — идентично уже существующей проверке `definition_id`/`meta.player_actor_id`, новой инфраструктуры не потребовалось. Портируемый conformance-тест `GV2RuntimeCore::Testing::RunWorldCurrentLocationConformance()` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/Testing/GV2WorldCurrentLocationConformance.h`, `.../Private/GV2WorldCurrentLocationConformance.cpp`) со собственным минимальным in-memory repository (один `location`-definition) проверяет: валидная ссылка на существующую локацию проходит `validate_state_tree`; Stable ID неверного kind (`core:screen.main`) отклоняется до обращения к repository; well-formed, но несуществующая ссылка отклоняется как dangling; `game.instances.world().current_location_id` читает значение без открытия mutation window. Тест исполняется на обоих хостах: `gv2-headless --self-test` (exit code 14 при провале) и UE Automation `GV2.Runtime.Lua.WorldCurrentLocationConformanceCrossHost` — оба зелёные. Изменение corpus обновило pinned хэши в том же change set: `Tests/Fixtures/expected_core_content_hash.txt`, `Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.{manifest,digest}.json5`, хардкод в `Headless/CMakeLists.txt` (`gv2_headless_golden_replay_matches_digest`) и в `Headless/Source/main.cpp`/`Source/GV2/Private/Tests/GV2ContentCoreRepositoryResolutionTests.cpp` (представительный core+test_mod content hash), а также ранее захардкоженные счётчики definitions (`GV2ContentCoreMinimalCoreSchemasTests.cpp`, `GV2ContentCoreRepositoryResolutionTests.cpp`, `GV2RuntimeCoreTests.cpp::RepositoryAccess`) и `Tests/Fixtures/PortableContentCore/valid/core/definitions/{locations,texts}.json5` (зеркало `GameData/core`, они делят один pinned hash). `ctest` 43/43, UE `GV2.Runtime` 50/50 (`TEST COMPLETE. EXIT CODE: 0`).

## Проверка milestone

- [x] Мир доступен как доменный объект, а не как сырая таблица.
- [x] Wrapper не попадает в canonical state ни при каких условиях.
- [x] Текущая локация ссылается на существующий definition kind `location`.
