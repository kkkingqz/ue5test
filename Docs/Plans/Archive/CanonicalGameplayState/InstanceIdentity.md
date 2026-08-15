---
title: Instance Identity Tasks
status: archived
version: 1.0
updated: 2026-08-14
depends_on:
  - StateRootAndModuleLifecycle.md
  - ../../../Architecture/CanonicalStateAndSave.md
  - ../../../Architecture/StableIDSpecification.md
---

# M2 — Instance Identity

## Результат этапа

`meta` содержит persistent счётчики, аллокатор выдаёт `instance_id` по grammar Stable ID, а принадлежность инстансов проверяется инвариантами. После этого форма state стабилизируется, и её можно фиксировать хэшем.

## Задачи

- [x] **CGS-06 — Реализовать аллокатор `instance_id`**
  - Зависимости: CGS-02.
  - Grammar `instance-kind "@" positive-counter`; counter начинается с 1, leading zeros запрещены.
  - Done: выданная пара `(kind, counter)` никогда не переиспользуется; счётчики не уменьшаются; next counters хранятся в `meta` и являются частью state; исчерпание диапазона даёт typed-ошибку, а не молчаливое переполнение.
  - Evidence: Создан модуль аллокатора `Scripts/runtime/instance_allocator.lua` (`core:module.runtime.instance_allocator`), поддерживающий валидацию, парсинг, форматирование и последовательное инкрементирование счетчиков в `game.state.meta.instance_counters` с защитой от переполнения; в C++ `GV2ContentCore::FStableId` и `GV2RuntimeCore::FStableId` добавлена поддержка `ParseInstanceId` / `IsValidInstanceId` и `FInstanceIdView`; расширены конформанс-тесты `GV2StableIdConformance.h` и `GV2LuaLifecycleConformance.cpp` (`TestInstanceAllocatorAllocatesSequentially`, `TestInstanceAllocatorRespectsExistingCounter`, `TestInstanceAllocatorRejectsInvalidKind`, `TestInstanceAllocatorRejectsExhaustedCounter`); CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-07 — Зафиксировать структуру `meta`**
  - Зависимости: CGS-06.
  - `meta` хранит save/schema версии, save identity, instance counters и зарезервированные слоты для PRNG и gameplay time.
  - Done: зарезервированные слоты существуют и пусты; их появление позже не требует изменения формы `meta`; ни одно поле `meta` не является runtime wrapper или кэшем.
  - Evidence: В `GV2RuntimeSession.cpp` и `state_validator.lua` зафиксирована каноническая форма `meta` (`schema_version = 1`, `save_version = 1`, `save_id = ""`, `instance_counters = {}`, `prng = {}`, `time = {}`); реализовано слияние вложенных таблиц мета-состояния в `MergeStateContribution`; валидатор `state_validator.lua` проверяет строгие инварианты типов (положительные целые версии, строковый save_id, плоские таблицы счетчиков с валидными сегментами kind и положительными счетчиками, плоские таблицы prng/time без метатаблиц); добавлены тесты `TestMetaSectionCanonicalFieldsPresent`, `TestMetaSectionRejectsInvalidSchemaVersion`, `TestMetaSectionRejectsInvalidInstanceCounterValue`, `TestMetaSectionRejectsInvalidInstanceCounterKind` в `GV2LuaLifecycleConformance.cpp`; CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-08 — Проверять инварианты принадлежности**
  - Зависимости: CGS-06.
  - Unique instance имеет один `instance_id` и один `definition_id`; каждый unique item принадлежит ровно одному logical container; player хранится отдельно от `actors`.
  - Done: нарушение принадлежности и дубликат `instance_id` дают typed-ошибку валидации; `definition_id` разрешается в pinned snapshot, отсутствующий definition является ошибкой, а не тихой порчей.
  - Evidence: В `state_validator.lua` и `GV2RuntimeSession.cpp` реализованы проверки инвариантов экземпляров: уникальность `instance_id` во всем дереве состояния, валидация грамматики и проверка существования `definition_id` в pinned snapshot репозитория через `game.repository.exists`, изоляция `player` от `actors` (запрет хранения игрока в `state.actors`), обязательное указание `owner_id` контейнера для каждого уникального предмета в `state.item_instances`; добавлены конформанс-тесты `TestInstanceOwnershipValidItemInstance`, `TestInstanceOwnershipRejectsDuplicateInstanceId`, `TestInstanceOwnershipRejectsMissingDefinitionIdInRepository`, `TestInstanceOwnershipRejectsPlayerInActors`, `TestInstanceOwnershipRejectsItemWithoutOwner` в `GV2LuaLifecycleConformance.cpp`; CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-09 — Синхронизировать документацию этапа**
  - Зависимости: CGS-06–CGS-08.
  - Done: `CanonicalStateAndSave` описывает фактическую структуру `meta` и поведение аллокатора; `StableIDSpecification` не расходится с реализацией grammar; `ImplementationStatus` обновлён.
  - Evidence: Документы `CanonicalStateAndSave.md` и `ImplementationStatus.md` обновлены с отражением формы `meta`, поведения аллокатора `instance_allocator` и инвариантов экземпляров/принадлежности; проверено соответствие `StableIDSpecification.md`.

## Проверка milestone

- [x] Повторная выдача одного `(kind, counter)` невозможна.
- [x] Счётчики переживают пересборку state в пределах session и остаются частью дерева.
- [x] Ссылка на несуществующий `definition_id` отклоняется валидацией.
- [x] Форма state зафиксирована и готова к хэшированию.
