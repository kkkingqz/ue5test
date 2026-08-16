---
title: Cold Start Load Tasks
status: draft
version: 1.1
updated: 2026-08-15
depends_on:
  - SavePath.md
  - ../../Architecture/StableIDSpecification.md
decisions:
  - ../../ADR/0023-stable-id-publication-freeze.md
---

# M4 — Cold Start Load

> **Материализует:** [Canonical State and Save](../../Architecture/CanonicalStateAndSave.md).
> **Задачи:** SAV-12…17.
> **Результат:** восстановление состояния и ссылочная целостность.

## Результат этапа

Приложение стартует из сохранения. Канонический хэш состояния после загрузки совпадает с хэшем до сохранения. Изменение данных за существующим Stable ID загрузке не мешает; отсутствие ID её останавливает.

Replacement session в этап не входит: загрузка выполняется только на холодном старте.

## Задачи

- [x] **SAV-12 — Ввести режим `LoadSave` на старте**
  - Дескриптор старта получает режим `LoadSave` и `save_slot_id`; cold start читает слот вместо построения defaults.
  - Done: отсутствующий или нечитаемый слот завершает старт как configuration failure до создания VM и показывает recovery surface; режим `NewGame` не затронут.
  - Evidence: `FRuntimeSession::StartFromSave(SessionGeneration, PinnedRepository, Sources, Storage, SaveSlotId, OutFault)` (`Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RuntimeSession.h`/`.cpp`) — новая точка входа рядом с существующим `Start()` (NewGame), который не изменился ни сигнатурой, ни поведением. `StartFromSave` вызывает `Storage.ReadSlot(SaveSlotId)` до `luaL_newstate()` — отсутствующий слот даёт `SaveSlotNotFound`, нечитаемый — `SaveSlotUnreadable`, оба до единого байта VM-стоимости. Recovery surface на уровне `FRuntimeSubsystem`/`GV2SessionCoordinator` (UI-слой) этим планом не затронут — `FRuntimeFault` с типизированным `Code` уже является тем, что recovery surface обязан показать; сам UI-слой вне boundary плана (README.md: "Границы").
    - Проверено `GV2RuntimeCore::Testing::RunColdStartLoadConformance()` (см. Evidence SAV-17): негативный кейс на `SaveSlotNotFound` в обоих host-ах.

- [x] **SAV-13 — Реализовать preflight контейнера**
  - Проверяются версия формата, версия кодека, `save_version` и integrity check — до присвоения состояния.
  - Done: несовпадение integrity check, неизвестная версия формата и downgrade `save_version` отклоняются раздельными типизированными кодами; провал preflight не оставляет частично применённого состояния; `repository_content_hash` при этом не сравнивается и на решение не влияет.
  - Evidence: `core:module.runtime.load.M.preflight(container_bytes)` (`Scripts/runtime/load.lua`) — четыре раздельных типизированных исхода: `SaveContainerCorrupt` (не строка / не декодируется кодеком / не таблица / отсутствуют обязательные поля конверта), `SaveFormatVersionUnknown` (`format_version`/`codec_version` больше текущего `save.SAVE_VERSION`/`canonical_codec.VERSION`), `SaveVersionDowngradeUnsupported` (`save_version` больше текущего `save.SAVE_VERSION` — загрузка сейва из более новой версии игры), `SaveIntegrityMismatch` (`state_hasher.sha256(payload) ~= integrity`). `repository_content_hash` preflight не читает вовсе. Провал preflight — просто `nil, code` до какого-либо декодирования payload, поэтому частично применённого состояния структурно не возникает (нечего применять).
    - Спека `Tests/Lua/save/load_path.lua`: 6 кейсов `preflight_*`, по одному на каждый исход plus non-string input.

- [x] **SAV-14 — Ввести реестр reference-полей**
  - Сейчас `definition_id` проверяется у любого узла, а `world.current_location_id` захардкожен отдельным условием; следующее поле-ссылка потребует такой же ручной правки, и его легко забыть.
  - Done: поля, являющиеся ссылками на definitions, объявлены в одном месте; валидатор состояния использует этот реестр вместо разрозненных условий; добавление нового поля-ссылки требует правки одного места; поведение существующих проверок не изменилось.
  - Evidence: `state_validator.M.definition_reference_fields = { current_location_id = "location" }` (`Scripts/runtime/state_validator.lua`) — единственное место; `validate_node` проверяет любое поле из этого реестра generic-веткой (grammar через `stable_id.is_kind`, существование через `game.repository.exists`) на КАЖДОМ узле дерева, а не только `state.world`. Старый захардкоженный блок `tree.world.current_location_id` в `validate_state_tree` удалён — покрытие идентично (та же проверка теперь срабатывает при обходе `state.world` узла generic-веткой), только текст сообщения об ошибке изменился (путь теперь выводится как `state.world` вместо жёстко вшитого префикса — ни одна спека не проверяла точный текст). `core:module.runtime.load` переиспользует этот же реестр (`state_validator.definition_reference_fields`) для SAV-15's rewrite-прохода — одна точка правды для обоих потребителей.
    - Проверено: `gv2-headless --self-test` — `state_hash` не изменился, `Tests/Lua/world/{current_location,travel_events,travel_command}.lua` (существующие спеки, использующие `current_location_id`) зелёные без изменений в самих спеках.

