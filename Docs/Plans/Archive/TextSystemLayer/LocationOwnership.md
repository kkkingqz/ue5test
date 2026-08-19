---
title: Location Ownership Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - PackageSet.md
  - ../../../Architecture/CanonicalStateAndSave.md
---

# M2 — Location Ownership

> **Материализует:** трёхуровневую границу на первой реальной подсистеме.
> **Задачи:** TSL-06…10.
> **Результат:** локация и переход принадлежат `textsystem`, стоимость перехода — игре, ядро о переходах не знает.

## Результат этапа

`textsystem` знает, что переход возможен; `rh` — чего он стоит; `core` не знает ни того, ни другого.

## Задачи

- [x] **TSL-06 — Базовый актор и его локация**
  - Зависимости: TSL-05.
  - Done: `textsystem` объявляет базовую схему актора с `current_location` типа `ref_definition<location>` и регистрирует ссылочное поле; `require_location(location)` ничего не мутирует и даёт типизированный отказ; параллельной формы с `_id` в designer-facing API не остаётся, включая имена параметров отказов; соответствующий код удалён из `rh/actors.lua`.
  - Evidence: `GameData/textsystem/scripts/gameplay/actors.lua` (`actor_decorator.require_location`, регистрация reference field `current_location`), `Tests/Lua/actions/location_actions.lua` (параметры отказа `required_location`/`current_location`), `Scripts/runtime/actor_registry.lua` (композиция цепочки декораторов `register_type`), код локаций удалён из `GameData/rh/scripts/gameplay/actors.lua`.

- [x] **TSL-07 — Операция перехода**
  - Зависимости: TSL-06.
  - Done: `move_to(location)` меняет каноническое состояние и публикует факты выхода и входа; операция принадлежит `textsystem` и ничего не знает о стоимости; событие входа продолжает перестраивать экран через источник презентации, а не через вызов из геймплея; спека покрывает публикацию обоих фактов и порядок.
  - Evidence: `GameData/textsystem/scripts/gameplay/actors.lua` (`actor_decorator.move_to`), публикация событий `textsystem:event.location.leave` и `textsystem:event.location.enter` в строгом порядке, спеки `Tests/Lua/world/travel_events.lua` и `Tests/Lua/world/travel_stamina.lua`.

- [x] **TSL-08 — Топология локаций**
  - Зависимости: TSL-06.
  - Done: `textsystem` владеет схемой локации со связностью; декоратор определения даёт `is_connected(target)` и `require_connected(target)`; схема локации переезжает из `rh` — это второй её переезд за неделю, и он должен быть выполнен `gv2-content rename`, а не копированием; `rh` описывает свои локации по схеме `textsystem`.
  - Evidence: `GameData/textsystem/schemas/location_v1.schema.json5` (`textsystem:schema.definition.location.v1`), декоратор `location_definition_decorator` в `GameData/textsystem/scripts/gameplay/actors.lua` (`is_connected`, `require_connected`), `GameData/rh/schemas/location_v1.schema.json5` удалена, локации `rh` соответствуют схеме `textsystem`.

- [x] **TSL-09 — Переход как команда игры**
  - Зависимости: TSL-07, TSL-08.
  - Done: `rh:command.travel` объявлен в authoring-файле `rh` и содержит только цену и допустимость: `require_connected`, `require_stamina`, `spend_stamina`, `move_to`; перекрытие `core:command.location.travel` удалено; привязки интерфейса и спеки переведены на новый идентификатор.
  - Evidence: `GameData/rh/scripts/authoring/gameplay.lua` (`rh:command.travel`), UI привязка в `GameData/rh/scripts/presentation/location_screen.lua`, спеки `Tests/Lua/world/travel_command.lua`, `Tests/Lua/world/travel_stamina.lua`, `Tests/Lua/presentation/dynamic_menu.lua`.

- [x] **TSL-10 — Убрать переход из ядра**
  - Зависимости: TSL-09.
  - Done: `core:command.location.travel`, `core:service.location` и `Scripts/gameplay/location_service.lua` удалены; у `Scripts/gameplay/root.lua` не остаётся обработчиков — модуль и каталог `Scripts/gameplay/` удалены; гейт границы ядра дополнен запретом на возврат словаря локаций и переходов; contracts описывают итоговое владение.
  - Evidence: Удалены `Scripts/gameplay/location_service.lua`, `Scripts/gameplay/root.lua` и каталог `Scripts/gameplay/`; обновлены `Scripts/bootstrap/main.lua`, `manifest.lua`, `properties.lua`; `Tools/Content/validate_core_boundary.py` проверяет отсутствие `Scripts/gameplay` и ссылок на location в ядре; `Docs/Architecture/LuaRuntimeContract.md` и `Docs/Status/ImplementationStatus.md` обновлены; 79/79 CTest тестов и `gv2-headless --self-test` пройдены.

## Проверка milestone

- [x] Переход работает и стоит выносливости.
- [x] `Scripts/gameplay/` в ядре отсутствует.
- [x] `rh/actors.lua` не содержит кода локации.
- [x] Форм с `_id` в designer-facing API не осталось.
- [x] Слайс проходится целиком; `state_hash` до и после сохранения совпадает.
