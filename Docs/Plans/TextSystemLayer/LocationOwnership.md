---
title: Location Ownership Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - PackageSet.md
  - ../../Architecture/CanonicalStateAndSave.md
---

# M2 — Location Ownership

> **Материализует:** трёхуровневую границу на первой реальной подсистеме.
> **Задачи:** TSL-06…10.
> **Результат:** локация и переход принадлежат `textsystem`, стоимость перехода — игре, ядро о переходах не знает.

## Результат этапа

`textsystem` знает, что переход возможен; `rh` — чего он стоит; `core` не знает ни того, ни другого.

## Задачи

- [ ] **TSL-06 — Базовый актор и его локация**
  - Зависимости: TSL-05.
  - Done: `textsystem` объявляет базовую схему актора с `current_location` типа `ref_definition<location>` и регистрирует ссылочное поле; `require_location(location)` ничего не мутирует и даёт типизированный отказ; параллельной формы с `_id` в designer-facing API не остаётся, включая имена параметров отказов; соответствующий код удалён из `rh/actors.lua`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **TSL-07 — Операция перехода**
  - Зависимости: TSL-06.
  - Done: `move_to(location)` меняет каноническое состояние и публикует факты выхода и входа; операция принадлежит `textsystem` и ничего не знает о стоимости; событие входа продолжает перестраивать экран через источник презентации, а не через вызов из геймплея; спека покрывает публикацию обоих фактов и порядок.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **TSL-08 — Топология локаций**
  - Зависимости: TSL-06.
  - Done: `textsystem` владеет схемой локации со связностью; декоратор определения даёт `is_connected(target)` и `require_connected(target)`; схема локации переезжает из `rh` — это второй её переезд за неделю, и он должен быть выполнен `gv2-content rename`, а не копированием; `rh` описывает свои локации по схеме `textsystem`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **TSL-09 — Переход как команда игры**
  - Зависимости: TSL-07, TSL-08.
  - Done: `rh:command.travel` объявлен в authoring-файле `rh` и содержит только цену и допустимость: `require_connected`, `require_stamina`, `spend_stamina`, `move_to`; перекрытие `core:command.location.travel` удалено; привязки интерфейса и спеки переведены на новый идентификатор.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **TSL-10 — Убрать переход из ядра**
  - Зависимости: TSL-09.
  - Done: `core:command.location.travel`, `core:service.location` и `Scripts/gameplay/location_service.lua` удалены; у `Scripts/gameplay/root.lua` не остаётся обработчиков — модуль и каталог `Scripts/gameplay/` удалены; гейт границы ядра дополнен запретом на возврат словаря локаций и переходов; contracts описывают итоговое владение.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] Переход работает и стоит выносливости.
- [ ] `Scripts/gameplay/` в ядре отсутствует.
- [ ] `rh/actors.lua` не содержит кода локации.
- [ ] Форм с `_id` в designer-facing API не осталось.
- [ ] Слайс проходится целиком; `state_hash` до и после сохранения совпадает.