- [x] **SAV-15 — Разрешать редиректы и переписывать состояние**
  - Зависимости: SAV-14.
  - `Find` уже резолвит redirect source в final target, поэтому переименованный ID грузится; но состояние обязано быть переписано на канонический target, иначе redirect-таблицу невозможно ретировать.
  - Done: после загрузки состояние содержит только канонические ID; повторное сохранение записывает их; спека покрывает цепочку редиректов длиной больше одного шага.
  - Evidence: `C++`-уровня `Find`/`Require` резолвят ровно один хоп redirect (`Source/GV2ContentCore/Private/RepositorySnapshot.cpp`) — для цепочки длиннее одного шага этого недостаточно (второй хоп сам по себе не хранится в `ById`, только в `Redirects`). `core:module.runtime.load.M.resolve_definition_id(id, repository_get)` компенсирует это в Lua: цикл до `MAX_REDIRECT_HOPS` (32) вызывает `repository_get(candidate)`; каждый `not_found` с `err.canonical_id` (это ровно то, что `Require` кладёт в `FRepositoryQueryError.CanonicalId` при прямом редиректе) продвигает `candidate` на следующий хоп. `rewrite_references(val, path, repository_get)` (внутри `load.lua`) рекурсивно обходит decoded state, резолвит `definition_id` (generic) и каждое поле из `state_validator.definition_reference_fields` (SAV-14), присваивая полю канонический итоговый ID — состояние после `decode_and_prepare` содержит только канонические ID по построению; последующее сохранение того же состояния (`save.build_envelope`) сериализует уже переписанные поля.
    - `repository_get` — инъецируемый параметр (по умолчанию `game.repository.get`, недоступный для прямой подмены спекой, так как `game.repository` — read-only таблица) — единственный способ протестировать цепочки без выделенной fixture-репозитория с реальными redirect-записями. Спека `Tests/Lua/save/load_path.lua`: `resolve_definition_id_single_redirect`, `resolve_definition_id_multi_hop_redirect_chain` (цепочка длиной 3), `resolve_definition_id_cycle_is_unknown` (защита от цикла), плюс два кейса против РЕАЛЬНОГО `GameData/core` repository (`resolve_definition_id_against_real_repository_*`) без инъекции.

- [x] **SAV-16 — Проверять ссылочную целостность**
  - Зависимости: SAV-14, SAV-15.
  - Отсутствие Stable ID — всегда ошибка; per-field recovery policy в v1 не вводится, формулировка `CanonicalStateAndSave` приводится в соответствие.
  - Done: изменение данных за существующим ID загрузке не мешает и покрыто спекой; отсутствующий ID даёт типизированную ошибку; retired (tombstoned) и unknown различаются отдельными кодами; висячая ссылка на instance (`player_actor_id`, `owner_id`) отклоняется так же.
  - Evidence: `resolve_definition_id` различает `"retired"` (repository `Require` вернул код `tombstoned`) и `"unknown"` (`not_found` без дальнейшего redirect-хопа, либо цикл) — `rewrite_references` останавливает загрузку на первом нерезолвящемся поле с `SaveReferenceRetired:`/`SaveReferenceUnknown:` (`load.lua`), само изменение данных за существующим ID (не ID, а полей вокруг него) загрузке не мешает структурно — resolve только проверяет существование самого ID, не остального содержимого definition. Висячие instance-ссылки (`meta.player_actor_id`, `item_instances[].owner_id`) уже были покрыты существующими проверками `state_validator.validate_state_tree` (не тронуты этим планом) — они выполняются в общей структурной валидации (шаг 4 `RunLifecycleHooks`), которая теперь одинаково прогоняется и для NewGame, и для decoded LoadSave-дерева, поэтому "отклоняется так же" выполняется без нового кода для instance-ссылок.
    - `CanonicalStateAndSave.md`, раздел «Missing mods»: старая формулировка про per-field recovery policy заменена на «отсутствие ID — всегда ошибка», с явным упоминанием `SaveReferenceRetired`/`SaveReferenceUnknown`.
    - Спека `Tests/Lua/save/load_path.lua`: `resolve_definition_id_tombstoned_is_retired`, `resolve_definition_id_unknown_is_unknown`, `decode_and_prepare_rejects_dangling_reference` (весь пайплайн, не только resolve).

