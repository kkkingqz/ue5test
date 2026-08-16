---
title: Headless Deterministic Replay Tasks
status: archived
version: 1.0
updated: 2026-08-14
depends_on:
  - ConformanceConsolidation.md
  - ../../../Architecture/HeadlessSimulationContract.md
  - ../../../Architecture/CommandsAndEvents.md
decisions:
  - ../../../ADR/0010-portable-runtime-and-headless-simulation.md
---

# M2 — Deterministic Replay

## Результат этапа

Прогон описывается run manifest и сводится в run digest. Одинаковый manifest даёт одинаковый digest в `gv2-headless` и в UE integration-тесте. Golden-манифесты хранятся как fixtures и проверяются CI.

## Задачи

- [x] **HPR-08 — Определить run manifest**
  - Зависимости: HPR-07.
  - Manifest содержит exact Lua release, `repository_content_hash`, seed и упорядоченный список принятых команд.
  - Done: manifest сериализуется и читается обратно без потерь; он не содержит абсолютных путей, таймингов и localized текста.
  - Evidence: Добавлены `FRunAcceptedCommand`, `FRunManifest`, `SerializeRunManifest`, `DeserializeRunManifest` в `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RunManifest.h` и `Private/GV2RunManifest.cpp`; реализован переносимый тест `RunRunManifestConformance()` в `GV2RunManifestConformance.h/.cpp`; добавлен UE Automation Test `GV2RuntimeCoreRunManifestTests.cpp`; CTest (19/19) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-09 — Определить run digest**
  - Зависимости: HPR-08.
  - Digest — детерминированная свёртка наблюдаемого результата прогона.
  - Done: зафиксировано, что входит в digest и что исключено; повторный прогон одного manifest даёт тот же digest; изменение принятой команды меняет digest.
  - Evidence: Добавлены `FRunResult`, `FRunDigest`, `ComputeRunDigest`, `SerializeRunDigest`, `DeserializeRunDigest` в `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RunDigest.h` и `Private/GV2RunDigest.cpp`; экспортирован `ComputeCanonicalHash` в `Source/GV2ContentCore/Public/GV2ContentCore/CanonicalHash.h`; реализован переносимый тест `RunRunDigestConformance()` в `GV2RunDigestConformance.h/.cpp`; добавлен UE Automation Test `GV2RuntimeCoreRunDigestTests.cpp`; CTest (19/19) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-10 — Выводить manifest и digest**
  - Зависимости: HPR-09.
  - Machine-readable вывод прогона расширяется manifest и digest.
  - Done: формат стабилен, парсится тестами, не зависит от locale и порядка полей.
  - Evidence: В `Headless/Source/main.cpp` добавлены CLI флаги `--output-manifest` и `--output-digest`, stdout расширен полями `digest_hash` и структурированным объектом `digest`; добавлен CTest `gv2_headless_outputs_manifest_and_digest` в `Headless/CMakeLists.txt`; обновлён `Docs/Architecture/BuildAndTooling.md`. CTest (20/20) успешно пройден.

- [x] **HPR-11 — Реализовать replay записанного manifest**
  - Зависимости: HPR-08.
  - Host принимает manifest вместо CLI-параметров прогона и исполняет ровно записанную последовательность команд.
  - Done: несовпадение `repository_content_hash` завершает прогон как configuration failure до bootstrap; отклонённая команда не изменяет последовательность молча.
  - Evidence: Добавлен `ReplayRunManifest` в `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2RunReplay.h` и `Private/GV2RunReplay.cpp`; реализован переносимый тест `RunRunReplayConformance()` в `GV2RunReplayConformance.h/.cpp`; добавлен UE Automation Test `GV2RuntimeCoreRunReplayTests.cpp`; в `Headless/Source/main.cpp` добавлен флаг `--manifest` с валидацией `repository_content_hash` (exit code 2); CTest (20/20) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-12 — Подтвердить digest в UE**
  - Зависимости: HPR-09, HPR-11.
  - Unreal integration-тест исполняет тот же manifest и сравнивает digest.
  - Done: расхождение digest между host-ами ломает сборку; тест не использует presentation и не требует ассетов сверх обычного bootstrap.
  - Evidence: Создан Unreal Automation Test `GV2.Runtime.Session.CrossHostDigestParity` в `Source/GV2/Private/Tests/GV2RuntimeCoreCrossHostDigestTests.cpp`, подтверждающий совпадение `FRunDigest` при исполнении одинакового `FRunManifest` поверх `GameData/core` между Headless и Unreal Engine хостами. Все тесты (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-13 — Завести golden-манифесты**
  - Зависимости: HPR-10, HPR-12.
  - Минимум один golden-прогон на существующем corpus хранится в fixtures вместе с ожидаемым digest.
  - Done: CTest и Unreal automation сверяют digest с golden; изменение наблюдаемого результата требует осознанного обновления golden в том же change set.
  - Evidence: Добавлены фикстуры `Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.manifest.json5` и `golden_headless_10_seed_42.digest.json5`; добавлен CTest `gv2_headless_golden_replay_matches_digest`; Unreal Automation Test `GV2.Runtime.Session.CrossHostDigestParity` загружает и валидирует золотой дайджест; CTest (21/21) и UE Automation Tests (`EXIT CODE: 0`) успешно пройдены.

- [x] **HPR-14 — Синхронизировать документацию**
  - Зависимости: HPR-13.
  - Done: `HeadlessSimulationContract` описывает фактический формат manifest/digest и режим replay; `BuildAndTooling` описывает CLI-поверхность и новые CTest; `ImplementationStatus` отражает изменившийся объём реализованного.
  - Evidence: Обновлены `Docs/Architecture/HeadlessSimulationContract.md` (структура манифеста, дайджеста, семантика ReplayRunManifest, CLI), `Docs/Architecture/BuildAndTooling.md` (CLI-опции, exit code 2, conformance table), `Docs/Status/ImplementationStatus.md` (статус deterministic replay: Реализовано) и `Docs/Plans/HeadlessParityAndReplay/README.md`.

## Проверка milestone

- [x] Повторный прогон одного manifest даёт идентичный digest.
- [x] Digest не зависит от таймингов, host-а и числа прогонов.
- [x] UE и headless дают одинаковый digest на одном manifest.
- [x] Golden-прогон проверяется CI и ломается при изменении наблюдаемого результата.
