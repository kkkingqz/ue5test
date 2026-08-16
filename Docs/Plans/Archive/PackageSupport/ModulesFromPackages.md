---
title: Modules from Packages Tasks
status: archived
version: 1.0
updated: 2026-08-16
depends_on:
  - ModuleSealing.md
  - DiscoveryAndOrder.md
  - ../../../Architecture/LuaRuntimeContract.md
---

# M4 — Modules from Packages

> **Материализует:** [Lua Runtime Contract § Module loader](../../../Architecture/LuaRuntimeContract.md), [Modding § Lua modules](../../../Architecture/Modding.md).
> **Задачи:** PKG-14…19.
> **Результат:** Lua приезжает из пакета, замещает модуль ядра и получает замещённую реализацию.

## Результат этапа

Пакет несёт `scripts/`. `module_id` резолвится по порядку пакетов. Замещающий модуль получает базу через `require_base()`: не вызвал — замена, вызвал — расширение.

## Задачи

- [x] **PKG-14 — Обнаружение `scripts/` внутри пакета**
  - Зависимости: PKG-05.
  - Сейчас Lua внутри пакета не обнаруживается вовсе: UE берёт `FPaths::ProjectDir()/Scripts`, headless — рабочий каталог.
  - Done: `GV2ContentHostSupport` обнаруживает `scripts/` внутри каждого пакета набора; filesystem-часть остаётся вне `GV2ContentCore` ([ADR-0019](../../../ADR/0019-content-host-support-module.md)); порядок обхода не является семантикой; UTF-8 и BOM обрабатываются так же, как сейчас.
  - Evidence: `GV2ContentHostSupport::DiscoverPackageScripts`, `DiscoverPackagesScripts`, unit tests in `PackageDiscoveryTests.cpp`.

- [x] **PKG-15 — Атрибуция source пакетом**
  - Зависимости: PKG-14.
  - Done: имя источника становится `@<package_id>/<relative>` вместо `@Scripts/<relative>`; стек ошибки и диагностика называют пакет-владельца; `core` использует тот же формат, без частного случая; сообщения тестов, пинящие старый формат, обновлены осознанно.
  - Evidence: `@core/`, `@<package_id>/`, `gv2_headless_script_feedback_python`, `Headless/Source/main.cpp`.

- [x] **PKG-16 — Резолюция `module_id` по провайдерам**
  - Зависимости: PKG-12, PKG-15.
  - Done: `module_id` резолвится по порядку пакетов, побеждает последний провайдер; `core:module.*` без раннего провайдера отвергается как `foreign_new_id`; замещение запечатанного модуля отвергается `LuaModuleSealed`; два провайдера одного `module_id` внутри одного пакета остаются fatal duplicate; невалидное замещение блокирует кандидат целиком без отката к замещённой реализации; `require(id)` из любого модуля всегда возвращает победителя.
  - Evidence: `GV2RuntimeSession::LoadModuleGraph`, `FGV2LuaModulePackageOverrideTest`.

- [x] **PKG-17 — `require_base()` и семантика цепочки**
  - Зависимости: PKG-16.
  - Done: загрузчик публикует `require_base()` рядом с `require`; вызов вне инициализации замещающего модуля даёт `LuaModuleBaseNotAvailable`; базовый source исполняется ровно один раз и до замещающего; базовая таблица не регистрируется как модуль; хуки жизненного цикла вызываются только у победителя, делегирование базе — явный вызов; второй замещающий пакет получает базой таблицу первого; цепочка детерминирована порядком пакетов.
  - Evidence: `RequireBase`, `CurrentBaseExportRegistryKey`, `module_override.lua`.

- [x] **PKG-18 — Зависимости по объединению цепочки**
  - Зависимости: PKG-16.
  - Done: эффективный набор зависимостей замещённого модуля — объединение объявленных по всей цепочке; missing/duplicate/cycle/reachability проверяются на объединении до инициализации первого модуля; замещающий модуль объявляет только свои прямые зависимости; цикл, возникающий только на объединении, отвергается с указанием обоих пакетов.
  - Evidence: `FModuleChain::EffectiveDependencies`, topological sort in `GV2RuntimeSession.cpp`.

- [x] **PKG-19 — Фикстура замещения и синхронизация документации**
  - Зависимости: PKG-17, PKG-18.
  - Done: фикстура мод-пакета содержит `scripts/` и покрывает полную замену модуля, расширение через `require_base()` с делегированием, цепочку из двух замещающих пакетов и каждый типизированный отказ; правила, выразимые в Lua, проверяются спеками ([ADR-0024](../../../ADR/0024-lua-spec-runner.md)), C++ проверяет только резолюцию провайдеров; набор исполняется обоими хостами; `LuaRuntimeContract`, `Modding` и guide [AddLuaModule](../../../Guides/AddLuaModule.md) описывают замещение и идиому `setmetatable(M, { __index = base })`.
  - Evidence: `test_mod/scripts/`, `module_override.lua`, `FGV2LuaModulePackageOverrideTest`, updated docs.

## Проверка milestone

- [x] Пакет полностью заменяет `core:module.gameplay.root`, не вызывая базу.
- [x] Пакет расширяет тот же модуль, делегируя базе необработанные команды.
- [x] Два пакета на один `module_id` выстраиваются в детерминированную цепочку.
- [x] Замещение запечатанного модуля и `core:module.*` без раннего провайдера отвергаются раздельно.
- [x] Зависимости базы загружаются, даже если замещающий пакет их не объявил.
- [x] Стек ошибки в модуле мода называет пакет-владельца.
