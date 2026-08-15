---
title: Content Independent Assertions Tasks
status: archived
version: 1.1
updated: 2026-08-15
depends_on:
  - FrozenTestCorpus.md
---

# M3 — Content Independent Assertions

## Результат этапа

Тесты утверждают свойства корпуса, а не его размер, и читают каждое pinned-значение из единственного источника.

## Задачи

- [x] **TAS-09 — Убрать дублирование pinned-значений**
  - Хэши, продублированные в `Headless/CMakeLists.txt`, `Headless/Source/main.cpp` и `GV2ContentCoreRepositoryResolutionTests.cpp`, читаются из файла-источника.
  - Done: изменение pinned-значения требует правки одного файла; ни один хэш корпуса не встречается в исходниках строковым литералом; синтетические хэши внутри conformance-наборов, не связанные с реальным корпусом, отличимы от корпусных по имени или комментарию.
  - Evidence:
    - **Golden digest hash** (`Headless/CMakeLists.txt`): вместо строкового литерала — `file(READ .../golden_headless_10_seed_42.digest.json5 ...)` + `string(JSON GV2_GOLDEN_DIGEST_HASH GET ... digest_hash)`, значение подставляется в `PASS_REGULAR_EXPRESSION` через переменную. Единственный источник — сам golden-фикстур-файл.
    - **Представительный merged core+test_mod content hash** (был продублирован в `Headless/Source/main.cpp` и `GV2ContentCoreRepositoryResolutionTests.cpp`): вынесен в новый файл `Tests/Fixtures/expected_merged_content_hash.txt` (sibling `expected_core_content_hash.txt`, не внутри `PortableContentCore/` — тот же PCC-01 запрет, что и в TAS-07). `Headless/Source/main.cpp` читает его в рантайме через `FixtureRoot.parent_path() / "expected_merged_content_hash.txt"`; `GV2ContentCoreRepositoryResolutionTests.cpp` — через `FFileHelper::LoadFileToString` (тот же паттерн, что `GV2ContentCoreCrossHostParityTests.cpp` уже использовал для `expected_core_content_hash.txt`).
    - **Синтетические хэши в conformance-наборах**: `GV2RunManifestConformance.cpp` и `GV2RunDigestConformance.cpp` содержат произвольные opaque hex-строки (`RepositoryContentHash`/`StateHash`), не связанные ни с одним реальным репозиторием — они тестируют механику сериализации/вычисления digest, а не контент. Добавлен явный комментарий у первого использования в каждом файле, отличающий их от корпусных pinned-значений. `GV2RunReplayConformance.cpp`'s `"0000...0000"` уже самоочевиден (canonical all-zero mismatch sentinel) — оставлен без изменений.
    - `ctest` 57/57, UE `GV2.Runtime` 54/54, `validate_host_conformance_parity.py` — 28 entry points, все без изменений в поведении (переход на чтение из файла даёт те же значения).

- [x] **TAS-10 — Заменить переписи на свойства**
  - `Definitions->AsArray().size() == 17`, `KindCounts.size() == 6`, `Screens.size() == 3` и подобные утверждения заменяются на проверки конкретных ID, разрешения kind, отсутствия дубликатов и порядка.
  - Done: добавление сущности в замороженный корпус не ломает ни один из этих тестов; там, где количество действительно является предметом проверки (например, «ровно один winner после override»), утверждение остаётся и снабжается комментарием, объясняющим почему.
  - Evidence: Все переписи из явного списка задачи заменены на проверки свойств (membership конкретных ID + uniqueness + sorted-order), с комментарием `TAS-10` у каждой замены:
    - `Headless/Source/main.cpp::RunSharedJson5FixtureConformance()`: `CoreBuild`/`ModBuild` size-checks → membership конкретных id + uniqueness (no duplicate winner ids после override); `Screens.size()==3` → `std::is_sorted` + membership.
    - `GV2ContentCoreMinimalCoreSchemasTests.cpp`: total/per-kind size-checks → uniqueness + `KindCounts[kind] >= 1` per kind. `KindCounts.size() == 6` **оставлен как есть** — это единственный случай, где количество является предметом проверки (полнота покрытия ровно шести схем этим фикстур-корпусом), снабжён комментарием, почему седьмой kind обязан либо получить осознанное решение о покрытии, либо считаться утечкой.
    - `GV2ContentCoreRepositoryResolutionTests.cpp`: `size()==17` → uniqueness + membership (test_mod-добавленный экран, нетронутый core-выживший); `Screens.size()==3` с индексами `[0..2]` → `std::is_sorted` + membership + explicit `bHasTombstoned==false`. Существующая проверка «`winner definitions use canonical ID order`» (попарная сортировка) уже была property-based — не тронута.
    - Дополнительно найдено и исправлено **вживую при верификации TAS-11** (не входило в исходный список задачи, но тот же класс проблемы): `Tools/Content/test_authoring_tools.py` пинил `active_ids: 15`/`[kind] (N)`/`total_active_ids==15` для **`GameData/core`** (не корпуса!) — заменено на regex-проверку формата + инвариант «total равен сумме списков по kind»; и `Source/GV2/Private/Tests/GV2RuntimeCoreTests.cpp`'s embedded Lua `game.repository.list(...)` проверки (полный список по индексам для screen/text/actor/item) — заменены на `assert_sorted`/`list_contains`.
    - `ctest` 57/57, UE `GV2.Runtime` 54/54.

- [x] **TAS-11 — Проверить независимость**
  - Зависимости: TAS-09, TAS-10.
  - Done: добавление сущности в `GameData/core` и отдельно в замороженный корпус проверено вручную; в первом случае не меняется ничего в `Tests/` и `Source/`, во втором меняется только соответствующее pinned-значение; результат зафиксирован в change set.
  - Evidence: Обе стороны проверены вживую временными пробами (`core:text.smoke.tas11_probe_a`/`_b`, каждая удалена после проверки).
    - **`GameData/core` независим**: добавление пробы дало реальное изменение hash `GameData/core` (подтверждено `gv2-content hash`); первый прогон вскрыл `gv2_content_authoring_tools_python` (пинил `active_ids: 15` для `GameData/core` — не входил в исходный список TAS-10, исправлен на месте). После фикса: `ctest` 57/57, UE `GV2.Runtime` 54/54 — ни один файл под `Tests/`/`Source/` не пришлось редактировать ради самой пробы (только заранее найденный `test_authoring_tools.py`).
    - **Замороженный корпус меняет только pinned-значения**: добавление пробы в `Tests/Fixtures/PortableContentCore/valid/core` корректно и предсказуемо провалило ровно три теста, все три — про pinned-значения, ни один — про код: `gv2_content_hash_core_fixture`/`CrossHostParity` (`expected_core_content_hash.txt`), `pcc_shared_json5_fixture_conformance_failed` внутри `gv2-headless --self-test`/`RepositoryResolutionM4` (`expected_merged_content_hash.txt`), `gv2_headless_golden_replay_matches_digest`/`CrossHostDigestParity` (golden fixtures, TAS-08). Первый прогон также вскрыл `GV2.Runtime.Lua.RepositoryAccess` (embedded Lua `list()`-проверки с полным индексным списком) — не входил в исходный список TAS-10, исправлен на месте (см. Evidence TAS-10). После фикса и отката пробы: `ctest` 57/57, UE `GV2.Runtime` 54/54.

## Проверка milestone

- [x] Каждое pinned-значение имеет один источник.
- [x] Ни один тест не ломается от добавления сущности.
- [x] Оставшиеся утверждения о количестве объяснены.
