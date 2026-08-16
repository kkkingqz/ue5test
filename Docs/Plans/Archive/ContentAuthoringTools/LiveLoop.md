---
title: Live Loop Tasks
status: archived
version: 1.0
updated: 2026-08-15
depends_on:
  - RenameSupport.md
  - ScriptFeedback.md
---

# M4 — Live Loop

## Результат этапа

Автор видит ошибку сразу после сохранения файла, а редактор подсказывает существующие идентификаторы вместо того, чтобы ждать запуска проверки.

## Задачи

- [x] **CAT-11 — Реализовать `validate --watch`**
  - Повторная проверка package root при изменении файлов; вывод в том же формате, что однократный запуск.
  - Done: режим не публикует repository и не пишет в `GameData`; ошибка чтения файла во время правки не завершает наблюдение; выход по сигналу корректен; повторный прогон не накапливает состояние между итерациями.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunValidate` с флагами `--watch`, `--poll-interval=MS`, `--max-iterations=N`, `TakeDirectorySnapshot`, обработкой сигналов SIGINT/SIGTERM и независимым выполнением каждой итерации), `Tools/Content/test_authoring_tools.py` (тесты 24-26 на однократный watch, многократный live-цикл с внесением/исправлением синтаксической ошибки и проверку отказа на других командах), `Tools/Content/CMakeLists.txt` (`gv2_content_validate_watch_*`), CTest (54/54 passed).

- [x] **CAT-12 — Реализовать `gv2-content index`**
  - Команда выгружает все идентификаторы корпуса, сгруппированные по kind, в machine-readable виде.
  - Done: индекс содержит active IDs и отдельно redirects/tombstones; порядок канонический; вывод пригоден как источник автодополнения для любого редактора без дополнительной обработки.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunIndex`, каноническая группировка активных ID по kind, извлечение redirects и tombstones, форматы `--format=text|json`), `Source/GV2ContentHostSupport/Private/PackageDiscovery.cpp` (парсинг `redirects` и `tombstones` из `package.json5`), `Tools/Content/test_authoring_tools.py` (тесты 27-30 на text, JSON, redirects/tombstones и отказы), `Tools/Content/CMakeLists.txt` (`gv2_content_index_*`), CTest (58/58 passed).

- [x] **CAT-13 — Собрать конфигурацию редактора**
  - Зависимости: CAT-11, CAT-12.
  - Минимальная интеграция для редактора, в котором работает автор: задача запуска `validate --watch` и подключение индекса как источника подсказок.
  - Done: конфигурация хранится в репозитории; она не является обязательной для сборки и её отсутствие ничего не ломает; инструкция по подключению описана в одном месте.
  - Evidence: `.vscode/tasks.json` (фоновая задача `GV2: Watch Content (GameData/core)` с встроенным `problemMatcher`, задачи валидации контента, проверки скриптов и генерации сниппетов), `.vscode/settings.json`, `Tools/Editor/generate_vscode_snippets.py` (генерация `.vscode/gv2-content.code-snippets` из `gv2-content index --format=json`), `Tools/Editor/README.md` (полная документация подключения и регулярных выражений для других IDE), `Tools/Content/test_authoring_tools.py` (тест 31 генерации сниппетов). При приёмке обнаружено, что `.gitignore` игнорировал `.vscode/` целиком, поэтому конфигурация фактически не попадала в репозиторий вопреки Done; исправлено заменой `.vscode/` на `.vscode/*` с отрицаниями для `tasks.json` и `settings.json` (git не может вернуть файл внутри исключённого каталога), сгенерированный `gv2-content.code-snippets` остаётся игнорируемым.

- [x] **CAT-14 — Синхронизировать документацию**
  - Зависимости: CAT-11–CAT-13.
  - Done: `BuildAndTooling` описывает полный набор команд `gv2-content` и режим `--check-scripts`; `ImplementationStatus` отражает появившийся tooling; `ContentDiagnosticsAndToolingProposal` отмечает реализованную часть.
  - Evidence: `Docs/Architecture/BuildAndTooling.md` (полный набор команд `gv2-content`, включая `validate --watch` и `index`, а также раздел «Интеграция с редакторами и Live Loop»), `Docs/Status/ImplementationStatus.md` (актуализирован статус `gv2-content CLI и Live Loop` по M1–M4), `Docs/Proposals/ContentDiagnosticsAndToolingProposal.md` (отмечена реализованная часть M1–M4 дорожной карты).

## Проверка milestone

- [x] Ошибка видна без ручного перезапуска проверки.
- [x] Индекс пригоден как источник автодополнения.
- [x] Отсутствие конфигурации редактора ничего не ломает.
