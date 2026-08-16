---
title: Save Path Tasks
status: draft
version: 1.1
updated: 2026-08-15
depends_on:
  - CanonicalCodec.md
  - SlotStorage.md
  - ../../Architecture/CommandsAndEvents.md
---

# M3 — Save Path

> **Материализует:** [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md).
> **Задачи:** SAV-08…11.
> **Результат:** конверт контейнера, safe point и атомарная запись.

## Результат этапа

Состояние сохраняется целиком в непрозрачный контейнер в разрешённый момент. Отказ записи не разрушает предыдущий слот.

## Задачи

- [x] **SAV-08 — Определить конверт контейнера**
  - Конверт содержит версию формата, версию кодека, `save_version`, `save_id`, `repository_content_hash`, integrity check и полезную нагрузку.
  - `repository_content_hash` записывается как provenance и не является условием загрузки.
  - Done: конверт сериализуется тем же кодеком; integrity check вычисляется по полезной нагрузке и совпадает с каноническим хэшем состояния; назначение `repository_content_hash` как provenance записано в `CanonicalStateAndSave`.
  - Evidence: `core:module.runtime.save` (`Scripts/runtime/save.lua`) добавлен в manifest (зависимости: `canonical_codec`, `state_hasher`) и агрегирован в `Scripts/bootstrap/main.lua` для reachability из `entry_module_id`. `M.build_envelope(state, save_id, repository_content_hash)` строит таблицу `{format_version, codec_version, save_version, save_id, repository_content_hash, integrity, payload}`: `payload = canonical_codec.serialize(state)`, `integrity = state_hasher.sha256(payload)` — численно то же самое, что `state_hasher.hash_state(state)` (доказано спекой `envelope_integrity_matches_canonical_state_hash`). `M.serialize_envelope(envelope) = canonical_codec.serialize(envelope)` — тот же кодек, не отдельный формат (envelope — обычная Lua-таблица, `payload` внутри неё — обычная строка произвольных байт, кодек уже умеет их кодировать length-prefixed). `save_id`/`repository_content_hash` — параметры вызова, не внутреннее состояние модуля (никакого module-local счётчика), поэтому конверт — чистая функция своих аргументов. `Docs/Architecture/CanonicalStateAndSave.md` получил раздел «Реализованный конверт» под «Save container», фиксирующий это и то, что оставшиеся поля контракта (PRNG, gameplay time, mods) относятся к последующим milestones.

- [x] **SAV-09 — Ввести фазу `Saving` и safe point**
  - Сохранение разрешено только когда runtime phase `Idle`, очереди команд и событий пусты, session `Ready` и нет активного перехода lifecycle.
  - Done: попытка сохранить во время `ExecutingCommand` или `PumpingEvents` отклоняется typed-ошибкой и не пишет в слот; во время `Saving` новая команда отклоняется либо ставится в очередь согласно принятому решению; фаза выставляется фактически и снимается после завершения.
  - Evidence: `M.is_safe_point()` проверяет `game.runtime.phase == "idle"` (что структурно исключает `executing_command`, `pumping_events` и `failed` — единственные другие значения фазы, `Scripts/runtime/command_dispatcher.lua`/`event_bus.lua`), плюс `game.commands.get_queue_length() == 0` и новый `game.events.get_queue_length()` (`Scripts/runtime/event_bus.lua`, зеркало уже существующего `command_dispatcher.get_queue_length`). `M.save()` вызывает эту проверку первым шагом и возвращает `false, "SaveNotAtSafePoint"` без единого обращения к `game.save_slots.write`, если она не проходит — реентерабельный save изнутри обработчика команды/события видит не-`idle` фазу и отклоняется тем же путём. Явную фазу `Saving` для M3 вводить не потребовалось: `M.save()` целиком синхронна (один вызов кодека + один вызов host-примитива, без промежуточной точки, где мог бы наблюдаться промежуточный статус) — `Docs/Architecture/CanonicalStateAndSave.md`, раздел «Реализованная проверка» под «Safe point», фиксирует это явно вместе с проверкой.
    - Спеки `Tests/Lua/save/save_path.lua`: `is_safe_point_true_at_baseline`, `save_rejected_during_executing_command_phase`, `save_rejected_during_pumping_events_phase`, `save_rejected_when_command_queue_not_empty` — каждый негативный кейс временно выставляет `game.runtime.phase`/кладёт запись в очередь напрямую (спека самодостаточна, `game` — обычная изменяемая таблица) и восстанавливает состояние перед возвратом.

