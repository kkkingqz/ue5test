---
title: Handler Replacement Tasks
status: normative
version: 1.0
updated: 2026-08-19
depends_on:
  - SideEffectGuards.md
  - ../../Architecture/Modding.md
decisions:
  - ../../ADR/0025-lua-module-replacement-and-export-freezing.md
  - ../../ADR/0032-field-contracts-and-generic-instance-creation.md
---

# M4 — Handler Replacement

> **Материализует:** раздел «Mods и override» [предложения](../../Proposals/CommandValidatorAuthoringProposal.md).
> **Задачи:** CVA-12…13.
> **Результат:** замена обработчика перестаёт быть молчаливой; политики чужих пакетов переживают её осознанно.

## Результат этапа

Валидаторы впервые делают замену обработчика значимой: policy чужого пакета продолжает ограничивать команду, реализацию которой она больше не видела. Пока замена происходит молча, это небезопасно.

## Задачи

- [ ] **CVA-12 — Явная заменяемость команды**
  - Зависимости: CVA-11.
  - Авторский адаптер регистрирует обработчик как `register(cmd_id, wrapped_handler, { override = exists })`: более поздний пакет заменяет существующий обработчик без единого сигнала.
  - Done: команда, допускающая замену, помечается заменяемой явно; повторная регистрация обработчика для незаменяемой команды отклоняется `CommandNotReplaceable` с указанием обоих пакетов; законная замена сохраняет валидаторы, объявленные другими пакетами; сохранение argument contract опубликованного `command_id` записано в contract как обязанность заменяющего пакета; форма совпадает с [ADR-0025](../../ADR/0025-lua-module-replacement-and-export-freezing.md) и [ADR-0032](../../ADR/0032-field-contracts-and-generic-instance-creation.md) — запечатано по умолчанию, заменяемо по объявлению; отрицательный случай покрыт спекой.
  - Evidence: `Scripts/authoring/context.lua`, `Docs/Architecture/Modding.md`, `Tests/Lua/authoring/command_validators.lua`.

- [ ] **CVA-13 — Сквозная верификация**
  - Зависимости: CVA-12.
  - Done: одна spec suite `Tests/Lua/authoring/command_validators.lua` исполняется обоими хостами через общий путь и покрывает полный перечень критериев приёмки предложения; дополнительной копии проверок на C++ не создано; полный `ctest`, `gv2-headless --self-test`, `--check-scripts`, `validate_docs.py`, `validate_core_boundary.py` и Unreal automation зелёные; в golden изменились только `script_set_hash` и производный `digest_hash`, воспроизведённые манифестом; `repository_content_hash`, хэш состояния, `final_screen_id` и `final_screen_fields` не изменились; contracts `CommandsAndEvents.md`, `LuaRuntimeContract.md`, `Modding.md`, `GameplayModel.md`, `AddCommand.md` и `ImplementationStatus.md` синхронизированы.
  - Evidence: отчёт CTest, golden-прогон, обновлённые contracts.

## Проверка milestone

- [ ] Повторная регистрация обработчика для незаменяемой команды отклоняется.
- [ ] Валидаторы чужих пакетов переживают законную замену.
- [ ] Одна spec suite исполняется обоими хостами; копии на C++ нет.
- [ ] Документация синхронизирована в том же change set.
