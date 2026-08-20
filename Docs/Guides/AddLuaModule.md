---
title: Add Lua Module
status: informative
version: 1.2
updated: 2026-08-20
depends_on:
  - README.md
---

# Добавить Lua-модуль

> **Задача:** завести новый модуль в `Scripts/` так, чтобы его загрузили оба хоста.
> **Предмет:** Lua module manifest, exports и lifecycle hooks.
> **Нужно:** собранный `gv2-headless`.
> **Нормативно:** [Lua Runtime Contract](../Architecture/LuaRuntimeContract.md), [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md).

## Шаги

**1. Выбрать каталог по назначению.** `runtime/` — переносимое ядро, `gameplay/` — правила и сервисы, `presentation/` — построение желаемого экрана, `resources/` — работа с `resource_id`, `boundary/` — точки входа хоста. Импортировать `boundary` из gameplay и presentation запрещено.

**2. Написать модуль.** Он обязан вернуть таблицу экспорта и объявить свой `id`:

```lua
local M = { id = "core:module.gameplay.trade" }

function M.register(ctx)
    -- регистрация сервисов, валидаторов, подписок
end

return M
```

Хуки необязательны: отсутствие `register` не является ошибкой. Какие хуки вызываются сейчас, а какие объявлены и не вызываются — в [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md).

**3. Объявить модуль в `Scripts/bootstrap/manifest.lua`** — путь, полный список прямых зависимостей и признак замещаемости `replaceable: boolean` (по умолчанию `false`; для модулей ядра `gameplay/` и `debug/` — `true`, для `runtime/`, `boundary/`, `bootstrap/`, `presentation/`, `resources/` — `false`). Скрытые импорты запрещены: `require` во время инициализации разрешён только для объявленных зависимостей.

**4. Сделать модуль достижимым.** Граф обходится от `entry_module_id`; недостижимый модуль отвергается. Обычно достаточно, чтобы его требовал кто-то уже достижимый, либо агрегировать его в `bootstrap/main.lua`.

**5. Проверить.**

```bash
./cmake-build-ci/Headless/gv2-headless --check-scripts
```

Проверка занимает доли секунды и не запускает геймплей.

## Замещение существующего модуля (моддинг)

Для замещения или расширения существующего модуля (например `core:module.gameplay.root`):

1. Убедиться, что целевой модуль замещаем (`replaceable: true` в манифесте ядра). Запечатанные модули ядра (`runtime/`, `boundary/`, `bootstrap/`, `presentation/`, `resources/`) замещать запрещено (`LuaModuleSealed`).
2. В манифесте мод-пакета (`scripts/manifest.lua`) объявить модуль с тем же `module_id`.
3. Для полного замещения написать модуль без вызова базы.
4. Для расширения/делегирования вызвать `local base = require_base()` при инициализации модуля и привязать прототип через метатаблицу:

```lua
local base = require_base()

local M = setmetatable({
    id = "core:module.gameplay.root",
}, { __index = base })

function M.handle_command(request)
    if request.command_id == "my_mod:command.special" then
        return handle_special(request)
    end
    return base.handle_command(request)
end

return M
```

## Типичные ошибки

**Файл создан, но не объявлен в манифесте.** Загрузчик отвергает незаявленные исходники — это не забывчивость проверки, а защита от случайно попавшего в дерево файла.

**Скрытая зависимость.** `require` модуля, которого нет в списке зависимостей, падает при инициализации.

**Цикл в графе.** Отвергается до инициализации первого модуля (в том числе циклы, возникающие при объединении цепочек замещения).

**Глобальные переменные.** Модуль не создаёт глобалов; приватные значения хранятся в локальных переменных, публичные возвращаются таблицей экспорта.

**Мутация чужой таблицы экспорта.** Таблицы экспорта модулей неизменяемы после загрузки (`LuaModuleExportFrozen`); добавление полей и подмена функций в чужом модуле запрещены.

**Замещение запечатанного модуля.** Попытка заместить модуль с `replaceable: false` отклоняется ошибкой `LuaModuleSealed`.

**`require_base()` вне замещающего модуля.** Вызов `require_base()` вне инициализации замещающего модуля отклоняется ошибкой `LuaModuleBaseNotAvailable`.

**Новый модуль в чужом namespace.** Мод может создавать новые модули только в своём namespace (ошибка `LuaModuleForeignNewId`).

**`require` в обработчике.** Импорты сохраняются в локальные переменные при инициализации, а не запрашиваются на каждый вызов.
