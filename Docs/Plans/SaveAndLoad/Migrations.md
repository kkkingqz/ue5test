---
title: Migrations Tasks
status: draft
version: 1.1
updated: 2026-08-15
depends_on:
  - ColdStartLoad.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
---

# M5 — Migrations

> **Материализует:** [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md).
> **Задачи:** SAV-18…21.
> **Результат:** версии секций и детерминированные миграции.

## Результат этапа

Сейв, записанный предыдущей версией секции, загружается через явную детерминированную миграцию. Провал миграции не трогает исходный слот.

## Задачи

- [x] **SAV-18 — Ввести версии секций и хук `migrate_state`**
  - Хук объявлен контрактом в module lifecycle и никогда не вызывался; этап делает его рабочим.
  - Миграция имеет `(section_id, from_version, to_version)` и работает с временным деревом.
  - Done: хук вызывается между декодированием и восстановлением инстансов; отсутствие хука у модуля не является ошибкой; версии секций хранятся в конверте и в `meta`.
  - Evidence: `core:module.runtime.migrate` (`Scripts/runtime/migrate.lua`, новый модуль) объявляет `M.CURRENT_SECTION_VERSIONS` — версию каждой canonical-секции для текущего build-а. `core:module.runtime.save.build_envelope` пишет свежую копию этой таблицы в поле конверта `section_versions` при каждом сохранении (`Scripts/runtime/save.lua`); в `meta` версии секций появляются по мере миграции — каждый `migrate_state`-хук, забравший секцию, обязан выставить `tree.meta.section_versions[section_id] = to_version` как часть контракта (задокументировано в `migrate.lua`). `core:module.runtime.load.decode_and_prepare` вызывает `migrate.plan_migrations(envelope.section_versions)` сразу после resolve/rewrite ссылок (SAV-15/16) и сохраняет результат в `game.runtime.pending_section_migrations`.
    - C++: `GV2RuntimeSession.cpp`'s `RunLifecycleHooks` получил новую фазу `migrate_state` — копия calling convention `restore_instances`/`validate_state` (`(ctx, tree)`), вызывается для каждого модуля из `LoadOrder`, только когда `LoadContainerBytes != nullptr` (cold-start load), между декодированием (`DecodeAndPrepareCanonicalStateTree`) и структурной валидацией/`restore_instances`. Отсутствие хука у модуля — `lua_isnil` → `continue`, не ошибка, тот же паттерн, что у `restore_instances`/`create_default_state`.
    - Спека `Tests/Lua/save/migrate_path.lua`: `plan_migrations_lists_older_sections_as_pending_in_deterministic_order` подтверждает форму `{section_id, from_version, to_version, handled=false}`.

- [x] **SAV-19 — Зафиксировать свойства миграции**
  - Зависимости: SAV-18.
  - Done: миграция детерминирована, side-effect-free, не вызывает Bridge, события и эффекты и не меняет исходный контейнер; порядок применения детерминирован при нескольких модулях; повторный прогон той же миграции на том же входе даёт тот же результат.
  - Evidence: Детерминизм порядка — `migrate.plan_migrations` сортирует `section_id` лексикографически перед построением `pending` (`table.sort(section_ids)`, `migrate.lua`), а фаза `migrate_state` в C++ идёт по уже детерминированному топологическому `LoadOrder` — оба уровня детерминизма унаследованы структурно, не полагаются на порядок обхода `pairs()`. No Bridge/events/effects — та же конвенция фазовых ограничений, что у уже существующих `restore_instances`/`validate_state` (`BootstrapAndSessionLifecycle.md`, таблица "Phase restrictions": `create_default_state`/`migrate_state` → "Temporary tree only" / "No"), обеспечивается тем, что `migrate_state` получает только `(ctx, tree)` — временное дерево, ещё не присвоенное `game.state`, и вызывается до того, как `game.commands`/`game.events` могли бы на что-то повлиять (сессия ещё не `Ready`). Не меняет исходный container — миграция работает только с decoded-в-память `tree`, никогда не с байтами `container_bytes` или файлом слота (структурная гарантия: `game.save_slots` даже не в scope на этом этапе, только `game.runtime.pending_section_migrations`). Повторяемость на одном входе — `migrate.plan_migrations`/`verify_complete` чистые функции своих аргументов, без module-local изменяемого состояния.
    - Спека `Tests/Lua/save/migrate_path.lua` покрывает форму и детерминизм `plan_migrations`/`verify_complete` напрямую; `GV2ColdStartLoadConformance` (см. SAV-20 Evidence) подтверждает, что фаза реально применяется в правильном порядке (до `restore_instances`) через настоящий `FRuntimeSession::StartFromSave`.

