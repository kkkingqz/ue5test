---
title: Schema Driven Authoring Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - ../../../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# M1 — Schema Driven Authoring

## Результат этапа

Автор получает справочник полей и валидную заготовку из тех же схем, которые использует рантайм. Ручное воспроизведение конверта и угадывание полей больше не нужны.

## Задачи

- [x] **CAT-01 — Реализовать `gv2-content describe`**
  - `describe <package-root> <definition-type>` печатает поля схемы: имя, kind, обязательность, ограничения, для ссылок — ожидаемый target kind и resource class.
  - Done: вывод порождается из схемы, а не из копии в документации; поддержаны `--format=text|json`; неизвестный тип даёт typed tool failure; extension-схемы перечисляются отдельно от основной.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunDescribe`, `FormatFieldDetailsText`, `WriteFieldSpecJson`), `Tools/Content/CMakeLists.txt` (`gv2_content_describe_item_text`, `gv2_content_describe_item_json`, `gv2_content_describe_rejects_unknown_type`, `gv2_content_describe_rejects_unknown_type_json`), CTest (27/27 passed).

- [x] **CAT-02 — Реализовать `gv2-content new`**
  - `new <package-root> <definition-type> <definition-id>` создаёт валидную заготовку: конверт файла, обязательные поля с плейсхолдерами, необязательные — опущены.
  - Done: результат немедленно проходит `validate`, если плейсхолдеры заполнены корректными значениями; команда отказывается перезаписывать существующий ID; при отсутствии файла нужного типа он создаётся, при наличии — запись добавляется в него; ID проверяется на грамматику до записи.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunNew`, `GeneratePlaceholderValue`, `FormatDefinitionEntry`, `InsertDefinitionEntryIntoJson5`, `CreateNewDefinitionFileContent`), `Tools/Content/test_authoring_tools.py`, `Tools/Content/CMakeLists.txt` (`gv2_content_new_rejects_duplicate_id`, `gv2_content_new_rejects_invalid_id`, `gv2_content_new_rejects_id_kind_mismatch`, `gv2_content_authoring_tools_python`), CTest (36/36 passed).

- [x] **CAT-03 — Описать команды в контракте**
  - Зависимости: CAT-01, CAT-02.
  - Done: `BuildAndTooling` описывает обе команды и их exit codes; добавлены CTest на успешный и на отказной путь каждой команды.
  - Evidence: `Docs/Architecture/BuildAndTooling.md` (документированы команды `describe` и `new`, exit codes, форматы вывода), `Docs/ImplementationStatus.md`, `Tools/Content/test_authoring_tools.py`, `Tools/Content/CMakeLists.txt` (CTest 36/36 passed).

## Проверка milestone

- [x] Справочник полей порождается из схемы и не может разойтись с ней.
- [x] Созданная заготовка проходит валидацию.
- [x] Обе команды покрыты CTest на оба пути.
