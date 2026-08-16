---
title: Host Resolution Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - CatalogFormat.md
  - ../../../UI/README.md
decisions:
  - ../../../ADR/0013-unified-text-pipeline.md
  - ../../../ADR/0022-external-translation-catalog.md
---

# M3 — Host Resolution

## Результат этапа

UE разрешает `TextSpec` в отображаемый текст выбранной locale. Headless и Lua переводов не видят.

## Задачи

- [x] **LOC-05 — Собрать host-артефакт из PO**
  - Build-шаг превращает PO в артефакт, который загружает UE. На первом шаге это String Table; переход на `.locres` выполняется отдельно и не меняет источник истины.
  - Done: шаг детерминирован и воспроизводим; артефакт не хранится в репозитории как источник истины; отсутствие артефакта не ломает запуск, а включает fallback.
  - Evidence: Реализованы `GV2ContentHostSupport::ExportPoToStringTableCsv()` и `GV2ContentHostSupport::LoadPackageLocalization()` (`Source/GV2ContentHostSupport/Public/GV2ContentHostSupport/LocalizationDiscovery.h`, `.../Private/LocalizationDiscovery.cpp`); добавлен CLI-скрипт сборки `Tools/Content/compile_localization.py`; в `Tools/Content/test_authoring_tools.py` добавлен тест 33 на детерминированную компиляцию PO-каталогов в String Table CSV; CTest `gv2_content_authoring_tools_python` и все 57 тестов зелёные.

- [x] **LOC-06 — Разрешать `TextSpec` в Presentation**
  - Резолвинг выполняется существующим text pipeline: Lua публикует `TextSpec`, Presentation превращает его в `FText`.
  - Done: `TextSpec` пересекает boundary неразрешённым; Lua не получает resolved-строку и не может ветвиться по ней; аргументы подставляются на стороне presentation.
  - Evidence: В Lua модуль `Scripts/resources/text.lua` конструирует чистый DTO `{ text_id, args, style }` без обращения к переводам (верифицировано в `Tests/Lua/resources/text_spec.lua`); boundary передаёт неразрешённый `FTextSpec`; `FGV2ScreenFieldAdapterRegistry::ResolveText` и `UGV2TextPipeline::Resolve` на стороне presentation подставляют именованные аргументы через `FText::Format` и применяют токены стилей/нормализацию разметки (проверено в `GV2RuntimeSubsystemTests.cpp`); все 57 CTest тестов зелёные.

- [x] **LOC-07 — Зафиксировать fallback**
  - Отсутствующий перевод отображает `source_message`.
  - Done: отсутствие ключа не является ошибкой и не пишет fault; отсутствие всего каталога locale эквивалентно отсутствию всех ключей; headless отображает `source_message` без обращения к каталогу.
  - Evidence: В `UGV2UiTheme` добавлен `FallbackTextCatalog`, `UGV2TextPipeline::Resolve` при отсутствии ключа в активном каталоге локали `TextCatalog` прозрачно использует fallback на `source_message` без ошибки и без выброса fault (проверено в `GV2RuntimeSubsystemTests.cpp`); в Headless `FUnresolvedLocalizationAdapter` сохраняет неразрешённый `TextSpec` без загрузки каталогов локализации; CTest и все 57 тестов зелёные.

## Проверка milestone

- [x] UE показывает перевод, headless — исходную строку.
- [x] Lua не видит resolved-текста.
- [x] Отсутствующий перевод не ломает запуск.
