---
title: Authoring Environment Tasks
status: archived
version: 1.0
updated: 2026-08-18
depends_on:
  - README.md
  - ../../../Architecture/LuaRuntimeContract.md
---

# M1 — Authoring Environment

> **Материализует:** [Lua Runtime Contract § Module loader](../../../Architecture/LuaRuntimeContract.md) в части окружения authoring-скриптов.
> **Задачи:** SAS-01…05.
> **Результат:** файл правил игры содержит только правила игры.

## Результат этапа

Из designer-facing файла исчезают три строки обвязки в начале, `return M` в конце и префикс `M.` в каждой строке между ними.

## Задачи

- [x] **SAS-01 — Создать ADR по окружению authoring-скрипта**
  - Контракт говорит: «Per-module environments и capability sandbox в v1 отсутствуют». Решение о прологе из локальных принималось два круга назад именно чтобы не менять это правило.
  - Done: ADR фиксирует отдельное лексическое окружение для `scripts/authoring/`, правила вывода имён и то, что окружение не является границей безопасности; отдельно записывает, **что не сошлось в прежнем решении** — цена была не в строке пролога, а в префиксе на каждой ссылке; отмечает, что правило «module source не создаёт globals» остаётся истинным и правки требует только фраза про отсутствие per-module окружений; принят до первой отметки `[x]` ниже.
  - Evidence: [ADR-0028](../../../ADR/0028-simplified-authoring-surface.md), [ADR Index](../../../ADR/README.md).

- [x] **SAS-02 — Класс authoring-скриптов и их окружение**
  - Зависимости: SAS-01.
  - Загрузчик исполняет модуль через `luaL_loadbufferx` плюс `lua_pcall`, поэтому подмена `_ENV` выполняется установкой первого upvalue чанка.
  - Done: файлы под `scripts/authoring/` пакета получают окружение с именами `commands`, `player`, `world`, `def`, `fail`, `emit`, `on`, `text`, `button`, `show_screen`; окружение знает namespace своего пакета, поэтому `commands.work` даёт `rh:command.work`, а `def.location("city.tavern")` — `rh:location.city.tavern`; попытка объявить глобальную переменную в authoring-скрипте отвергается; модули программиста продолжают использовать обычное окружение и `require`.
  - Evidence: `Scripts/authoring/context.lua:create_authoring_environment`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Tests/Lua/authoring/simplified_surface.lua`.

- [x] **SAS-03 — Дескриптор создаётся загрузчиком**
  - Зависимости: SAS-02.
  - Done: дескриптор существует до исполнения файла, поэтому `return M` не требуется и возвращённое значение authoring-скрипта игнорируется; регистрация по-прежнему откладывается до фазы `register`; порядок обхода дескриптора детерминирован.
  - Evidence: `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `Tests/Lua/authoring/simplified_surface.lua`.

- [x] **SAS-04 — Неявный успех команды**
  - Зависимости: SAS-03.
  - Done: `return nil` — успех без значения, `return value` — успех со значением, `return fail(...)` — типизированный отказ, непойманная ошибка — fault; **отказом является только объект, произведённый `fail()`**, любое другое возвращённое значение считается значением, даже если содержит поле `ok`; адаптер сам собирает канонический результат; negative case на таблицу с `ok = false`, возвращённую не через `fail()`.
  - Evidence: `Scripts/authoring/context.lua:FAIL_SENTINEL`, `Tests/Lua/authoring/simplified_surface.lua:implicit_command_returns_and_fail_normalization`.

- [x] **SAS-05 — Спеки через диспетчеризацию и синхронизация contract**
  - Зависимости: SAS-04.
  - Authoring-скрипт не возвращает дескриптор и живёт в особом окружении, поэтому спека не может потребовать его через `require`.
  - Done: правила authoring-скрипта проверяются отправкой команды и проверкой результата, состояния и событий; прямой доступ к телу authoring-функции не предоставляется — и это записано вместе с причиной: правило, непроверяемое через команду, не является правилом игры; [Lua Runtime Contract](../../../Architecture/LuaRuntimeContract.md) описывает класс authoring-скриптов, его окружение и правило неявного успеха.
  - Evidence: `Tests/Lua/authoring/simplified_surface.lua`, `Docs/Architecture/LuaRuntimeContract.md`.

## Проверка milestone

- [x] Designer-файл не содержит `M`, `return M` и вызова создания контекста.
- [x] Глобальная переменная в authoring-скрипте отвергается.
- [x] Команда без явного результата считается успешной.
- [x] Таблица с `ok = false`, возвращённая не через `fail()`, отказом не является.
- [x] Спека проверяет правило через диспетчеризацию.
