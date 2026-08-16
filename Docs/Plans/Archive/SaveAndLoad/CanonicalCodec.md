---
title: Canonical Codec Tasks
status: archived
version: 1.1
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/CanonicalStateAndSave.md
decisions:
  - ../../../ADR/0021-opaque-save-container.md
---

# M1 — Canonical Codec

> **Материализует:** [Canonical State and Save](../../../Architecture/CanonicalStateAndSave.md).
> **Задачи:** SAV-01…04.
> **Результат:** обратимая каноническая кодировка как отдельный модуль.

## Результат этапа

Каноническая кодировка становится самостоятельным модулем с прямой и обратной операцией. Хэширование и сериализация сейва используют одну реализацию.

## Задачи

- [x] **SAV-01 — Вынести кодек в отдельный модуль**
  - `M.serialize` переносится из `core:module.runtime.state_hasher` в `core:module.runtime.canonical_codec`; хэшер становится потребителем кодека и сохраняет свой публичный интерфейс.
  - Done: поведение и результат хэширования не изменились, что подтверждается неизменностью pinned `state_hash` в golden-прогонах; вторая реализация кодировки отсутствует; имя модуля соответствует назначению.
  - Evidence: Новый `Scripts/runtime/canonical_codec.lua` (`core:module.runtime.canonical_codec`) содержит дословно перенесённый `M.serialize` (текст ошибки на неподдерживаемый тип не изменён) плюс `M.VERSION = 1` — задел под SAV-04. `Scripts/runtime/state_hasher.lua` теперь требует `canonical_codec`, `M.serialize = canonical_codec.serialize` (публичный интерфейс сохранён как delegate) и `M.hash_state` вызывает `canonical_codec.serialize` вместо собственной копии. `Scripts/bootstrap/manifest.lua`: `canonical_codec` зарегистрирован без зависимостей, `state_hasher` получил зависимость на него. Второй реализации кодировки в дереве не осталось.
    - Проверено: `gv2-headless --self-test` — `state_hash` не изменился (`2f17eb28ab16acb4f5cfbeaf49cc3ea302a09398f4980d9e9071c1a21e987773`, тот же скаляр, что во всех предыдущих прогонах этой сессии); `ctest` 57/57 (включая `gv2_headless_golden_replay_matches_digest`); `gv2-content` пересобран без регрессий.
    - UE `GV2.Runtime`: по пути пришлось пересобрать редактор (несвязанная поломка сборки — `/*` внутри doc-комментария в параллельно добавленном `LocalizationDiscovery.h`, однострочный фикс) и обнаружился один пред-существующий провал `GV2.Runtime.Lua.SpecRunnerHost` (кейс `lifecycle_phases.subscriber_registry_frozen_after_init`) — относится к незавершённой параллельной работе над `GameplayEventsAndWorld`/EventBus, не к кодеку; не тронуто. Всё, что действительно относится к state hashing/canonical codec (`MarshallerConformance`, `LifecycleConformanceCrossHost`, `CommandValidatorSpecRunnerHost`, остальные кейсы `world/`), зелёное.

- [x] **SAV-02 — Реализовать обратную операцию**
  - `M.deserialize` восстанавливает дерево из канонической строки.
  - Done: roundtrip сохраняет типы и порядок для всех поддерживаемых значений — null, bool, int64, finite double, string с произвольными байтами, dense array, string-key object; `game.null` восстанавливается как `game.null`, а не как отсутствие ключа; порядок ключей объекта после roundtrip даёт ту же каноническую строку.
  - Evidence: `Scripts/runtime/canonical_codec.lua` получил `M.deserialize` — рекурсивный descent-парсер с явным курсором позиции (`decode_value`/`decode_array`/`decode_object`, приватные upvalue-функции модуля). Каждый тег грамматики serialize имеет зеркальную декодирующую ветку: `n`→`game.null` (никогда не Lua `nil` — то самое различение из Done), `b0`/`b1`→bool, `i<dec>;`→integer (`math.tointeger`), `d<16 hex>`→float (`hex_to_bytes` + `string.unpack(">d", ...)`, точное обращение `string.pack(">d", val)`/`string.unpack(">I8", ...)` из serialize), `s<len>:<bytes>`→string (bytes читаются как есть, без экранирования — формат уже length-prefixed), `[<count>:...]`→dense array, `{<count>:...}`→string-key object. Порядок ключей после roundtrip гарантирован структурно: `M.serialize` всегда сортирует ключи при кодировании, поэтому повторная кодировка декодированного дерева детерминированно даёт тот же порядок — отдельного кода не потребовалось.
  - Проверено спекой `Tests/Lua/save/canonical_codec.lua` (см. Evidence SAV-03 — та же спека покрывает обе задачи).

