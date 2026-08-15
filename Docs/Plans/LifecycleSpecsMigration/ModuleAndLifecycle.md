---
title: Module and Lifecycle Tasks
status: draft
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0024-lua-spec-runner.md
---

# M1 — Module and Lifecycle

## Результат этапа

Правила обнаружения модулей, порядок разрешения графа зависимостей и вызовы хуков жизненного цикла (`init`, `register`, `ready`) покрыты декларативными спеками в `Tests/Lua/lifecycle/`.

## Задачи

- [x] **LSM-01 — Спека на регистрацию и топологический порядок модулей**
  - Описание: Спека проверяет обнаружение модулей в пакете, парсинг поля `id`, зависимости `dependencies`, построение топологического порядка и корректное отклонение циклических зависимостей и дубликатов `id`.
  - Done: Спека `Tests/Lua/lifecycle/module_graph.lua` исполняется через Spec Runner.
  - Evidence: Создан `Tests/Lua/lifecycle/module_graph.lua` (кейсы `loaded_core_modules_have_valid_ids`, `topological_sorter_resolves_dag`, `topological_sorter_detects_cycles`, `topological_sorter_detects_self_dependency`); `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **LSM-02 — Спека на хуки жизненного цикла и freeze реестров**
  - Описание: Спека проверяет последовательный вызов хуков `init(ctx)`, `register(ctx)`, `ready(ctx)` для всех модулей, изоляцию глобального окружения, недоступность мутаций состояния на фазе регистрации и freeze реестров (`game.commands.validators`, `game.events.subscribers`, `game.services`) после фазы `register`.
  - Done: Спека `Tests/Lua/lifecycle/lifecycle_phases.lua` проверяет порядок фаз и заморозку реестров.
  - Evidence: Создан `Tests/Lua/lifecycle/lifecycle_phases.lua` (кейсы `validator_registry_frozen_after_init`, `subscriber_registry_frozen_after_init`, `service_registry_frozen_after_init`, `service_registry_lookup_and_require`, `sandbox_removes_unsafe_primitives`, `runtime_phase_and_command_tracking`); `Headless/Source/main.cpp` и `Source/GV2/Private/Tests/GV2LuaSpecRunnerHostTests.cpp` дополнены поддеревьями `resources` и `lifecycle`; CTest (57/57 passed).

## Проверка milestone

- [x] Модульный граф и циклические зависимости проверяются спеками.
- [x] Все три фазы инициализации и freeze реестров проверяются спеками.
