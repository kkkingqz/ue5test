---
title: State Root and Module Lifecycle Tasks
status: archived
version: 1.0
updated: 2026-08-14
depends_on:
  - README.md
  - ../../../Architecture/CanonicalStateAndSave.md
  - ../../../Architecture/BootstrapAndSessionLifecycle.md
decisions:
  - ../../../ADR/0004-lua-state-mutation.md
  - ../../../ADR/0007-lua-module-environment.md
---

# M1 — State Root and Module Lifecycle

## Результат этапа

Module loader вызывает lifecycle hooks в порядке, заданном контрактом. Каждый module объявляет свой вклад в canonical state, координатор собирает `game.state` до перехода session в `Ready`, а невалидный state блокирует создание session вместо того, чтобы проявиться позже.

## Задачи

- [x] **CGS-01 — Ввести module lifecycle hooks**
  - Реализовать вызов hooks в порядке контракта: core modules, затем mods по resolved load order.
  - В объёме этапа вызываются `register`, `create_default_state`, `validate_state` и `start`. `migrate_state`, `restore_instances`, `build_initial_projection`, `stop` и `unregister` остаются объявленными контрактом и не вызываются; это фиксируется явно, а не подразумевается.
  - Done: hook отсутствует в module — это не ошибка; ошибка внутри hook прекращает следующие user hooks и переводит candidate session в `Failed`; фазовые ограничения из таблицы контракта соблюдаются; `install()` как параллельная точка входа устранён.
  - Evidence: Реализован вызов жизненного цикла модулей (`RunLifecycleHooks`, `RunLifecyclePhase`) в `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`; хуки `register` перенесены в `Scripts/boundary/ingress.lua` и `Scripts/boundary/outbound.lua`, вызовы `install()` устранены; создан переносимый конформанс-тест `RunLuaLifecycleConformance()` в `Source/GV2RuntimeCore/Public/GV2RuntimeCore/Testing/GV2LuaLifecycleConformance.h` и `Private/GV2LuaLifecycleConformance.cpp`; добавлен UE Automation Test `GV2.Runtime.Lua.LifecycleConformanceCrossHost`; CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-02 — Создать canonical state root**
  - Зависимости: CGS-01.
  - `game.state` содержит семь секций контракта: `meta`, `player`, `actors`, `item_instances`, `world`, `quests`, `mods`.
  - Координатор собирает root из вкладов `create_default_state` до перехода в `Ready`; после сборки root доступен через фасад.
  - Done: пустая сессия стартует с непустым `game.state`; секция, не объявленная ни одним module, существует и пуста; повторный bootstrap даёт идентичное дерево.
  - Evidence: В `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` добавлена сборка корневого состояния из семи секций (`meta`, `player`, `actors`, `item_instances`, `world`, `quests`, `mods`) и слияние вкладов `create_default_state` перед назначением в `game.state`; добавлены кросс-хостовые тесты в `Source/GV2RuntimeCore/Private/GV2LuaLifecycleConformance.cpp` (`TestCanonicalStateRootStructure`, `TestCanonicalStateContributionsMerged`, `TestCanonicalStateInvalidSectionRejected`, `TestCanonicalStateNonTableContributionRejected`, `TestCanonicalStateNonTableSectionContributionRejected`); CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-03 — Проверять допустимые значения state**
  - Зависимости: CGS-02.
  - Валидация допускает strings, bool, int64, finite double, dense arrays, string-key maps и `game.null`.
  - Done: functions, metatables, userdata, cycles, sparse-массивы, non-finite числа, operation handles и copies definition-таблиц отклоняются typed-ошибкой; ошибка блокирует переход session в `Ready`; проверка выполняется в Lua и не требует передачи дерева в host.
  - Evidence: Создан модуль валидации `Scripts/runtime/state_validator.lua`; в `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` внедрена автоматическая валидация канонического дерева состояний `ValidateCanonicalStateTree` в Lua VM перед передачей управления `validate_state` и переходом в `Ready`; добавлены кросс-хостовые конформанс-тесты в `GV2LuaLifecycleConformance.cpp` (`TestStateValidationAllowsValidValues`, `TestStateValidationRejectsFunction`, `TestStateValidationRejectsMetatable`, `TestStateValidationRejectsNonFiniteNumberNaN`, `TestStateValidationRejectsNonFiniteNumberInfinity`, `TestStateValidationRejectsCycle`, `TestStateValidationRejectsSparseArray`, `TestStateValidationRejectsMixedArrayObject`, `TestStateValidationRejectsDefinitionTableCopy`); CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-04 — Зафиксировать изоляцию секций**
  - Зависимости: CGS-02.
  - Module пишет только в свою область: стандартные сущности — в общие секции через объявленный вклад, нестандартное mod state — в `mods[mod_id]`.
  - Done: попытка module записать в чужой `mods[other_id]` или переопределить чужой вклад обнаруживается при сборке root; `mods[mod_id]` не дублирует standard state.
  - Evidence: В `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` в функции `MergeStateContribution` реализованы проверки изоляции секции `mods` (модуль может вносить нестандартное состояние только под своим `mod_id` / namespace) и обнаружение коллизий/попыток перезаписи существующих ключей в стандартных секциях; добавлены кросс-хостовые конформанс-тесты в `GV2LuaLifecycleConformance.cpp` (`TestStateIsolationAllowedModContribution`, `TestStateIsolationRejectsForeignModContribution`, `TestStateCollisionRejectsDuplicateKeyInStandardSection`, `TestStateCollisionAllowsDisjointKeysInSameSection`); CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-05 — Синхронизировать документацию этапа**
  - Зависимости: CGS-01–CGS-04.
  - Done: `BootstrapAndSessionLifecycle` описывает фактически вызываемое подмножество hooks и явно называет отложенные; `LuaRuntimeContract` описывает `game.state` как доступное поле фасада; `ImplementationStatus` отражает изменившийся объём.
  - Evidence: Обновлены `Docs/Architecture/BootstrapAndSessionLifecycle.md`, `Docs/Architecture/LuaRuntimeContract.md`, `Docs/Architecture/BuildAndTooling.md` и `Docs/ImplementationStatus.md`.

## Проверка milestone

- [x] Session с валидными modules достигает `Ready` с непустым `game.state`.
- [x] Module с невалидным default блокирует session и не доходит до `Ready`.
- [x] Повторный bootstrap на одинаковом наборе modules даёт идентичный state.
- [x] Параллельная точка входа `install()` отсутствует.
