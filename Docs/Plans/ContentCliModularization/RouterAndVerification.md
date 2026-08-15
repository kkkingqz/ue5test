---
title: Router and Verification Tasks
status: draft
version: 1.0
updated: 2026-08-15
depends_on:
  - README.md
  - SupportComponents.md
  - QueryCommands.md
  - MutationAndAnalysisCommands.md
  - ../../Architecture/BuildAndTooling.md
---

# M4 — Router and Verification

## Результат этапа

Точка входа `Tools/Content/Source/main.cpp` сведена к компактному роутеру команд, проект пересобирается, авторские тесты и документация синхронизированы.

## Задачи

- [x] **CCM-08 — Упрощение `main.cpp` и обновление `Source/CMakeLists.txt`**
  - Описание: Заменить монолитный `main.cpp` на компактный CLI-роутер (< 150-170 строк), регистрирующий и вызывающий диспетчеры команд. Добавить все новые `.cpp` файлы в `Tools/Content/CMakeLists.txt`.
  - Done: CLI собирается из модульных файлов; `main.cpp` сокращён до 167 строк.
  - Evidence: `Tools/Content/Source/main.cpp`, `Tools/Content/CMakeLists.txt`; CTest (57/57 passed).

- [x] **CCM-09 — Комплексная верификация и синхронизация документации**
  - Описание: Запустить полный набор авторских тестов `Tools/Content/test_authoring_tools.py`, убедиться в прохождении всех 57 CTest тестов; обновить `Docs/Architecture/BuildAndTooling.md` и `Docs/ImplementationStatus.md`.
  - Done: 100% совместимость проверена, тесты и валидация документации зелёные.
  - Evidence: CTest (57/57 passed), `validate_docs.py`, `validate_host_conformance_parity.py`.

## Проверка milestone

- [x] `Tools/Content/Source/main.cpp` не содержит логики отдельных команд.
- [x] Все существующие тесты утилит авторинга проходят без изменений.
- [x] `validate_docs.py` и `validate_host_conformance_parity.py` проходят успешно.
