---
title: Domain API Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - AuthoringEnvironment.md
  - ../../Architecture/CommandsAndEvents.md
---

# M3 — Domain API

> **Материализует:** [Commands and Events](../../Architecture/CommandsAndEvents.md) в части предусловий и доменных операций.
> **Задачи:** SAS-10…13.
> **Результат:** правило игры выражается предусловием и операцией, без разбора результатов и транспорта.

## Результат этапа

`player:require_gold(price)` и `player:spend_gold(price)` — два вызова с разным смыслом, и оба читаются без пояснений.

## Задачи

- [ ] **SAS-10 — Предусловия `require_*`**
  - Зависимости: SAS-04.
  - Done: `require_location`, `require_stamina`, `require_gold` ничего не мутируют; при выполненном условии возвращают управление, при невыполненном дают типизированный отказ команды с параметрами; отказ до первой мутации остаётся обычным отказом, после — `AuthoringFailAfterMutation` по существующему правилу; rollback не вводится.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **SAS-11 — Операции `spend_*` без разбора результата**
  - Зависимости: SAS-10.
  - Done: успешная операция возвращает управление, и дизайнер не пишет проверку результата; **невыполненное предусловие в `spend_*` даёт fault, а не отказ** — если `require_*` является санкционированной проверкой, то отказ списания означает, что её забыли, и сообщение обязано указывать на пропущенную проверку, а не на нехватку ресурса; автопродвижение реализуется нелокальным выходом, и ограничение записано: обёртывание вызова в `pcall` внутри designer-кода перехватит сентинел и сломает механизм.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **SAS-12 — Обработчик получает объекты**
  - Зависимости: SAS-04.
  - `travel.lua` сегодня вручную разбирает транспортный DTO, а designer-код смешивает `current_location`, `current_location_id` и `world.current_location`.
  - Done: аргумент команды приходит обработчику готовым handle, разбора DTO в designer-коде не остаётся; в designer-facing API остаётся только `player.current_location`, возвращающая `DefinitionHandle<Location>`; формы с `_id` из designer-facing поверхности убраны, canonical ID остаётся внутренним представлением; negative case на передачу непереносимого значения аргументом.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **SAS-13 — Единый доменный API актора**
  - Зависимости: SAS-11.
  - Done: `add_gold`, `spend_gold`, `add_stamina`, `spend_stamina`, `add_item` и `travel` существуют в одном месте — на акторе; двойная форма вызова `function(a, b)` убрана в пользу устойчивого colon-синтаксиса; смена локации выполняется `player:travel(target)`, который меняет состояние и публикует сопутствующие факты, а проверка связности остаётся в команде; при необходимости добавляется `location:require_connected(target)`.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] `require_*` даёт отказ, `spend_*` при непроверенном предусловии — fault.
- [ ] Designer-код не разбирает результат доменной операции.
- [ ] Аргумент команды приходит handle, а не строкой или таблицей.
- [ ] `current_location_id` в designer-facing API отсутствует.
