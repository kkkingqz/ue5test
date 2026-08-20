---
title: Add Lua Spec
status: informative
version: 2.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/HeadlessSimulationContract.md
---

# Добавить portable Lua spec

> **Задача:** проверить Lua-правило одной реализацией в headless и UE.
> **Предмет:** `Tests/Lua/`, `ELuaSpecTier`, изоляция case и общий spec runner.
> **Нормативно:** [Headless Simulation](../Architecture/HeadlessSimulationContract.md), [Build and Tooling](../Architecture/BuildAndTooling.md).

Если проверяется C++ API — parser, marshalling, storage, manifest/digest serialization или сам runner — нужен shared C++ conformance entry point. Gameplay/Lua rule в C++ дублировать запрещено.

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

Запуск:

```bash
./cmake-build-ci/Headless/gv2-headless --self-test
```

Failure identity одинакова в обоих host-ах: `<spec>.<case>`.
