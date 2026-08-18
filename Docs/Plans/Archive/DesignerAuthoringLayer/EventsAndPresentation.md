---
title: Events and Presentation Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - Commands.md
  - ../../../Architecture/CommandsAndEvents.md
  - ../../../UI/ScreenTemplates.md
---

# M5 — Events and Presentation

> **Материализует:** [Commands and Events](../../../Architecture/CommandsAndEvents.md) и [Screen Templates](../../../UI/ScreenTemplates.md) в designer-facing форме.
> **Задачи:** DLA-17…20.
> **Результат:** событие и экран описываются без конвертов и DTO, ни одной пользовательской строки в коде.

## Результат этапа

Экран собирается из кнопок с привязкой к командам, событие публикуется одной строкой, тексты приходят из каталога.

## Задачи

- [x] **DLA-17 — `emit` и `on`**
  - Зависимости: DLA-06, DLA-07.
  - Конверт события отвергает таблицу с метатаблицей, поэтому обёртку в payload положить нельзя.
  - Done: `emit(name, payload)` канонизирует имя в `rh:event.<name>` и разворачивает обёртки помеченными ссылками; при доставке подписчик получает свежие обёртки; в contract записано, что подписчик видит **текущее** состояние, а не снимок на момент публикации; `on(name, fn)` накапливается в дескрипторе модуля и регистрируется на фазе `register`.
  - Evidence: `Scripts/authoring/context.lua`, `Scripts/authoring/tagged_ref.lua`, тест `emit_and_on_roundtrip_with_wrapper_rehydration` в `Tests/Lua/authoring/events_and_presentation.lua`.

- [x] **DLA-18 — `action` и `button`**
  - Зависимости: DLA-07, DLA-09.
  - Done: `action(command_descriptor, ...)` принимает дескриптор команды и переносимые аргументы, но **не произвольное замыкание** — иначе из него нельзя получить semantic input; результат — пара `command_id` и canonical args, канонизированных тем же правилом границы; `action` ничего не исполняет; `button(text_spec, action)` даёт элемент списка кнопок; передача замыкания отвергается типизированной ошибкой.
  - Evidence: `Scripts/authoring/presentation.lua` (`create_action_helper`, `create_button_helper`), тесты `action_accepts_command_descriptor_and_rejects_closure`, `button_and_show_screen_reject_raw_strings` в `Tests/Lua/authoring/events_and_presentation.lua`.

- [x] **DLA-19 — `show_screen` и тексты**
  - Зависимости: DLA-18.
  - Done: `show_screen{ template, description, buttons }` собирает и публикует желаемое состояние экрана; `template` принимает definition-хэндл в форме `<package>.def.screen("…")`; `text("key")` даёт `TextSpec` по `<package>:text.<key>`; сырая строка в любом текстовом поле отвергается; соответствие «код ошибки → текст» задано соглашением об именовании и записано в contract, чтобы presentation знал правило.
  - Evidence: `Scripts/authoring/presentation.lua` (`create_text_helper`, `create_show_screen_helper`), `Docs/UI/ScreenTemplates.md`, тесты `text_spec_canonical_ids_and_error_convention`, `button_and_show_screen_reject_raw_strings` в `Tests/Lua/authoring/events_and_presentation.lua`.

- [x] **DLA-20 — Инструмент сбора текстов**
  - Зависимости: DLA-08, DLA-19.
  - Done: инструмент обходит Lua-исходники пакета, собирает **литеральные** вызовы `text(...)` и ключи `fail(...)`, находит отсутствующие `text`-definitions и записи PO-каталога и дописывает их; вычисляемые аргументы не собираются, и это записано как граница инструмента; повторный запуск на неизменном входе ничего не меняет; инструмент не трогает уже существующие переводы.
  - Evidence: `Tools/Content/collect_texts.py`, тест 36 в `Tools/Content/test_authoring_tools.py` (CTest #52 `gv2_content_authoring_tools_python`).

## Проверка milestone

- [x] Обёртка в payload события доходит до подписчика обёрткой.
- [x] `action` с замыканием отвергается.
- [x] Сырая строка в тексте экрана или кнопки отвергается.
- [x] Инструмент находит недостающие тексты интерфейса и отказов и идемпотентен.
