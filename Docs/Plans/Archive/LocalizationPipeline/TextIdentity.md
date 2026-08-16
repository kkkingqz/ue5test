---
title: Text Identity Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/DefinitionEnvelopeAndSchemaRules.md
decisions:
  - ../../../ADR/0022-external-translation-catalog.md
---

# M1 — Text Identity

## Результат этапа

Definition kind `text` остаётся реестром идентификаторов и хранит исходную строку, но перестаёт быть местом хранения переводов.

## Задачи

- [x] **LOC-01 — Изменить схему `text`**
  - `data.message` заменяется на `data.source_message`: строка на языке авторинга, обязательная, непустая.
  - Done: проверка `text_id` как typed reference kind `text` не изменилась; определения `GameData/core` и фикстур мигрированы; pinned content hash обновлён в этом же change set с явным указанием причины.
  - Evidence: Схемы `GameData/core/schemas/text_v1.schema.json5` и `Tests/Fixtures/PortableContentCore/valid/core/schemas/text_v1.schema.json5` обновлены (`fields.source_message`), определения `texts.json5` (core и test_mod) переведены на `source_message`; обновлены `Source/GV2RuntimeCore/Private/GV2LuaRepositoryConformance.cpp`, `Tests/Fixtures/expected_core_content_hash.txt` (`0e68f1736f301f313ee30764c2685701ddd357d1453e3696388e6528fa15c581`), `Tests/Fixtures/expected_merged_content_hash.txt` (`edd7f97fc074ace9950ddad7979f1709c8d8e5de40fe86afa01cf494b0789d3c`), `Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.{manifest,digest}.json5`; `gv2-headless --self-test` и CTest (57/57 passed).

- [x] **LOC-02 — Зафиксировать назначение исходной строки**
  - `source_message` служит контекстом переводчика и fallback при отсутствии перевода; она не является «переводом на язык по умолчанию» и не заменяет каталог.
  - Done: правило записано в `DefinitionEnvelopeAndSchemaRules` или в контракте текста; negative case на пустую строку; gameplay не читает `source_message` как условие.
  - Evidence: Документировано в `Docs/Architecture/DefinitionEnvelopeAndSchemaRules.md` (семантика `source_message`, запрет чтения gameplay-логикой, fallback-назначение) и `Docs/Architecture/GameDataRepositoryContract.md` (ссылки kind `text`); negative test case на пустую строку добавлен в `Source/GV2ContentCore/Private/ScalarValidationConformance.cpp` (`scalar_validation.empty_string_min_length_rejected`); CTest (57/57 passed), `validate_docs.py` и `validate_host_conformance_parity.py` зелёные.

## Проверка milestone

- [x] Опечатка в `text_id` остаётся фатальной диагностикой.
- [x] Схема `text` не содержит per-locale полей.
- [x] Смена pinned hash объяснена в change set.