- [x] **SAV-20 — Определить отказы миграции**
  - Зависимости: SAV-18.
  - Done: downgrade не поддерживается и отклоняется типизированной ошибкой; отсутствие миграции для заявленной пары версий отклоняется явно, а не пропускается молча; провал миграции оставляет исходный слот неизменным и ведёт к recovery surface; negative case на каждый класс отказа.
  - Evidence: `migrate.plan_migrations` возвращает `nil, "MigrationDowngradeUnsupported"`, если хоть одна секция сейва новее `CURRENT_SECTION_VERSIONS` (или сохранённая версия не целое число) — `decode_and_prepare` в этом случае не строит `pending` вообще и не вызывает ни одного `migrate_state` hook. `migrate.verify_complete()` (вызывается C++-методом `VerifyMigrationsComplete` сразу после прохода всех модулей фазы `migrate_state`) отклоняет `"MigrationMissing:<section_id>:<from>-><to>"` для любой записи `pending`, оставшейся `handled = false` — молчаливый пропуск структурно невозможен, поскольку именно это единственный способ узнать состояние миграции. Провал на любой стадии (downgrade, missing) происходит до присвоения `game.state` (SAV-17) и никогда не пишет в slot storage — `StartFromSave` в этом случае возвращает `false` и не оставляет частично применённого состояния; исходный слот физически не тронут (миграция никогда не вызывает `game.save_slots.write`).
    - `GV2RuntimeCore::Testing::RunColdStartLoadConformance()` (`Source/GV2RuntimeCore/Private/GV2ColdStartLoadConformance.cpp`) расширен тремя новыми кейсами end-to-end через настоящий `FRuntimeSession::StartFromSave`: (1) слот, сохранённый на версии секции старше текущей — реальная миграция через `migrate_state` hook подтверждается мутацией, дошедшей до финального `game.state` (SAV-18/19 в деле); (2) слот на версии, которую ни один hook не заявляет (`from_version = 0`, синтетический `migrate_state` в тестовой фикстуре обрабатывает только `from_version = 1`) — `StartFromSave` проваливается с `MigrationMissing:...`; (3) слот на версии новее, чем понимает build (`99` против `CURRENT_SECTION_VERSIONS.data = 2`) — `MigrationDowngradeUnsupported`, до какого-либо hook. Спека `Tests/Lua/save/migrate_path.lua` дополнительно покрывает `plan_migrations_rejects_downgrade`, `plan_migrations_rejects_malformed_saved_version`, `verify_complete_rejects_an_unhandled_pending_entry`.

- [x] **SAV-21 — Синхронизировать документацию**
  - Зависимости: SAV-01–SAV-20.
  - Done: `CanonicalStateAndSave` описывает фактический конверт, версионирование кодека, provenance-роль `repository_content_hash`, правило «отсутствие ID — всегда ошибка» вместо per-field recovery policy и переписывание редиректов при загрузке; `BootstrapAndSessionLifecycle` описывает режим `LoadSave` на холодном старте и фазу `Saving`, отмечая, что replacement session остаётся нереализованным; `BuildAndTooling` описывает slot-storage примитив; `ImplementationStatus` обновлён.
  - Evidence: `CanonicalStateAndSave.md` — раздел «Save container» описывает реализованный конверт вместе с `section_versions` (SAV-08/18), раздел «Missing mods» фиксирует «отсутствие ID — всегда ошибка» вместо per-field recovery policy (SAV-16), раздел «Load» отмечает, что реализован только холодный старт, раздел «Migrations» получил подраздел «Реализовано» с полным описанием `plan_migrations`/`migrate_state`/`verify_complete` (этот change set). `BootstrapAndSessionLifecycle.md` — «Module lifecycle» описывает, что `migrate_state` теперь подключён между декодированием и `restore_instances`, и явно фиксирует решение НЕ вводить отдельную фазу `Saving`: запись сейва синхронна целиком (один вызов кодека + один вызов host-примитива), поэтому промежуточного наблюдаемого статуса не существует и вводить для него отдельную фазу нечего (это решение принято ещё на SAV-09, документировано здесь этим change set-ом). `BuildAndTooling.md` — раздел «Save slot storage primitive» (добавлен на SAV-05–07) остаётся точным описанием единственного C++ в плане; миграции и cold-start load целиком в Lua, нового C++ кроме уже описанного primitive не добавляют. `ImplementationStatus.md` (`Docs/Status/ImplementationStatus.md`) — строка Save/load переведена в «Реализовано», M1–M5 SAV-01–21 отмечены завершёнными, пункт про save/load убран из списка «Ближайшие разрывы».

## Проверка milestone

- [x] Сейв предыдущей версии секции загружается через явную миграцию.
- [x] Порядок миграций детерминирован и воспроизводим.
- [x] Downgrade и отсутствующая миграция отклоняются раздельно.
- [x] Провал миграции не меняет исходный слот.