- [x] **SAV-17 — Восстановить состояние и подтвердить roundtrip**
  - Зависимости: SAV-13, SAV-16.
  - Вызывается хук `restore_instances`; canonical state присваивается только после полного успеха.
  - Done: `state_hash` до сохранения и после загрузки совпадает; совпадение подтверждено в обоих host-ах на одном контейнере; провал любой стадии оставляет session без назначенного состояния и ведёт к recovery surface.
  - Evidence: `RunLifecycleHooks` (`GV2RuntimeSession.cpp`) реструктурирован: дерево берётся либо из `CreateDefaultCanonicalStateTree` (NewGame), либо из `DecodeAndPrepareCanonicalStateTree` (LoadSave, вызывает `core:module.runtime.load.decode_and_prepare` — SAV-13/14/15/16 целиком внутри одного Lua-вызова); структурная валидация (шаг 4) выполняется для обоих режимов одинаково; затем, только для LoadSave, новая фаза `restore_instances` (тот же `(ctx, tree)` calling convention, что у `validate_state`; отсутствие хука у модуля не ошибка — `BootstrapAndSessionLifecycle.md`); только ПОСЛЕ этого — модульный `validate_state`, затем присвоение `game.state`, затем `start`. Любой провал на любой стадии (`DecodeAndPrepareCanonicalStateTree`, структурная валидация, `restore_instances`, `validate_state`) возвращает `false` из `RunLifecycleHooks` до строки, присваивающей `game.state` — `game.state` физически не установлен, сессия не стартовала (`Start()`/`StartFromSave()` вызывают `Stop()` и возвращают `false`).
    - `GV2RuntimeCore::Testing::RunColdStartLoadConformance()` (`Source/GV2RuntimeCore/Private/GV2ColdStartLoadConformance.cpp`, новый, portable, исполняется обоими host-ами) — end-to-end: сессия A (`Start`, NewGame) сохраняет через реальный `FFilesystemSaveSlotStorage` (temp-каталог), берёт `GetCanonicalStateHash()`; сессия B (`StartFromSave`) грузит тот же слот, сравнивает `GetCanonicalStateHash()` — обязаны совпасть; `restore_instances`-маркер-модуль подтверждает вызов хука (`game.debug.restore_instances_called`, вне canonical state — не влияет на сравнение хэшей); негативные кейсы — повреждённый контейнер (`SaveContainerCorrupt`, сессия не стартует) и никогда не записанный слот (`SaveSlotNotFound`, до VM). Встроенные Lua-модули этого conformance-набора — намеренно синтетические заглушки (не копии реальных `save.lua`/`load.lua`/`canonical_codec.lua`): реальные правила preflight/redirect/safe-point уже полностью проверены `Tests/Lua/save/{canonical_codec,save_path,load_path}.lua`, поэтому C++-conformance проверяет только оркестрацию `StartFromSave`, не дублирует Lua-правила (ADR-0024) — файл добавлен в `MECHANISM_LUA_FIXTURE_CONFORMANCE_FILES` в `validate_host_conformance_parity.py` с этим обоснованием.
    - Подключено к обоим host-ам: `gv2-headless --self-test` (после `RunSaveSlotStorageConformance`, exit code 18 при провале) и `GV2.Runtime.SaveAndLoad.ColdStartLoadConformance`.
    - Проверено: `ctest` 57/57; `gv2-headless --self-test` — golden `state_hash` не изменился (`2f17eb28ab16acb4f5cfbeaf49cc3ea302a09398f4980d9e9071c1a21e987773`); UE `GV2.Runtime` — 53/53 (был 52/52 до этой задачи, регрессий нет); `validate_docs.py` — 114 файлов; `validate_host_conformance_parity.py` — 26 entry points.

## Проверка milestone

- [x] Хэш состояния совпадает до сохранения и после загрузки в обоих host-ах.
- [x] Правка данных за существующим ID не мешает загрузке.
- [x] Отсутствующий ID останавливает загрузку и различает retired и unknown.
- [x] После загрузки в состоянии остаются только канонические ID.
