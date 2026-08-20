---
title: Add Lua Module
status: informative
version: 2.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/LuaRuntimeContract.md
---

# Добавить programmer Lua module

> **Задача:** добавить infrastructure/composition module в `Scripts/` или package `scripts/` и включить его в общий graph.
> **Предмет:** module source, generated manifest, dependency graph и immutable exports.
> **Нормативно:** [Lua Runtime](../Architecture/LuaRuntimeContract.md), [Build and Tooling](../Architecture/BuildAndTooling.md).

Gameplay rules, Commands, entity methods и presentation не требуют programmer module: для них используйте [Authoring](../Authoring/README.md).

## Шаги

1. Выберите каталог по ответственности: `runtime/`, `presentation/`, `resources/`, `boundary/` или composition в `bootstrap/`. Gameplay не импортирует `boundary`.
2. Верните export table с `id`; все private values оставьте lexical locals:

   ```lua
   local dependency = require("core:module.runtime.portable_value")

   local M = { id = "core:module.runtime.example" }

   function M.register(ctx)
       -- только lifecycle-owned registration
   end

   return M
   ```

3. Используйте только literal `require("<module_id>")`. Generator выведет direct dependencies; dynamic require запрещён.
4. Перегенерируйте package manifest:

   ```bash
   python3 Tools/Content/generate_manifest.py GameData/<package>
   python3 Tools/Content/generate_manifest.py GameData/<package> --check
   ```

   Незамещаемый `module_id` выводится из path. Для `replaceable: true` target ID объявляется в `package.json5` явно.
5. Сделайте module достижимым от `entry_module_id` через literal import.
6. Проверьте graph:

   ```bash
   ./cmake-build-ci/Headless/gv2-headless --check-scripts
   ```

## Replacement

Запечатанные `runtime`, `boundary`, `bootstrap`, `presentation` и `resources` modules заменять нельзя. Разрешённый replacement объявляет тот же ID, а для расширения вызывает `require_base()` только во время module initialization. После загрузки exports immutable (`LuaModuleExportFrozen`).

## Типичные ошибки

- `ManifestMismatch`: generator не запущен после изменения sources/imports.
- `DynamicRequireDisallowed`: ID вычисляется во время выполнения.
- Hidden dependency, cycle или unreachable module: graph отклоняется до первого module.
- `LuaModuleSealed`/`LuaModuleForeignNewId`: package нарушает ownership или replacement policy.
- `require()` в handler: импортируйте один раз при initialization и сохраните local.
