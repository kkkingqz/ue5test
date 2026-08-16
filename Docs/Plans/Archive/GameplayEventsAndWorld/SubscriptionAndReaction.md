---
title: Subscription and Reaction Tasks
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - EventBusCore.md
  - ../../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../../ADR/0004-lua-state-mutation.md
---

# M4 — Subscription and Reaction

## Результат этапа

Модуль подписывается на факт по `event_id` и реагирует на него. Условие срабатывания проверяется в самом обработчике: декларативных фильтров и предикатного языка не вводится.

## Задачи

- [x] **GEW-10 — Реализовать подписку по `event_id`**
  - Подписка регистрируется на фазе `register` и замораживается вместе с остальными registries.
  - Условие вида «переход именно из A в B» проверяется внутри обработчика по полям payload.
  - Done: подписка на неизвестный `event_id` отклоняется при регистрации, а не молча не срабатывает; двойная подписка одного модуля на один `event_id` отклоняется либо явно допускается с зафиксированным порядком; фильтров в API подписки нет.
  - Evidence: `Scripts/runtime/subscriber_registry.lua` (`M.create_registry`, `registry.register` с проверками `InvalidSubscriberId`/`InvalidEventId`/`InvalidSubscriberHandler`/`SubscriberDuplicateRegistration`, `registry.freeze` с `SubscriberRegistryFrozen`), `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` (freeze `events.subscribers`), `Tests/Lua/events/subscription.lua` (6 спека-кейсов), `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-11 — Ограничить права обработчика**
  - Обработчик читает state и repository, может поставить в очередь команду или новое событие.
  - Прямая мутация state запрещена: во время pump mutation window закрыт.
  - Done: попытка изменить state из обработчика даёт typed-ошибку; ошибка обработчика прекращает текущий pump, оставшиеся события отбрасываются и session переходит в `Failed`, при этом исходная команда остаётся committed.
  - Evidence: `Scripts/runtime/event_bus.lua` (pump при закрытом `mutation_window`, изоляция `pcall`, сброс очереди и переход в `phase = "failed"` при ошибках подписчика с сохранением состояния исходной команды), `Tests/Lua/events/handler_permissions.lua` (3 спека-кейса: отказ при мутации с `MutationWindowClosed`, сохранение committed state при ошибке подписчика, успешное чтение `state` и `repository`), `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-12 — Зафиксировать поведение отложенной команды**
  - Команда, поставленная обработчиком события, исполняется после завершения текущего pump и открывает собственное mutation window.
  - Done: последовательность «команда → факты → команда из обработчика → её факты» зафиксирована спекой в `Tests/Lua/events/`; вложенный синхронный вызов handler из handler отклоняется typed-ошибкой; переполнение очереди даёт typed rejection и не расходует sequence.
  - Evidence: `Scripts/runtime/command_dispatcher.lua` (`game.commands.enqueue`, `MAX_COMMAND_QUEUE_SIZE = 100`, отказ при переполнении `CommandQueueFull`, последовательное исполнение отложенных команд после завершения pump с открытием собственного окна мутации, отказ на синхронный `dispatch`), `Tests/Lua/events/deferred_commands.lua` (3 спека-кейса), `gv2-headless --self-test` и CTest (57/57 passed).

## Проверка milestone

- [x] Подписка работает по `event_id`, фильтров в API нет.
- [x] Обработчик события не может изменить state напрямую.
- [x] Отложенная команда исполняется в собственном окне и её порядок воспроизводим.
