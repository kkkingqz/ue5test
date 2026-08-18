---
title: Commands Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - Foundation.md
  - ../../Architecture/CommandsAndEvents.md
---

# M2 — Commands

> **Материализует:** [Commands and Events](../../Architecture/CommandsAndEvents.md) в designer-facing форме.
> **Задачи:** DLA-05…09.
> **Результат:** команда объявляется присваиванием в дескриптор, отказ типизирован, время исполнения видно в коде.

## Результат этапа

Геймплейная команда пишется как обычная Lua-функция, а Stable ID, момент регистрации и конверт остаются за слоем.

## Задачи

- [x] **DLA-05 — Дескриптор модуля и прокси команд**
  - Зависимости: DLA-01.
  - `commands.buy = function … end` и `commands.buy:run(sword)` несовместимы в обычной таблице, поэтому `commands` — прокси.
  - Done: `rh.gameplay()` / `authoring.gameplay(package_id)` возвращает дескриптор; `__newindex` принимает только функцию и отвергает повторное присваивание ключа (`CommandAlreadyDefined`); `__index` по неизвестному ключу после freeze даёт **ошибку `UnknownCommandKey`, а не `nil`**; дескриптор команды стабилен для ключа, поэтому вычисленный при загрузке `action(commands.buy, …)` остаётся валидным; ключ канонизируется в `<package_id>:command.<key>`, `commands["shop.buy"]` → `rh:command.shop.buy`.
  - Evidence: `Scripts/authoring/commands.lua`, `Scripts/authoring/context.lua`, спека `Tests/Lua/authoring/commands.lua` (`commands_proxy_declaration_and_descriptor_stability`, `commands_proxy_errors_on_unknown_key_after_freeze`).

- [x] **DLA-06 — Отложенная регистрация**
  - Зависимости: DLA-05.
  - Реестры замораживаются после фазы `register`, а присваивание в дескриптор происходит при загрузке модуля.
  - Done: дескриптор накапливает команды, ничего не регистрируя при загрузке; `M.register(ctx)` обходит его на фазе `register` и выполняет настоящую регистрацию в `game.commands.handlers.register` с детерминированной сортировкой по `command_id`; присваивание после freeze — ошибка `CommandDeclarationAfterFreeze`.
  - Evidence: `Scripts/authoring/context.lua`, спека `Tests/Lua/authoring/commands.lua` (`commands_proxy_errors_on_unknown_key_after_freeze`).

- [x] **DLA-07 — Канонизация аргументов и обратное превращение**
  - Зависимости: DLA-04, DLA-05.
  - Done: обёртка на границе слоя разворачивается в помеченную ссылку `{ __gv2_ref = "instance" | "definition", id = … }`, рекурсивно по вложенным таблицам; на входе в обработчик помеченная ссылка превращается в свежую обёртку (`game.instances.actors.get` / `game.repository.get`), а обычная строка, совпадающая со Stable ID, остаётся строкой; правило одинаково для `:run()`, `:later()` и `action()`; помеченная ссылка проходит проверку переносимости без исключений.
  - Evidence: `Scripts/authoring/tagged_ref.lua`, `Scripts/authoring/commands.lua`, спека `Tests/Lua/authoring/commands.lua` (`argument_canonicalization_and_rehydration`, `action_produces_semantic_action_dto`).

- [x] **DLA-08 — `fail()` с ключом и правилом мутации**
  - Зависимости: DLA-02, DLA-04.
  - Done: `fail(key, params)` даёт `<package_id>:error.<key>`; `params` канонизируются и проверяются общим примитивом `portable_value.validate`, обратного превращения не получают; вызов до первой мутации — типизированный отказ обработчика `{ ok = false, error = ... }`; после — `AuthoringFailAfterMutation` с указанием команды и требованием перенести проверки выше; признак берётся сравнением `write_revision` на входе и в момент вызова; negative case на оба случая.
  - Evidence: `Scripts/authoring/context.lua`, спека `Tests/Lua/authoring/commands.lua` (`fail_before_mutation_returns_typed_refusal`, `fail_after_mutation_throws_authoring_fail_after_mutation`).

- [x] **DLA-09 — `run()` и `later()`**
  - Зависимости: DLA-07.
  - Done: `:run()` — синхронная диспетчеризация, команда получает собственное окно мутации; вызов из активного обработчика отвергается как вложенная диспетчеризация (`AuthoringNestedRunDisallowed`); `:later()` кладёт переносимый DTO в очередь `game.commands.enqueue` и исполняется в своём окне; автоматического выбора между ними нет; спеки покрывают запрет вложенности и порядок исполнения отложенных команд.
  - Evidence: `Scripts/authoring/commands.lua`, `Scripts/authoring/context.lua`, спека `Tests/Lua/authoring/commands.lua` (`nested_run_rejected_from_inside_command_handler`, `later_enqueues_and_executes_in_deferred_queue`).

## Проверка milestone

- [x] Команда объявляется присваиванием и регистрируется на фазе `register`.
- [x] Опечатка в имени команды падает сразу, а не даёт `nil`.
- [x] Обёртка в аргументах доходит до обработчика обёрткой, строка — строкой.
- [x] `fail()` после мутации даёт ошибку, а не отказ.
- [x] `:run()` из обработчика отвергается.
