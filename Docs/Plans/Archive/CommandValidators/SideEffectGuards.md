---
title: Side Effect Guards Tasks
status: archived
version: 1.0
updated: 2026-08-19
depends_on:
  - ValidatorAuthoring.md
  - ../../../Architecture/CommandsAndEvents.md
decisions:
  - ../../../ADR/0003-command-and-event-model.md
  - ../../../ADR/0028-simplified-authoring-surface.md
---

# M3 — Side Effect Guards

> **Материализует:** раздел «Read-only permission scope» [предложения](../../../Proposals/Archive/CommandValidatorAuthoringProposal.md).
> **Задачи:** CVA-09…11.
> **Результат:** валидатор не может произвести наблюдаемый эффект, и это проверено на отказ в каждой точке.

## Результат этапа

Валидатор остаётся read-only не по соглашению, а по проверке.

Объём этапа ограничен перечнем. Валидатор исполняется внутри `dispatch`, где `is_dispatching = true`, а окно мутации закрыто, поэтому запись в каноническое состояние и вложенный `run(...)` уже отклоняются существующими механизмами, а поздние регистрации — заморозкой реестров. Новых охранников требуют ровно четыре точки.

## Задачи

- [x] **CVA-09 — Охранники в четырёх точках**
  - Зависимости: CVA-08.
  - Done: в scope `validator` отклоняются `emit(...)`, `show_screen(...)`, `commands.*:later(...)` и мутирующие точки входа Gameplay Service; отказ — programmer fault `AuthoringValidatorSideEffectDisallowed` с указанием пакета, ID валидатора и попытанной операции, а не типизированный геймплейный отказ; проверка выполняется в общем boundary, а не локальными соглашениями.
  - Evidence: `Scripts/authoring/context.lua`, `Scripts/runtime/`, `Tests/Lua/authoring/command_validators.lua`.

- [x] **CVA-10 — Спека на отказ в каждой точке**
  - Зависимости: CVA-09.
  - Done: спека покрывает по одному отрицательному случаю на каждую из четырёх точек плюс запись в каноническое состояние и вложенный `run(...)`, отклоняемые существующими механизмами; отдельно проверяется, что разрешённое остаётся разрешённым — чтение `player`, `world`, обёрток и определений, read-only запросы к репозиторию, чистые вычисления и построение portable `params`.
  - Evidence: `Tests/Lua/authoring/command_validators.lua`.

- [x] **CVA-11 — Область действия и порядок относительно поиска обработчика**
  - Зависимости: CVA-10.
  - Done: в contract записано, что валидаторы применяются к каждому dispatch независимо от источника, в том числе к отложенному `commands.*:later(...)`, и что внутренним вызовом их обойти нельзя; отдельно записано, что `run_validators` выполняется до проверки наличия обработчика, поэтому для неизвестной команды сначала отрабатывают валидаторы, а гарантия наличия target держится на проверке при заморозке; оба утверждения покрыты спеками.
  - Evidence: `Docs/Architecture/CommandsAndEvents.md`, `Tests/Lua/authoring/command_validators.lua`.

## Проверка milestone

- [x] Каждая из четырёх точек отклоняется из валидатора с `AuthoringValidatorSideEffectDisallowed`.
- [x] Разрешённые операции чтения продолжают работать.
- [x] Отложенный вызов проходит цепочку валидаторов.
- [x] Перечень точек записан в contract; добавление нового helper с эффектом обязано его расширять.
