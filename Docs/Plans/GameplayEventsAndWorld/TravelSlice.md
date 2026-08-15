---
title: Travel Slice Tasks
status: draft
version: 2.0
updated: 2026-08-15
depends_on:
  - SubscriptionAndReaction.md
  - WorldDomainObject.md
---

# M5 — Travel Slice

## Результат этапа

Сценарий проходит целиком в обоих host-ах: игрок отправляет команду перемещения, валидатор проверяет допустимость, handler меняет текущую локацию через сервис, мир публикует факты выхода и входа, подписчик на них реагирует.

## Задачи

- [x] **GEW-13 — Реализовать команду перемещения**
  - `core:command.location.travel` с payload, содержащим целевую локацию.
  - Путь: validator → handler → Gameplay Service → World.
  - Done: недопустимый переход отклоняется валидатором с typed error и не меняет state; успешный переход меняет текущую локацию ровно один раз; handler остаётся тонким и не содержит правил.
  - Evidence: `Scripts/gameplay/location_service.lua` (`core:service.location`, `core:validator.location.travel`), `Scripts/gameplay/root.lua` (`core:command.location.travel`), `Scripts/bootstrap/manifest.lua` и `main.lua`, `Tests/Lua/world/travel_command.lua` (4 спека-кейса: успешный переход, отказ на ту же локацию, отказ на неизвестную локацию, отказ на невалидный формат ID), `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-14 — Опубликовать факты перехода**
  - `core:event.location.leave` и `core:event.location.enter` с полями исходной и целевой локации.
  - Один реальный подписчик реагирует на конкретный переход, проверяя условие в обработчике.
  - Done: порядок `leave` перед `enter` зафиксирован спекой; отказавшая команда не публикует ни одного факта; подписчик, проверяющий другое условие, не срабатывает.
  - Evidence: `Scripts/gameplay/location_service.lua` (эмитит `core:event.location.leave` до смены состояния и `core:event.location.enter` после смены состояния с полями `from_location_id` и `to_location_id`), `Tests/Lua/world/travel_events.lua` (4 спека-кейса: строгий порядок leave перед enter, 0 фактов при отказе валидатора, реакция подписчика только на своё условие, постановка отложенной команды подписчиком), `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **GEW-15 — Подтвердить сценарий в обоих host-ах**
  - Зависимости: GEW-13, GEW-14.
  - Done: сценарий воспроизводится headless replay и Unreal automation; digest меняется за счёт изменения state, а не за счёт событий.
  - Golden-прогон строится на замороженном корпусе (TAS-08), поэтому расширение golden manifest командой перемещения обновляет digest осознанно и в этом же change set; добавление контента в `GameData/core` golden не трогает.
  - Evidence: Расширен `Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.manifest.json5` командой `core:command.location.travel` (11 команд), обновлён `golden_headless_10_seed_42.digest.json5` с новым `state_hash` (`841425c22f67356a8990265a5b77e06ce7bd357e01713ee26ee40a642acaaa36`) и `digest_hash` (`c4ff441dfb0a481fa0672267258e638059b031a96485c5339a4c946ee549504d`); обновлён `Source/GV2/Private/Tests/GV2RuntimeCoreCrossHostDigestTests.cpp`; CTest `gv2_headless_golden_replay_matches_digest` и `gv2-headless --self-test` зелёные (57/57 passed).

- [x] **GEW-16 — Синхронизировать документацию**
  - Зависимости: GEW-13–GEW-15.
  - Done: `CommandsAndEvents` отмечает реализованную часть lifecycle и фактическую модель подписки; `LuaRuntimeContract` описывает `game.instances.world` и права обработчика события; `CanonicalStateAndSave` описывает форму секции `world`; `GlossaryAndNaming` содержит термины World, event subscription и mutation window; `ImplementationStatus` обновлён.
  - Evidence: Обновлены `Docs/Architecture/CommandsAndEvents.md`, `Docs/Architecture/LuaRuntimeContract.md`, `Docs/Architecture/CanonicalStateAndSave.md`, `Docs/Architecture/GlossaryAndNaming.md`, `Docs/ImplementationStatus.md`, `validate_docs.py` (98 файлов) и `validate_host_conformance_parity.py` (24 entry points) прошли успешно.

## Проверка milestone

- [x] Сценарий перемещения проходит целиком в UE и headless.
- [x] Отказ валидатора не меняет state и не публикует фактов.
- [x] Порядок `leave`/`enter` воспроизводим.
- [x] Подписчик реагирует только на своё условие.
