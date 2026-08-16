---
title: Event Bus Core Tasks
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - CommandValidators.md
  - ../../../Architecture/CommandsAndEvents.md
decisions:
  - ../../../ADR/0003-command-and-event-model.md
---

# M3 — Event Bus Core

## Результат этапа

Gameplay публикует неотменяемые факты после успешного commit. Доставка детерминирована, ограничена и не может изменить state.

События сознательно не входят в run digest, поэтому весь порядок доставки проверяется Lua-спеками: регрессионная сетка golden здесь не помогает.

Спеки этапа живут в `Tests/Lua/events/`. Регистрация тестовых подписчиков и команд требует собственной сессии, поэтому под-дерево исполняется на fixture-сессии по образцу `Tests/Lua/commands/`.

## Задачи

- [x] **GEW-06 — Определить конверт события**
  - `event_id` kind `event`, schema version, immutable payload, correlation/causation IDs и source provenance.
  - Done: payload содержит только допустимые portable-значения; конверт не содержит ссылок на runtime-объекты и таблицы state; конверт неизменяем после публикации.
  - Evidence: `Scripts/runtime/event_envelope.lua` (`M.create`, `M.is_envelope`, валидация Stable ID kind `event`, положительного `schema_version`, deep copy и read-only proxy для payload/source/envelope, проверка portable types и отказ на runtime-объекты/таблицы `game.state`/функции/циклы/non-finite числа/смешанные ключи), `Scripts/bootstrap/manifest.lua` (`core:module.runtime.event_envelope`), `Tests/Lua/events/envelope.lua` (11 спека-кейсов), подключение под-дерева `Tests/Lua/events` в `Headless/Source/main.cpp` и `GV2LuaSpecRunnerHostTests.cpp`, `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-07 — Публиковать только после commit**
  - Факты накапливаются в контексте текущей команды и доставляются после её успешного завершения.
  - Done: отказавшая команда не доставляет ни одного факта; факт, поставленный в очередь до мутации, не доставляется, если команда позже отказала; publish в обход dispatcher невозможен.
  - Evidence: `Scripts/runtime/event_bus.lua` (`M.begin_command_context`, `M.enqueue`/`emit`, `M.commit_command_context`, `M.rollback_command_context`, `M.discard_command_context`, `M.has_active_context`, `M.get_published_events`), интеграция в `Scripts/runtime/command_dispatcher.lua` (пост-коммит доставка при `ok ~= false`, автоматический откат `rollback` при `ok == false`, сброс `discard` при исключениях, запрет публикации вне команды `EventEnqueueOutsideCommandContext`), `Scripts/bootstrap/manifest.lua` и `main.lua`, `Tests/Lua/events/post_commit.lua` (6 спека-кейсов), `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-08 — Обеспечить детерминированную доставку**
  - Очередь FIFO; события, порождённые обработчиками, обрабатываются breadth-first.
  - Порядок подписчиков: priority, package load order, registration order.
  - Done: спека с несколькими подписчиками и вложенными событиями фиксирует точную последовательность доставки; последовательность одинакова в обоих host-ах; порядок не зависит от порядка регистрации модулей в файловой системе.
  - Evidence: `Scripts/runtime/event_bus.lua` (`M.subscribe` с `priority` и `order`, `M.get_subscribers`, детерминированная сортировка `priority` ascending + `order` ascending, FIFO очередь и breadth-first обработка вложенных событий), `Tests/Lua/events/delivery_order.lua` (5 спека-кейсов: `subscriber_priority_ordering`, `fifo_event_queue_ordering`, `breadth_first_nested_event_delivery`, `cross_host_order_determinism`, `invalid_subscriber_rejected`), `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-09 — Ввести фазы и лимит pump**
  - Появляются фазы `Idle`, `ExecutingCommand`, `PumpingEvents`. Фаза `Saving` в план не входит.
  - Done: новая команда во время `ExecutingCommand`/`PumpingEvents` ставится в очередь либо отклоняется typed-ошибкой согласно принятому в этап решению; превышение настроенного лимита pump переводит session в `Failed`; negative case на лимит.
  - Evidence: `Scripts/runtime/event_bus.lua` (`DEFAULT_PUMP_LIMIT = 1000`, `set_pump_limit`, `get_pump_limit`, `reset_pump_limit`, ограничение цикла pump с переходом в `game.runtime.phase = "failed"` и выбросом `EventPumpLimitExceeded`), `Scripts/runtime/command_dispatcher.lua` (отклонение вызовов во время `executing_command` ошибкой `CommandDispatchReentrant`, во время `pumping_events` ошибкой `CommandDispatchDuringEventPump`, в состоянии `failed` ошибкой `SessionStateFailed`), `Tests/Lua/events/pump_limit_and_phases.lua` (5 спека-кейсов), `gv2-headless --self-test` и CTest (57/57 passed).

## Проверка milestone

- [x] Факты доставляются только после успешного commit.
- [x] Точная последовательность доставки зафиксирована спекой.
- [x] Превышение лимита pump переводит session в `Failed`.
- [x] Фазы выставляются фактически, а не только объявлены.
