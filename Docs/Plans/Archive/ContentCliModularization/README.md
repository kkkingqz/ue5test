---
title: Content CLI Modularization Implementation Plan
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - ../../../Architecture/BuildAndTooling.md
  - ../../../Architecture/DefinitionEnvelopeAndSchemaRules.md
decisions:
  - ../../../ADR/0018-portable-content-core-module.md
  - ../../../ADR/0019-content-host-support-module.md
  - ../../../ADR/0023-stable-id-publication-freeze.md
---

# План реализации Content CLI Modularization

> **Архив.** План выполнен полностью (M1–M4) и больше не является источником задач. Нормативное поведение перенесено в [Build and Tooling](../../../Architecture/BuildAndTooling.md). Документ сохраняется как implementation record.

## Цель

Провести архитектурный рефакторинг CLI-утилиты `gv2-content`, разбив монолитный файл `Tools/Content/Source/main.cpp` (3381 строк) на изолированные модули команд, поддержки AST-модификаций и файлового наблюдения под `Tools/Content/Source/`.

## Состояние на входе

Утилита `gv2-content` поддерживает 9 команд (`validate`, `inspect`, `describe`, `new`, `refs`, `rename`, `index`, `hash`, `coverage`), флаги `--watch`, `--format=text|json`, `--locale`, `--poll-interval`, `--max-iterations`, `--provenance`. Вся логика реализована в одном файле `Tools/Content/Source/main.cpp`, включая:
1. Парсинг опций командной строки и вывод `--help`.
2. AST-парсер/токен-модификатор JSON5 для команд `new` и `rename`.
3. Модуль слежения за изменениями файлов для режима `validate --watch`.
4. Форматирование JSON и текстового вывода.
5. Индивидуальные обработчики для каждой команды.

## Принятые решения

- Разбить код на подкаталоги в `Tools/Content/Source/`:
  - `Commands/`: отдельные классы/функции для каждой подкоманды.
  - `Support/`: общие утилиты форматирования, AST-манипулятор JSON5 и snapshot/watcher файловой системы.
  - `main.cpp`: легковесный CLI-роутер, разбор общих аргументов и вызов диспетчеров команд.
- Поведение, exit codes и форматы вывода команд (`text`/`json`) остаются на 100% бинарно и семантически совместимыми.
- Обновить `Source/CMakeLists.txt` для сборки модульных исходных файлов `gv2-content`.

## Milestones

- [x] M1 — [Support Components](SupportComponents.md): выделение `Json5AstRewriter`, `DirectoryWatcher` и `CliOutputFormatter`.
- [x] M2 — [Query Commands](QueryCommands.md): модули команд `inspect`, `describe`, `refs`, `index`, `hash`.
- [x] M3 — [Mutation and Analysis Commands](MutationAndAnalysisCommands.md): модули команд `new`, `rename`, `validate`, `coverage`.
- [x] M4 — [Router and Verification](RouterAndVerification.md): модульный `main.cpp`, верификация авторских тестов и CI.

## Критический путь

```text
Support Components
→ Query Commands
→ Mutation and Analysis Commands
→ Router and Verification
```

## Итоговый Definition of Done

- [x] `Tools/Content/Source/main.cpp` содержит только CLI-роутер и верхнеуровневый разбор аргументов (< 170 строк).
- [x] Каждая подкоманда изолирована в своём модуле в `Tools/Content/Source/Commands/`.
- [x] Общие вспомогательные механизмы вынесены в `Tools/Content/Source/Support/`.
- [x] Все 57 CTest тестов, включая `gv2_content_authoring_tools_python`, проходят без изменений поведения.
