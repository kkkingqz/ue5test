---
title: Demo Out Tasks
status: draft
version: 1.0
updated: 2026-08-17
depends_on:
  - README.md
  - ../../Architecture/Modding.md
---

# M1 — Demo Out

> **Материализует:** [ADR-0026](../../ADR/0026-core-and-gameplay-ownership.md) в части demo content.
> **Задачи:** CBM-01…03.
> **Результат:** демонстрационный экран и его контент перестают быть частью движка.

## Результат этапа

`core:screen.test`, тексты демо и `Scripts/debug/start.lua` уезжают из ядра. Движок перестаёт содержать контент, существующий только чтобы что-то показать.

## Задачи

- [ ] **CBM-01 — Завести пакет `sample`**
  - Отдельный пакет для демонстрационного контента: манифест, `definitions/`, `scripts/`, зависимость от `core`.
  - Done: пакет валиден и грузится обоими хостами; не входит в игровой набор по умолчанию — подключается явно; пустой пакет не является ошибкой.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **CBM-02 — Перенести демо-экран и его модуль**
  - Зависимости: CBM-01.
  - `core:screen.test` и `core:text.screen.test.*` — в `sample`; `Scripts/debug/start.lua` — в `sample/scripts/`.
  - Done: ID меняют namespace на `sample:`; модуль объявлен в манифесте пакета; команды демо (`core:command.test.*`, `core:command.debug.start`) переезжают вместе с ним и меняют namespace; `boundary/ingress.lua` и манифест ядра не упоминают демо; при отключённом `sample` сессия стартует и игра работает.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **CBM-03 — Развязать проверки от демо**
  - Зависимости: CBM-02.
  - Golden-прогон и часть спеков опираются на `core:screen.test` как на конечный экран.
  - Done: прогоны, которым нужен демо-экран, подключают `sample` явно; спеки, которым он не нужен, от него не зависят; `final_screen_id` в golden получает осознанное значение, а не унаследованное; эталон обновлён воспроизведением манифеста.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] `GameData/core/definitions/` не содержит демонстрационных сущностей.
- [ ] `Scripts/debug/` отсутствует.
- [ ] Игра работает без пакета `sample`.
- [ ] `repository_content_hash` golden не изменился.
