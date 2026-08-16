---
title: Package Commands Tasks
status: archived
version: 1.0
updated: 2026-08-16
depends_on:
  - DispatchByKey.md
  - ../../../Architecture/Modding.md
---

# M3 — Package Commands

> **Материализует:** [Modding § Lua modules](../../../Architecture/Modding.md), [Commands and Events](../../../Architecture/CommandsAndEvents.md).
> **Задачи:** CHR-08…10.
> **Результат:** пакет добавляет команду, не трогая ядро, и это зафиксировано контрактом.

## Результат этапа

Проверено делом: команда из пакета проходит полный путь — валидаторы, окно мутации, события — и исполняется обоими хостами. Правка ядра для этого не потребовалась ни в Lua, ни в C++.

## Задачи

- [x] **CHR-08 — Команда из пакета**
  - Зависимости: CHR-07.
  - Done: фикстура мод-пакета регистрирует собственный обработчик под собственным namespace и исполняет команду; путь покрыт валидатором и публикацией события, чтобы проверялся весь цикл, а не только вызов; набор исполняется обоими хостами; ни один файл `Scripts/` и ни одна строка C++ для этого не менялись — это и есть критерий приёмки задачи.
  - Evidence: Создан набор тестов `Tests/Lua/commands/package_commands.lua`, покрывающий полный цикл исполнения команды пакета `test_mod:command.craft_item` (валидатор `test_mod:validator.craft_check`, обработчик с мутацией `game.state.test_crafted_item`, событие `test_mod:event.item_crafted` и подписчик `test_mod:subscriber.craft_listener`). В `Scripts/` и C++ изменений не вносилось.

- [x] **CHR-09 — Перекрытие команды пакетом**
  - Зависимости: CHR-08.
  - Done: пакет, регистрирующий уже занятый `command_id` без `options.override`, получает `CommandHandlerDuplicateRegistration` и блокирует старт сессии; с `options.override = true` его обработчик заменяет прежний; заявленный `override` над свободным `command_id` — отдельная ошибка; оба случая покрыты спеками; в contract записано, что правило «поздний пакет побеждает» для команд сознательно не вводится.
  - Evidence: `Tests/Lua/commands/package_commands.lua` покрывает `duplicate_registration_without_override_rejected`, `explicit_override_replaces_existing_handler` и `override_on_unregistered_command_rejected`.

- [x] **CHR-10 — Синхронизировать документацию**
  - Зависимости: CHR-08, CHR-09.
  - Done: [Commands and Events](../../../Architecture/CommandsAndEvents.md) описывает диспетчеризацию по ключу, отказ на неизвестную команду и порядок фаз без цепочки; [Lua Runtime Contract](../../../Architecture/LuaRuntimeContract.md) получает раздел `game.commands.handlers` рядом с `game.commands.validators`; [Modding](../../../Architecture/Modding.md) описывает регистрацию команд пакетом и правило перекрытия; guide [AddCommand](../../../Guides/AddCommand.md) переписан на регистрацию вместо правки `ingress`/`root`; [Implementation Status](../../../Status/ImplementationStatus.md) отмечает, что добавление команды больше не требует C++.
  - Evidence: Обновлены `CommandsAndEvents.md`, `LuaRuntimeContract.md`, `Modding.md`, `AddCommand.md`, `ImplementationStatus.md`.

## Проверка milestone

- [x] Команда из пакета исполняется в обоих хостах без правки ядра.
- [x] Валидатор и событие из пакета работают на том же пути.
- [x] Дубликат и `override` покрыты раздельными спеками.
- [x] Guide описывает добавление команды одной регистрацией.
