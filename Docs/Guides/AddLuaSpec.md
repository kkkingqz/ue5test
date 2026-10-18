---
title: Add Lua Spec
status: informative
version: 2.2
updated: 2026-10-18
depends_on:
  - README.md
  - ../Architecture/HeadlessSimulationContract.md
---

# Добавить portable Lua spec

> **Задача:** проверить Lua-правило одной реализацией в headless и UE.
> **Предмет:** `Tests/Lua/`, `ELuaSpecTier`, изоляция case и общий spec runner.
> **Нормативно:** [Headless Simulation](../Architecture/HeadlessSimulationContract.md), [Build and Tooling](../Architecture/BuildAndTooling.md).

Если проверяется C++ API — parser, marshalling, storage, manifest/digest serialization или сам runner — нужен shared C++ conformance entry point. Gameplay/Lua rule в C++ дублировать запрещено.

Новый Command, Validator, Event, Gameplay Service, migration или desired presentation сначала получает Lua spec и production Lua consumer; новый native entry point для такого изменения не создаётся. Если сценарий обнаружил действительно отсутствующую native capability, примените [When to Write Cpp](WhenToWriteCpp.md): scope reason, production consumer, negative fixture и actual enumerator добавляются вместе с API.

## Выбрать tier

| Tier | Packages/session | Подкаталоги |
|---|---|---|
| `Core` | `core` | `actions`, `actors`, `events`, `lifecycle`, `resources`, `save` |
| `TextSystem` | `core`, `textsystem`, `sample` | `world` |
| `FullGame` | `core`, `textsystem`, `rh` | `authoring`, `economy`, `presentation` |
| `FixtureCommands` | изолированная command fixture | `commands` |

Новый top-level subtree сначала требует явной записи в `GV2TestSupport` и host mapping. Необъявленный subtree отклоняется кодом 16.

## Написать spec

```lua
return {
    wrapper_writes_delegate_to_state = function()
        local mutation_window = require("core:module.runtime.mutation_window")
        mutation_window.execute_in_window(function()
            game.instances.world().marker = "spec"
        end)
        assert(game.state.world.marker == "spec", "write must reach state.world")
    end,
}
```

- Filename и case key используют lowercase `snake_case`.
- Case не зависит от порядка и восстанавливает изменённое state/registries.
- Нужные до freeze declarations помещаются в подходящий fixture tier, а не внедряются в production session.
- Проверка включает отрицательный case и убеждается, что без исправления действительно падает.
- Новое module-level временное состояние в любом модуле `core` регистрируется в `core:module.runtime.test_state_registry` в момент появления — функциями `snapshot`/`restore`/`reset`, а не добавлением в чей-то рукописный список (GAP-003, EGC-04). Незарегистрированное состояние обнаруживает перечислитель `test_state_registry.find_unregistered_state`, выводящий upvalue-ы модуля по идентичности (`debug.upvalueid`), а не по списку имён; `Tests/Lua/lifecycle/registry_sealing.lua` держит эту проверку для `core:module.authoring.context`.

Запуск:

```bash
./cmake-build-ci/Headless/gv2-headless --self-test
```

Failure identity одинакова в обоих host-ах: `<spec>.<case>`.
