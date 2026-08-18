---
title: RH Migration Tasks
status: draft
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

- [ ] **DLA-21 — Переписать команды `rh`**
  - Зависимости: DLA-09, DLA-16.
  - `travel`, `buy`, `work`, `wait_day` сегодня регистрируются через `game.commands.handlers.register` и проверяют условия отдельными валидаторами.
  - Done: команды объявлены присваиванием в дескриптор; условия отказа выражены `fail(key, params)` до первой мутации; расход выносливости и золота идёт через managed-операции; проверка связности локаций использует метод definition-обёртки; ни одного обращения к `game.*` в теле; поведение не изменилось — существующие спеки проходят с прежними ожиданиями, кроме кодов отказов, которые становятся ключами.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **DLA-22 — Переписать presentation `rh`**
  - Зависимости: DLA-19, DLA-21.
  - Меню локации собирается сегодня вручную из `button_list` с `text.spec` и жёстко заданной таблицей действий.
  - Done: экран локации собирается `show_screen` с кнопками переходов из `connected_location_ids` и действиями локации; тексты приходят через `text(...)`; перепубликация после действия сохраняется; ни одной сырой строки; спеки динамического меню проходят с прежними утверждениями о составе кнопок.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **DLA-23 — Удалить старый путь и синхронизировать документацию**
  - Зависимости: DLA-21, DLA-22.
  - Done: прямые вызовы `game.commands.handlers.register`, `game.commands.validators.register` и `game.events.subscribers.register` из `GameData/rh/scripts/` удалены, а не оставлены запасными; [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md), [Commands and Events](../../Architecture/CommandsAndEvents.md), [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md) и [Definition Envelope](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) описывают итоговое поведение; `Docs/Authoring/` переписан на новый синтаксис — это язык, на котором дизайнер работает; [Implementation Status](../../Status/ImplementationStatus.md) обновлён; в предложении проставлено `proposal_state: implemented`.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] В теле команд `rh` нет ни одного обращения к `game.*`.
- [ ] Ни одной пользовательской строки в геймплейном коде.
- [ ] Слайс проходится целиком: работа, покупка, переход, ожидание.
- [ ] `state_hash` до и после сохранения совпадает.
- [ ] Старый путь регистрации удалён.
- [ ] `Docs/Authoring/` описывает тот синтаксис, который работает.
