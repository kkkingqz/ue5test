---
title: RH Cleanup Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - ModuleDiscovery.md
  - DomainApi.md
  - PresentationSource.md
---

# M5 — RH Cleanup

> **Материализует:** всю поверхность на реальном геймплее.
> **Задачи:** SAS-17…21.
> **Результат:** правила игры — один файл; инфраструктурных Lua-файлов в пакете не остаётся.

## Результат этапа

Проверка поверхности делом. Если четыре правила не выражаются в сорока строках без обвязки, поверхность недостаточно высокого уровня — и это видно здесь.

Этап не начинается, пока не закрыты M1–M4: переписывать геймплей на половину поверхности значит переписывать дважды.

## Задачи

- [x] **SAS-17 — Слить designer-модули в один файл**
  - Зависимости: SAS-05, SAS-13.
  - `shop.lua`, `work.lua`, `travel.lua`, `time.lua` — 183 строки на четыре правила.
  - Done: создан `GameData/rh/scripts/authoring/gameplay.lua` с `work`, `wait_day`, `buy`, `travel`; старые четыре модуля удалены; в файле нет `M`, `return M`, `game.*`, литералов Stable ID, `current_location_id`, конвертов результата и вызовов презентации.
  - Evidence: `GameData/rh/scripts/authoring/gameplay.lua`, `Tests/Lua/actions/location_actions.lua`.

- [x] **SAS-18 — Одна команда покупки вместо команды на товар**
  - Зависимости: SAS-12, SAS-17.
  - Done: `shop.buy_sword` и `shop.buy_armor` заменены общей `buy(item)` (`rh:command.buy`); цена берётся из definition предмета (`item.price`); привязки в интерфейсе `location_screen.lua` передают ссылку на предмет аргументом действия `{ item = ... }`; добавление третьего товара не требует ни строки Lua.
  - Evidence: `GameData/rh/scripts/authoring/gameplay.lua`, `GameData/rh/scripts/presentation/location_screen.lua`, `Tests/Lua/actions/location_actions.lua`, `Tests/Lua/presentation/dynamic_menu.lua`, `Tests/Lua/authoring/simplified_surface.lua` (`general_buy_command_supports_arbitrary_item_definitions`).

- [x] **SAS-19 — Удалить инфраструктурные файлы пакета**
  - Зависимости: SAS-09, SAS-17.
  - Done: `scripts/manifest.lua` генерируется `generate_manifest.py`; `gameplay/root.lua` удалён; `services/economy.lua` и спека `Tests/Lua/economy/economy_service.lua` удалены, операции живут на акторе; `rh:service.economy` в реестре сервисов отсутствует.
  - Evidence: `GameData/rh/package.json5`, `GameData/rh/scripts/manifest.lua`, `Tools/Content/generate_manifest.py`.

- [x] **SAS-20 — Перевести экран локации на источник презентации**
  - Зависимости: SAS-16, SAS-17.
  - Done: `presentation/location_screen.lua` регистрируется как источник презентации (`game.presentation.register_source`) и не импортируется геймплеем; подписка на событие входа удалена — перестроение выполняет рантайм; спеки динамического меню проходят с прежними утверждениями.
  - Evidence: `GameData/rh/scripts/presentation/location_screen.lua`, `Tests/Lua/presentation/dynamic_menu.lua`.

- [x] **SAS-21 — Удалить `Docs/Authoring/` и синхронизировать документацию**
  - Зависимости: SAS-17–SAS-20.
  - Раздел описывает прежний синтаксис целиком и после переработки устаревает весь.
  - Done: каталог `Docs/Authoring/` удалён, ссылки на него убраны из `Docs/README.md`, `AGENTS.md`, `Tools/Documentation/validate_docs.py`; причина записана в `Docs/Status/ImplementationStatus.md`; в `Docs/Proposals/SimplifiedAuthoringSurfaceProposal.md` проставлено `proposal_state: implemented`.
  - Evidence: `Docs/README.md`, `AGENTS.md`, `Tools/Documentation/validate_docs.py`, `Docs/Status/ImplementationStatus.md`, `Docs/Proposals/SimplifiedAuthoringSurfaceProposal.md`.

## Проверка milestone

- [x] Правила игры — один файл; в нём нет обвязки.
- [x] `manifest.lua`, `root.lua`, `economy.lua` отсутствуют.
- [x] Добавление товара не требует Lua.
- [x] Геймплей не импортирует код презентации.
- [x] Слайс проходится целиком; `state_hash` до и после сохранения совпадает.
- [x] `Docs/Authoring/` удалён, документация на него не ссылается.