- [x] **SAV-03 — Определить отказ на повреждённом входе**
  - Done: усечённая строка, неизвестный тег, несовпадение объявленной длины строки или счётчика контейнера и лишние байты в хвосте дают типизированную ошибку с позицией; частично восстановленное дерево не возвращается; negative case на каждый класс повреждения.
  - Evidence: Каждая decode-функция вызывает общий `fail(pos, detail)` → `error("CanonicalCodecCorrupt: " .. detail .. " at byte offset " .. pos, 0)`; поскольку это Lua `error()`, а не return-based результат, весь стек рекурсивного descent-а разворачивается немедленно — частично построенное дерево структурно не может достичь вызывающего кода. Границы проверяются перед чтением (объявленная длина строки/счётчик массива/счётчик ключей объекта сверяются с оставшимся размером входа до попытки прочитать), поэтому усечение ловится как typed error, а не паникой доступа за пределы строки. Неизвестный тег, лишние байты в хвосте (`pos ~= #str + 1` на верхнем уровне) — отдельные проверки.
    - `Tests/Lua/save/canonical_codec.lua` (13 кейсов, оба SAV-02 и SAV-03): roundtrip для null/bool/int64 границ/float/строк с произвольными байтами (включая NUL и все служебные разделители кодека)/dense array/object с пересортировкой ключей/вложенного state-подобного дерева; negative cases на усечённую строку, неизвестный тег, несовпадение счётчика массива, несовпадение счётчика ключей объекта, лишние байты в хвосте, отсутствие частичного дерева при ошибке, наличие byte offset в сообщении об ошибке.
    - Подключено к обоим host-ам как под-дерево `Tests/Lua/save/` на той же production-сессии, что и `world/`/`events/`/`resources/`/`lifecycle/` (не нужна изолированная фикстура — кодек не трогает `game.state`/`game.commands`).
    - Проверено вживую: временный сбой в кейсе `null_roundtrips_as_game_null` корректно провалил `gv2-headless --self-test` (exit 16, `id=canonical_codec.null_roundtrips_as_game_null`) — откачен.
    - `ctest` 57/57, UE `GV2.Runtime` 50/51 (единственный минус — пред-существующий `lifecycle_phases.subscriber_registry_frozen_after_init`, относится к незавершённой параллельной работе над `GameplayEventsAndWorld`/EventBus, не к кодеку — не тронуто), `validate_host_conformance_parity.py` — 24 entry points, doc-валидатор — 114 файлов.

- [x] **SAV-04 — Зафиксировать версионирование кодека**
  - Зависимости: SAV-01–SAV-03.
  - Кодек становится частью совместимости сейвов: изменение кодировки ломает не только golden-прогоны, но и сохранения.
  - Done: правило записано в `CanonicalStateAndSave`; кодек объявляет версию, которая входит в конверт контейнера; изменение кодировки без поднятия `save_version` невозможно провести незаметно.
  - Evidence: `Docs/Architecture/CanonicalStateAndSave.md` получил раздел «Кодек и `save_version`» в «Save container»: `canonical_codec.M.VERSION` — версия самой кодировки, независимая от `save_version` контейнера (секции/конверт); правило — изменение кодировки обязано поднять оба значения в одном change set; `save_version` может расти отдельно (миграция секции), `M.VERSION` — никогда не растёт без `save_version`.
    - Механизм «незаметно не получится» — не только формулировка в контракте, но и работающий trip wire: `Tests/Lua/save/canonical_codec.lua` получил кейс `codec_version_and_encoding_are_pinned`, утверждающий `codec.VERSION == 1` и точную pinned-каноническую строку для фиксированного представительного значения (`{6:1:ai1;1:bs2:hi1:c[2:i1;i2;]1:db11:en1:fd4004000000000000}` для `{a=1,b="hi",c={1,2},d=true,e=game.null,f=2.5}`).
    - Проверено вживую: временно поднят `M.VERSION` до 2 без изменения кодировки — `gv2-headless --self-test` немедленно провалился на этом же кейсе (`canonical_codec.VERSION changed — this must be a deliberate bump...`, exit 16); откачено. Это доказывает, что даже версия-без-encoding-изменения (не говоря о реальном изменении кодировки, которое дополнительно провалит pinned-строку) не проходит незамеченной.
    - `ctest` 57/57, UE `GV2.Runtime.Lua.SpecRunnerHost` — `Tests/Lua/save` подтверждён зелёным отдельным прогоном с переставленным (временно, только для проверки) порядком под-деревьев, поскольку пред-существующий сбой в `Tests/Lua/lifecycle` (чужая незавершённая работа) останавливает общий цикл раньше, чем тот доходит до `save` в штатном порядке — сам факт остановки цикла на первом провалившемся под-дереве является пред-существующим свойством того же цикла (введён параллельно с EventBus-работой, не мной), не багом SAV-01..04.
    - `validate_host_conformance_parity.py` — 24 entry points, doc-валидатор — 114 файлов.

## Проверка milestone

- [x] Кодировка имеет одну реализацию, используемую и хэшем, и сейвом.
- [x] Roundtrip не теряет типы и различает `game.null` и отсутствие ключа.
- [x] Повреждённый вход даёт типизированную ошибку с позицией.
- [x] Связь версии кодека с `save_version` зафиксирована в контракте.
