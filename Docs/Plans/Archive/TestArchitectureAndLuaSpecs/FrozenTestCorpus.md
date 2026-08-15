---
title: Frozen Test Corpus Tasks
status: archived
version: 1.1
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/BuildAndTooling.md
---

# M2 — Frozen Test Corpus

## Результат этапа

Тестовый корпус и контент игры перестают быть одним деревом. Добавление сущности в `GameData/core` не трогает `Tests/`.

## Задачи

- [x] **TAS-06 — Заморозить тестовый корпус**
  - `Tests/Fixtures/PortableContentCore/valid/core` фиксируется в текущем виде и объявляется независимым от `GameData/core`.
  - Done: корпус больше не является зеркалом; его изменение допускается только когда предметом изменения являются сами правила разрешения контента; правило записано рядом с корпусом и в `BuildAndTooling`.
  - Evidence: `Tests/Fixtures/PortableContentCore/README.md` получил раздел «Frozen» прямо под заголовком: `valid/core`/`valid/test_mod` больше не растут вместе с `GameData/core`, изменение допустимо только когда предметом являются сами правила разрешения контента (parsing/schema/override/redirect/tombstone/provenance), не gameplay-сущности; заодно исправлены устаревшие числа в этом же файле (`core has nine definitions`/`eleven canonical winners`, актуальные с GEW-05 — пятнадцать/семнадцать). `Docs/Architecture/BuildAndTooling.md` получил тот же раздел «Заморожен (TAS-06)» в «Shared fixtures and conformance», со ссылкой на план. На момент заморозки корпус и `GameData/core` побайтово совпадают (это стартовая точка, не требование держать их синхронными впредь) — не переиндексировался (`fixtures.index` не менялся), только описание политики.

- [x] **TAS-07 — Развести pinned-хэши**
  - У продакшн-контента и у тестового корпуса появляются отдельные pinned-значения: изменение одного не затрагивает другое.
  - Done: `expected_core_content_hash.txt` относится к тестовому корпусу; хэш `GameData/core` либо не пинится вовсе, либо пинится отдельным файлом; смоук-проверка `gv2-content validate GameData/core` продолжает работать и не зависит от хэша.
  - Evidence: Выбран первый вариант из Done («не пинится вовсе»): CTest `gv2_content_hash_gamedata_core` (`Tools/Content/CMakeLists.txt`) удалён целиком; `gv2_content_validate_gamedata_core` (уже существовавший smoke-тест, только `gv2-content validate`, без сравнения хэша) остаётся единственной проверкой на `GameData/core`. `Tests/Fixtures/expected_core_content_hash.txt` остался pinned-значением ИСКЛЮЧИТЕЛЬНО для `valid/core` — попытка физически перенести файл внутрь `Tests/Fixtures/PortableContentCore/` (чтобы лежал рядом с корпусом) провалила CTest `pcc_shared_fixture_contract`: PCC-01 явно запрещает любой файл `expected*` внутри дерева корпуса (`Tools/Content/validate_pcc_fixtures.py`) — файл остался как sibling-директория, что и так уже разводит его с `GameData/core`. Обновлены оба потребителя: `Source/GV2/Private/Tests/GV2ContentCoreCrossHostParityTests.cpp` (комментарий уточнён, путь не менялся) и `Docs/Architecture/BuildAndTooling.md` («Раздельные pinned-значения (TAS-07)»). `ctest` 57/58 → 57/57 (минус удалённый тест), UE `GV2.Runtime` 54/54, `validate_host_conformance_parity.py` — 28 entry points без изменений.

- [x] **TAS-08 — Перевести golden-прогон на тестовый корпус**
  - Golden manifest и digest строятся на замороженном корпусе, а не на контенте игры.
  - Done: добавление сущности в `GameData/core` не меняет golden; изменение замороженного корпуса по-прежнему требует осознанного обновления golden; cross-host digest parity сохраняется.
  - Evidence: `Headless/CMakeLists.txt`: `gv2_headless_golden_replay_matches_digest` получил `--content-root=${GV2_PCC_FIXTURE_ROOT}/valid/core` — golden-прогон явно указывает на замороженный корпус вместо дефолтного резолва в `GameData/core` (флаг `--content-root` уже существовал и применяется единообразно к обоим режимам, включая `--manifest=`). `Source/GV2/Private/Tests/GV2RuntimeCoreCrossHostDigestTests.cpp`: репозиторий для сверки digest строится из `Tests/Fixtures/PortableContentCore/valid/core`, а не из `GameData/core`. Значения фикстур (`Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.{manifest,digest}.json5`) не изменились — на момент переключения оба дерева побайтово совпадали (TAS-06), поэтому смена источника прошла без регенерации golden.
    - Гарантия проверена вживую: временно добавлена сущность `core:text.smoke.tas08_probe` только в `GameData/core` (content hash изменился: `bcd34cd3...` → `c5ee4644...`) — `gv2_headless_golden_replay_matches_digest` и `gv2_content_hash_core_fixture` (пинит замороженный корпус) остались зелёными без единой правки; проба удалена, хэш `GameData/core` вернулся к `bcd34cd3...`.
    - `ctest` 57/57, UE `GV2.Runtime` 54/54 (`GV2.Runtime.Session.CrossHostDigestParity` — Success), `validate_host_conformance_parity.py` — 28 entry points.

## Проверка milestone

- [x] `GameData/core` и тестовый корпус — разные деревья с разной судьбой.
- [x] Добавление локации в игру не меняет ни одного файла в `Tests/`.
- [x] Golden-прогон не зависит от контента игры.
