---
title: Registry Tasks
status: draft
version: 1.0
updated: 2026-08-16
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
---

# M1 — Registry

> **Материализует:** [Lua Runtime Contract § `game.commands`](../../Architecture/LuaRuntimeContract.md).
> **Задачи:** CHR-01…03.
> **Результат:** реестр обработчиков существует, замораживается вместе с остальными и покрыт спеками.

## Результат этапа

`game.commands.handlers` доступен и ведёт себя как остальные реестры. Диспетчер его ещё не использует, поэтому наблюдаемое поведение игры не меняется: незадействованный реестр — не второй путь исполнения.

## Задачи

- [x] **CHR-01 — Создать модуль реестра**
  - Новый `core:module.runtime.handler_registry` по форме `core:module.runtime.validator_registry`: та же структура `create_registry()`, те же классы ошибок, тот же канонический порядок перечисления.
  - API: `register(command_id, handler_fn, options)`, `get(command_id)`, `exists(command_id)`, `ids()`, `freeze()`.
  - Done: `command_id` обязан быть canonical Stable ID kind `command` — иначе `InvalidCommandId`; обработчик обязан быть функцией — иначе `InvalidCommandHandler`; повторная регистрация без `options.override` — `CommandHandlerDuplicateRegistration`; с `options.override = true` запись заменяется, а отсутствие замещаемой записи при заявленном `override` — тоже ошибка; регистрация после freeze — `CommandHandlerRegistryFrozen`; `ids()` возвращает детерминированный порядок; приоритет не поддерживается, `sequence` хранится только для перечисления.
  - Evidence: `Scripts/runtime/handler_registry.lua`.

- [x] **CHR-02 — Подключить реестр и его заморозку**
  - Зависимости: CHR-01.
  - Единственная правка C++ в плане: список замораживаемых реестров захардкожен в `GV2RuntimeSession.cpp`.
  - Done: модуль публикует реестр как `game.commands.handlers` на фазе `register`; `FreezeGameRegistry({"commands", "handlers"})` добавлен рядом с существующими тремя вызовами; модуль объявлен в манифесте и достижим от `entry_module_id`; замещаемость модуля выставлена `false` — реестр принадлежит ядру.
  - Evidence: `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Scripts/bootstrap/{manifest,main}.lua`, `Source/GV2TestSupport/Private/CommandValidatorFixture.cpp`, `Tests/Fixtures/CommandValidatorSpecs/manifest.lua`.

- [x] **CHR-03 — Покрыть реестр спеками**
  - Зависимости: CHR-02.
  - Done: спеки в `Tests/Lua/commands/` покрывают успешную регистрацию и lookup, невалидный `command_id`, обработчик не-функцию, дубликат, `override` над существующей и над отсутствующей записью, регистрацию после freeze и детерминизм `ids()`; ни одна проверка не написана на C++ ([ADR-0024](../../ADR/0024-lua-spec-runner.md)).
  - Evidence: `Tests/Lua/commands/handler_registry.lua`, 59/59 CTest passed, `gv2-headless --self-test` passed.

## Проверка milestone

- [x] `game.commands.handlers` доступен на фазе `register` и заморожен после неё.
- [x] Каждый класс отказа регистрации покрыт negative case.
- [x] Наблюдаемое поведение игры не изменилось: реестр пуст и никем не используется.
- [x] `repository_content_hash` не изменился.
