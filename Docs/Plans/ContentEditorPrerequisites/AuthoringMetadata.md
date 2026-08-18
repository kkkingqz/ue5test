---
title: Authoring Metadata Tasks
status: draft
version: 1.0
updated: 2026-08-18
depends_on:
  - README.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# M3 — Authoring Metadata

> **Материализует:** [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) в части данных для авторского интерфейса.
> **Задачи:** CEP-08…12.
> **Результат:** форма строится по объявленным подписям и порядку, а не по случайному порядку полей в файле схемы.

## Результат этапа

Перестановка полей в файле схемы перестаёт менять интерфейс, а подписи перестают быть английскими идентификаторами.

## Задачи

- [x] **CEP-08 — Формат метаданных**
  - Зависимости: CEP-01.
  - Файл рядом со схемой: `schemas/<name>.ui.json5`. Изоляция от `content_hash` получается структурно, как у PO-каталогов, а не избирательностью в нормализации.
  - Done: формат объявляет для поля `label`, `description`, `category`, `order` и подсказку виджета; файл необязателен, его отсутствие не является ошибкой; неизвестный ключ формата — ошибка, а не молчаливое игнорирование; ключи, не разрешающиеся в поле схемы, отвергаются.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/AuthoringMetadata.h`, `Source/GV2ContentCore/Private/AuthoringMetadata.cpp`; conformance `RunAuthoringMetadataConformance` (`AuthoringMetadataConformance.cpp`) покрывает валидный парсинг, `unresolved_field`, `unknown_field` и `invalid_type`; `validate_host_conformance_parity.py` подтвердил 29 entry points; UE automation test `GV2ContentCoreAuthoringMetadataTests.cpp`.

- [x] **CEP-09 — Изоляция от сборки репозитория**
  - Зависимости: CEP-08.
  - Done: `.ui.json5` не участвует в сборке репозитория и не влияет на `content_hash` — проверяется тестом, меняющим метаданные и сверяющим хэш до и после, по образцу проверки PO-каталогов; невалидный файл метаданных не блокирует сборку контента, а даёт диагностику своего класса.
  - Evidence: `Source/GV2ContentHostSupport/Private/PackageDiscovery.cpp` явно пропускает `*.ui.json5` при сканировании каталога `schemas/`; тест 40d в `Tools/Content/test_authoring_tools.py` подтвердил инвариантность `content_hash` при добавлении/модификации `.ui.json5`.

- [x] **CEP-10 — Выдача в `describe`**
  - Зависимости: CEP-08.
  - Done: `gv2-content describe --format=json` отдаёт метаданные рядом с описанием поля; отсутствие метаданных даёт предсказуемый пустой блок, а не отсутствие ключа; порядок полей в выдаче определяется `order`, а при его отсутствии — порядком объявления в схеме, и это записано.
  - Evidence: `Tools/Content/Source/Commands/DescribeCommand.cpp` загружает парные `.ui.json5`, выполняет `std::stable_sort` полей по `order` (и оригинальному индексу при совпадении), генерирует блок `"ui": { ... }` (или `"ui": {}` при отсутствии) и форматирует UI-свойства в текстовом выводе; тесты 40a..40f в `Tools/Content/test_authoring_tools.py` и CTests `gv2_content_describe_*`.

- [x] **CEP-11 — Гейт на устаревшие метаданные**
  - Зависимости: CEP-08.
  - Риск отдельного файла — расхождение: поле переименовали, запись осталась.
  - Done: проверка ломает CI, если ключ метаданных не разрешается в поле схемы; сообщение называет файл, ключ и схему; негативный тест подтверждает срабатывание; проверка зарегистрирована в CTest рядом с прочими гейтами контента.
  - Evidence: `Tools/Content/validate_authoring_metadata.py` реализует валидацию дерева пакетов и `--self-test`; зарегистрирован в корневом `CMakeLists.txt` как CTests `authoring_metadata_gate_contract` и `authoring_metadata_gate_negative_contract`; тест 40h в `Tools/Content/test_authoring_tools.py` проверяет отказ на устаревшем поле.

- [x] **CEP-12 — Синхронизация документации**
  - Зависимости: CEP-02, CEP-07, CEP-11.
  - Done: [Definition Envelope and Schema Rules](../../Architecture/DefinitionEnvelopeAndSchemaRules.md) описывает метаданные и их изоляцию; [Build and Tooling](../../Architecture/BuildAndTooling.md) — новый гейт; в разделе 40 предложения о редакторе три пункта отмечены закрытыми со ссылкой на этот план; в [Simplified Authoring Surface](../../Proposals/SimplifiedAuthoringSurfaceProposal.md) снята пометка о блокировке декларативных экранов; [Implementation Status](../../Status/ImplementationStatus.md) обновлён.
  - Evidence: Обновлены `Docs/Architecture/DefinitionEnvelopeAndSchemaRules.md`, `Docs/Architecture/BuildAndTooling.md`, `Docs/Proposals/ContentEditorPluginProposal.md` §40, `Docs/Proposals/SimplifiedAuthoringSurfaceProposal.md` §11.1, `Docs/Plans/ContentEditorPrerequisites/README.md`, `Docs/Status/ImplementationStatus.md`; валидация `Tools/Documentation/validate_docs.py` пройдена без ошибок.

## Проверка milestone

- [x] Метаданные читаются `describe` и не влияют на `content_hash`.
- [x] Отсутствие файла метаданных не является ошибкой.
- [x] Устаревший ключ ломает CI.
- [x] Порядок полей в форме определяется объявленным `order`.
