---
title: RH Migration Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - EventsAndPresentation.md
  - RuntimeState.md
---

# M6 — RH Migration

> **Материализует:** весь слой на реальном геймплее.
> **Задачи:** DLA-21…23.
> **Результат:** геймплей `rh` написан на designer-facing Lua, старый путь удалён.

## Результат этапа

Проверка слоя делом: если текущий слайс не выражается на нём без обращений к фасаду, слой недостаточно высокого уровня, и это видно здесь, а не через полгода.

Этап не начинается, пока не закрыты обе ветки плана: переписывать геймплей на половину слоя значит переписывать дважды.

## Задачи

- [x] **DLA-21 — Переписать команды `rh`**
  - Зависимости: DLA-09, DLA-16.
  - `travel`, `buy`, `work`, `wait_day` переписаны на дескриптор `local M = authoring.gameplay("rh")`.
  - Done: команды объявлены присваиванием в `M.commands`; условия отказа выражены `fail(key, params)` до первой мутации; расход выносливости и золота идёт через managed-операции и методы актора (`spend_stamina`, `spend_gold`, `add_gold`, `add_stamina`, `add_item`); проверка связности локаций использует метод definition-обёртки `is_connected`; ни одного обращения к `game.*` в теле команд; все спеки действий локаций и путешествий проходят.
  - Evidence: `GameData/rh/scripts/gameplay/travel.lua`, `shop.lua`, `time.lua`, `work.lua`, `actors.lua`.

- [x] **DLA-22 — Переписать presentation `rh`**
  - Зависимости: DLA-19, DLA-21.
  - Меню локации и переходы переписаны на `M.show_screen`, `M.button`, `M.action` и `M.text`.
  - Done: экран локации собирается `M.show_screen` с кнопками переходов из `connected_location_ids` и действиями локации; тексты приходят через `M.text(...)`; перепубликация после действия сохраняется; ни одной сырой строки; подписка на событие через `M.on("core:event.location.enter", ...)`; спеки динамического меню проходят с прежними утверждениями.
  - Evidence: `GameData/rh/scripts/presentation/location_screen.lua`, `Tests/Lua/presentation/dynamic_menu.lua`.

- [x] **DLA-23 — Удалить старый путь и синхронизировать документацию**
  - Зависимости: DLA-21, DLA-22.
  - Done: прямые вызовы `game.commands.handlers.register`, `game.commands.validators.register` и `game.events.subscribers.register` из `GameData/rh/scripts/gameplay/` и `presentation/` удалены; `Docs/Authoring/` актуализирован; `collect_texts.py` применён к `GameData/rh`; [Implementation Status](../../../Status/ImplementationStatus.md) обновлён; в предложении проставлено `proposal_state: implemented`.
  - Evidence: `Docs/Authoring/*`, `Docs/Proposals/DesignerLuaAuthoringProposal.md`, `Docs/Status/ImplementationStatus.md`.

## Проверка milestone

- [x] В теле команд `rh` нет ни одного обращения к `game.*`.
- [x] Ни одной пользовательской строки в геймплейном коде.
- [x] Слайс проходится целиком: работа, покупка, переход, ожидание.
- [x] `state_hash` до и после сохранения совпадает.
- [x] Старый путь регистрации удалён.
- [x] `Docs/Authoring/` описывает тот синтаксис, который работает.
