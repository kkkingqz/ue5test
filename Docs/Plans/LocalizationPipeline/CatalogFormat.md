---
title: Catalog Format Tasks
status: draft
version: 1.0
updated: 2026-08-15
depends_on:
  - TextIdentity.md
  - ../../Architecture/BuildAndTooling.md
decisions:
  - ../../ADR/0022-external-translation-catalog.md
---

# M2 — Catalog Format

## Результат этапа

Переводы лежат в PO-каталогах внутри package root, ключуются `text_id` и не влияют ни на discovery пакета, ни на его хэш.

## Задачи

- [x] **LOC-03 — Определить размещение и формат каталога**
  - Каталог: `<package-root>/localization/<locale>.po`, ключ — `text_id`, контекст переводчика — `source_message`.
  - Done: discovery пакета не сканирует `localization/` и не меняет поведение при её появлении; `content_hash` не зависит от содержимого каталогов; мод поставляет переводы тем же способом внутри своего package root.
  - Evidence: Документировано в `Docs/Architecture/BuildAndTooling.md` (конвенция `<package-root>/localization/<locale>.po`, игнорирование при discovery, изоляция `content_hash`); добавлен `GameData/core/localization/ru.po` (7 текстов); в `Tools/Content/test_authoring_tools.py` добавлен тест 32 на неизменность `gv2-content validate`, `index`, `hash` при создании и редактировании PO-каталогов; CTest `gv2_content_authoring_tools_python` и все 57 тестов зелёные.

- [x] **LOC-04 — Реализовать чтение PO**
  - Одна реализация разбора, используемая всеми потребителями.
  - Done: поддержаны многострочные значения и экранирование; повреждённый или неразбираемый файл даёт typed-ошибку с указанием строки и не приводит к частично загруженному каталогу; вторая реализация формата отсутствует и это проверяется так же, как parity conformance entry points.
  - Evidence: Реализован `GV2ContentCore::ParsePo()` (`Source/GV2ContentCore/Public/GV2ContentCore/PoParser.h`, `.../Private/PoParser.cpp`); conformance-набор `GV2ContentCore::Testing::RunPoParserConformance()` (`PoParserConformance.h`, `.../Private/PoParserConformance.cpp`) покрывает заголовки, контексты `msgctxt`, многострочную конкатенацию, экранирование, комментарии и отрицательные проверки (незакрытые кавычки, недопустимые escape-последовательности, пропущенные поля, дублирующиеся контексты); conformance вызывается в обоих хостах (`Headless/Source/main.cpp` и `GV2ContentCorePoParserTests.cpp`); `validate_host_conformance_parity.py` подтвердил 25 entry points.

## Проверка milestone

- [x] Появление и правка `localization/` не меняют `content_hash`.
- [x] PO разбирается одной реализацией.
- [x] Повреждённый каталог даёт typed-ошибку с позицией.
