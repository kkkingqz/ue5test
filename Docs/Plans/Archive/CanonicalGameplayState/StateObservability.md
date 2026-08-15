---
title: State Observability Tasks
status: archived
version: 1.0
updated: 2026-08-14
depends_on:
  - InstanceIdentity.md
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../Architecture/HeadlessSimulationContract.md
decisions:
  - ../../../ADR/0020-cpp-scope-criterion.md
  - ../../../ADR/0021-opaque-save-container.md
---

# M3 — State Observability

## Результат этапа

Lua вычисляет канонический хэш своего state и публикует его host-у одним скаляром. Хэш входит в run digest, поэтому golden-прогон начинает ломаться при изменении наблюдаемого состояния. Само дерево boundary не пересекает.

## Задачи

- [x] **CGS-10 — Реализовать канонический хэш state в Lua**
  - Зависимости: CGS-07.
  - Детерминированная сериализация: лексикографическая сортировка ключей объектов, сохранение порядка массивов, стабильное представление int64 и finite double, явное представление `game.null` и отличие absent от null.
  - Done: одинаковое дерево даёт одинаковый хэш независимо от порядка вставки ключей; изменение любого значения меняет хэш; сериализация не зависит от locale и не содержит адресов, таймингов и путей; реализация полностью в Lua, C++ не участвует.
  - Evidence: Создан модуль `Scripts/runtime/state_hasher.lua` (`core:module.runtime.state_hasher`), реализующий каноническую сериализацию типов Lua (лексикографическая сортировка строковых ключей, dense-массивы `[N:...]`, IEEE 754 бинарное представление double, 64-битные целые `i...;`, явное отличие `game.null` от отсутствующего ключа) и стандартный SHA-256 на чистом Lua 5.4; модуль зарегистрирован в `manifest.lua` и экспортирован через `bootstrap/main.lua`; добавлены конформанс-тесты `TestStateHasherProducesDeterministicHashIndependentOfKeyOrder`, `TestStateHasherChangesWhenAnyValueChanges`, `TestStateHasherDistinguishesNullFromAbsent` в `GV2LuaLifecycleConformance.cpp`; CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-11 — Опубликовать хэш host-у**
  - Зависимости: CGS-10.
  - Host получает одну строку через узкий accessor; state дерево boundary не пересекает.
  - Done: accessor возвращает только скаляр; отсутствие state даёт пустое значение, а не ошибку; глубокий Lua → portable reader не появляется (ADR-0021).
  - Evidence: В `Scripts/boundary/outbound.lua` зарегистрирован fixed entrypoint `game.runtime.get_canonical_state_hash`, вызывающий `state_hasher.hash_state(game.state)`; в `FRuntimeSession` реализован метод `GetCanonicalStateHash(FRuntimeFault* OutFault)` возвращающий скалярную строку хэша (или пустую строку, если состояние отсутствует или сессия не активна, без генерации ошибки); дерево состояния boundary не пересекает; добавлены конформанс-тесты `TestSessionStateHashReturnsEmptyWhenSessionNotStarted`, `TestSessionStateHashReturnsCanonicalHashWhenSessionActive`, `TestSessionStateHashReturnsEmptyWhenStateMissing` в `GV2LuaLifecycleConformance.cpp`; CTest (21/21), Parity Validator (22/22) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **CGS-12 — Включить хэш в run digest**
  - Зависимости: CGS-11.
  - `FRunResult` получает поле state hash, digest учитывает его в канонической свёртке.
  - Done: изменение state меняет `digest_hash`; повторный прогон одного manifest даёт идентичный digest; поле присутствует в machine-readable выводе.
  - Evidence: В `FRunResult` и `FRunDigest` добавлено поле `StateHash`; в `ComputeRunDigest` хэш состояния включён в каноническую свёртку `HashPayload`; обновлены функции сериализации `SerializeRunDigest` и десериализации `DeserializeRunDigest`; в `ReplayRunManifest` и headless CLI `main.cpp` хэш состояния извлекается через `Runtime.GetCanonicalStateHash()` перед завершением сессии и выводится в JSON; в `GV2RunDigestConformance.cpp` добавлен тест изменения хэша digest при изменении `StateHash`.

- [x] **CGS-13 — Обновить golden-прогоны**
  - Зависимости: CGS-12.
  - Golden manifest/digest в `Tests/Fixtures/GoldenRuns` пересчитываются один раз вместе с включением хэша.
  - Done: CTest и Unreal automation сверяют новый digest; cross-host parity подтверждает совпадение хэша state между headless и UE.
  - Evidence: Обновлён fixture `Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5` с новым `state_hash` (`2f17eb28ab16acb4f5cfbeaf49cc3ea302a09398f4980d9e9071c1a21e987773`) и `digest_hash` (`8abc023bc35aba21e28d0434adbbc5f1bb1dd34b6c3c9968feb24ae4260e10c3`); обновлён regex проверки в `Headless/CMakeLists.txt`; cross-host parity тест `GV2.Runtime.Session.CrossHostDigestParity` в UE и CTest `gv2_headless_golden_replay_matches_digest` успешно пройдены со 100% совпадением между headless и UE.

- [x] **CGS-14 — Синхронизировать документацию этапа**
  - Зависимости: CGS-13.
  - Done: `HeadlessSimulationContract` описывает состав digest с учётом state hash; `LuaRuntimeContract` описывает правило публикации скаляра; `BuildAndTooling` описывает изменившийся вывод прогона; `ImplementationStatus` обновлён.
  - Evidence: Синхронизированы `HeadlessSimulationContract.md` (включение `state_hash` в `FRunResult`, `FRunDigest` и CLI stdout), `LuaRuntimeContract.md` (правило публикации скалярного хэша состояния через `GetCanonicalStateHash()` и entrypoint `game.runtime.get_canonical_state_hash`), `BuildAndTooling.md` (вывод `state_hash` в `gv2-headless`, conformance test `RunRunDigestConformance`), `ImplementationStatus.md` (отметка о реализации M1–M3); в `CanonicalGameplayState/README.md` Milestone 3 отмечен как выполненный.

## Проверка milestone

- [x] Порядок вставки ключей не влияет на хэш.
- [x] Изменение любого значения state меняет digest.
- [x] Headless и UE дают одинаковый хэш state на одном manifest.
- [x] Canonical state по-прежнему не пересекает boundary: host видит только скаляр.
