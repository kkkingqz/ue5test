---
title: Add Lua Spec
status: informative
version: 1.2
updated: 2026-08-20
depends_on:
  - README.md
---

# Добавить проверку

> **Задача:** проверить правило так, чтобы проверка исполнялась обоими хостами.
> **Предмет:** portable Lua spec и общий spec runner.
> **Нужно:** собранный `gv2-headless`.
> **Нормативно:** [Headless Simulation](../Architecture/HeadlessSimulationContract.md), [Build and Tooling](../Architecture/BuildAndTooling.md).

## Когда спека, а когда C++

Правило целиком выражено в Lua — спека. Проверяется C++ API (сериализация, парсинг, marshalling, storage) — C++ conformance entry point. Новый C++-набор на Lua-правило запрещён и ломает CI.

## Шаги

**1. Выбрать под-дерево.** `Tests/Lua/world/`, `events/`, `lifecycle/`, `resources/`, `save/` — на production-сессии; `Tests/Lua/commands/` — на изолированной fixture-сессии, потому что регистрация тестовых валидаторов и команд требует собственной сессии.

**2. Написать файл, возвращающий именованные кейсы.**

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

Кейс независим и не полагается на порядок исполнения. Если кейсу нужно изменить состояние, он открывает mutation window явно: на production-сессии окно закрыто.

**3. Убрать за собой.** Кейс, менявший состояние или реестры, восстанавливает исходное перед возвратом — следующий кейс не должен зависеть от того, что было раньше.

**4. Запустить.**

```bash
./cmake-build-ci/Headless/gv2-headless --self-test
```

Провал печатает стабильный идентификатор `<spec>.<case>` — тот же в обоих хостах.

**5. Проверить, что спека действительно ловит.** Полезная привычка: временно сломать проверяемое поведение и убедиться, что нужный кейс упал с ожидаемым идентификатором, затем откатить.

## Типичные ошибки

**Новое под-дерево.** Список под-деревьев сейчас перечислен в коде обоих хостов, поэтому новое под-дерево требует правки двух C++-файлов; иначе спеки будут исполняться только в headless. Внутри существующего под-дерева новый файл подхватывается сам.

**Зависимость между кейсами.** Порядок обнаружения отсортирован, но опираться на него нельзя.

**Проверка C++ API спекой.** Сериализацию манифеста, digest и marshalling проверяет C++ conformance — там спека ничего не даст.

**Мутация без окна.** На production-сессии запись в `game.state` вне окна отклоняется; это правило, а не помеха тестированию.
