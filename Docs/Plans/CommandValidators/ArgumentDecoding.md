---
title: Argument Decoding Tasks
status: normative
version: 1.0
updated: 2026-08-19
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0026-core-and-gameplay-ownership.md
  - ../../ADR/0028-simplified-authoring-surface.md
  - ../../ADR/0033-command-validator-authoring.md
---

# M1 — Argument Decoding

> **Материализует:** раздел «Единая семантика аргументов» [предложения](../../Proposals/CommandValidatorAuthoringProposal.md).
> **Задачи:** CVA-01…03.
> **Результат:** ядро не знает имён игровых параметров; декодирование существует в одном экземпляре и готово к переиспользованию валидатором.

## Результат этапа

Этап не добавляет валидаторов. Он снимает препятствие: предложение требует, чтобы обработчик и валидатор получали результат одной функции, а эта функция сегодня содержит список игровых имён внутри `core`.

Наблюдаемое поведение игры не меняется.

## Задачи

- [x] **CVA-01 — Создать ADR по авторским валидаторам**
  - Done: ADR фиксирует публичный `validate(command_ref, validator_name, validator_fn)`; формулу Stable ID валидатора и его непрозрачность; расширение `fail()` на scope валидатора; общий execution scope `none | command | validator | event`; перечень из четырёх точек, требующих охранника; разрешение target на заморозке; явную заменяемость команды; декодирование аргументов по форме значения. Отдельно записывает отвергнутое: `priority` в авторском API, отдельный реестр на команду, индекс `command_id → validators` до измерения, извлечение списка имён параметров как общего helper. Принят до первой отметки `[x]` ниже.
  - Evidence: `Docs/ADR/0033-command-validator-authoring.md`, `Docs/ADR/README.md`.

- [x] **CVA-02 — Убрать имена игровых параметров из ядра**
  - Зависимости: CVA-01.
  - `Scripts/authoring/context.lua` выбирает «главный аргумент» по списку `target_location_id`, `location_id`, `target`, `item_id`, `item`, `destination`. Это понятия `textsystem` и `rh` внутри `core` ([INV-016](../../Architecture/Invariants.md)); гейт их не ловит, потому что `validate_core_boundary.py` проверяет определения, схемы и идентификаторы, а не имена переменных.
  - Done: список удалён; декодирование определяется формой значения — массив даёт позиционные аргументы, пустая таблица даёт вызов без аргументов, иная таблица передаётся целиком; позиционный аргумент, являющийся валидным Stable ID и разрешимым в pinned repository, передаётся как definition wrapper по форме значения, а не по имени параметра; единственный вызов, полагавшийся на список (`textsystem` передаёт `{ target_location_id = conn_id }`), переведён на позиционную форму; наблюдаемое поведение перехода между локациями не изменилось.
  - Evidence: `Scripts/authoring/context.lua`, `GameData/textsystem/scripts/presentation/location_presenter.lua`, `Tests/Lua/world/`, `Tests/Lua/economy/`.

- [x] **CVA-03 — Единственное декодирование аргументов**
  - Зависимости: CVA-02.
  - Done: правила декодирования вынесены в один internal helper `decode_authoring_args(raw_args)` вместе с rehydration tagged references; обёртка обработчика вызывает только его; helper пригоден к переиспользованию обёрткой валидатора без изменений; спека покрывает позиционную, пустую и табличную формы и совпадение результата у двух вызывающих.
  - Evidence: `Scripts/authoring/context.lua`, `Tests/Lua/authoring/`.

## Проверка milestone

- [x] В `Scripts/` не осталось имён игровых параметров.
- [x] Позиционная, пустая и табличная формы декодируются одной функцией.
- [x] Переход между локациями и покупка работают как прежде.
- [x] В golden изменились только `script_set_hash` и производный `digest_hash`.