- [x] **SAV-10 — Реализовать запись сейва**
  - Зависимости: SAV-08, SAV-09.
  - Done: состояние сериализуется целиком и передаётся примитиву одним вызовом; Lua не выполняет filesystem I/O; повторное сохранение того же состояния даёт побайтово одинаковый контейнер; сохранение не изменяет canonical state.
  - Evidence: `M.save(slot_id, save_id, repository_content_hash)` сериализует `game.state` целиком одним вызовом `canonical_codec.serialize` внутри `build_envelope`, затем передаёт результат `game.save_slots.write(slot_id, container)` — новый read-only C++ биндинг (`game.save_slots`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`, `SaveSlotsWrite`), единственный пропускающий байты в `GV2RuntimeCore::ISaveSlotStorage::WriteSlot` (SAV-05/06) без интерпретации. Lua нигде не открывает файл и не получает путь. `FRuntimeSession::SetSaveSlotStorage(ISaveSlotStorage*)` — новый метод, которым composition root подключает storage к сессии; необязателен (session стартует и без него, SAV-05). Детерминизм доказан спекой `repeated_build_of_same_state_gives_byte_identical_container`; отсутствие мутации — спекой `save_does_not_mutate_canonical_state` (сравнивает `canonical_codec.serialize(game.state)` до и после вызова).
    - Оба host-а подключают `GV2RuntimeCore::FFilesystemSaveSlotStorage`, рутованный во временный каталог, к сессии, на которой исполняется `Tests/Lua/save/`: `Headless/Source/main.cpp` (self-test spec loop) и `GV2LuaSpecRunnerHostTests.cpp` — специфика подключена только для этого под-дерева практически (безвредно для остальных, поскольку только `save/`-спеки вызывают `game.save_slots.write`), сам временный каталог создаётся и удаляется вокруг каждого прогона. Значит `Tests/Lua/save/save_path.lua` бьёт по настоящему host-примитиву, а не по mock.

- [x] **SAV-11 — Зафиксировать failure semantics записи**
  - Зависимости: SAV-10.
  - Done: отказ примитива возвращается как typed result и не переводит session в `Failed`; предыдущий слот остаётся валидным и читаемым; неуспешное сохранение не меняет `save_id` и не расходует его; negative case на каждый класс отказа.
  - Evidence: `M.save()` возвращает `false` плюс один из типизированных кодов (`"SaveNotAtSafePoint"`, `"SaveSlotStorageUnavailable"`, `"SaveWriteFailed:<primitive_code>"`) и никогда не трогает `game.runtime.phase` — отказ структурно не может перевести сессию в `failed` (единственные writer-ы `game.runtime.phase = "failed"` — `command_dispatcher.lua`/`event_bus.lua`, вне пути `save.lua`). Предыдущий слот валиден в силу гарантии самого примитива (SAV-06/07: atomic rename, temp-файл никогда не публикуется при ошибке) — `M.save()` ничего не делает, что могло бы это нарушить. `save_id` — параметр вызова, не внутреннее состояние модуля: `save.lua` никогда не читает и не пишет никакой module-local счётчик, поэтому неуспешный вызов тривиально «не расходует» `save_id` — доказано спекой `failed_save_does_not_consume_save_id` (тот же `save_id` успешно повторно используется сразу после отклонённой попытки). Negative case на класс отказа `SaveNotAtSafePoint` — три кейса SAV-09 выше; на класс `SaveWriteFailed` — `save_rejects_invalid_slot_id_with_typed_write_failure` (slot id вне грамматики `IsValidSaveSlotId` доходит до реального примитива и получает `Failure`, единственный класс отказа примитива, реально достижимый из Lua без filesystem-доступа — `not_found`/`unreadable` покрыты на уровне C++ conformance, SAV-07). `SaveSlotStorageUnavailable` — защитная ветка (`game.save_slots` отсутствует), не покрыта Lua-спекой: production/test-сессии, на которых исполняются `Tests/Lua/save/`, всегда получают storage начиная с SAV-10 wiring, отдельная фикстура без storage ради одной ветки признана избыточной.
    - Проверено: `ctest` 57/57; `gv2-headless --self-test` — `state_hash` не изменился (`2f17eb28ab16acb4f5cfbeaf49cc3ea302a09398f4980d9e9071c1a21e987773`), все 11 новых кейсов `save_path.lua` проходят; UE `GV2.Runtime` — 52/52, регрессий нет; `validate_docs.py` — 114 файлов; `validate_host_conformance_parity.py` — 25 entry points (не менялось — `save_path.lua` это Lua-спека, не C++ conformance, ADR-0024 не требует её регистрации в allowlist).

## Проверка milestone

- [x] Сохранение вне safe point отклоняется и не пишет в слот.
- [x] Повторное сохранение одного состояния воспроизводимо побайтово.
- [x] Отказ записи оставляет предыдущий слот валидным.
- [x] Сохранение не меняет состояние.
