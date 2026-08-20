---
title: TestArchitectureAndLuaSpecs Archive Summary
status: archived
version: 1.0
updated: 2026-08-15
---

# TestArchitectureAndLuaSpecs: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Сделать расширение контента и добавление gameplay-правил операциями, не требующими C++. Правило — файл в `Scripts/`, проверка — файл в `Tests/Lua/`, контент — файл в `GameData/`

**Результат:** Сделать расширение контента и добавление gameplay-правил операциями, не требующими C++. Правило — файл в `Scripts/`, проверка — файл в `Tests/Lua/`, контент — файл в `GameData/`

## Этапы и задачи

### M1 — Lua Spec Runner

Один generic portable entry point находит и исполняет Lua-спеки из `Tests/Lua/`. Оба host-а вызывают его; добавление спеки расширяет покрытие обоих без единой строки C++

- `TAS-01` — Определить формат спеки
- `TAS-02` — Реализовать обнаружение и исполнение
- `TAS-03` — Зафиксировать идентичность провала
- `TAS-04` — Подключить оба host-а
- `TAS-05` — Обновить контракты

### M2 — Frozen Test Corpus

Тестовый корпус и контент игры перестают быть одним деревом. Добавление сущности в `GameData/core` не трогает `Tests/`

- `TAS-06` — Заморозить тестовый корпус
- `TAS-07` — Развести pinned-хэши
- `TAS-08` — Перевести golden-прогон на тестовый корпус

### M3 — Content Independent Assertions

Тесты утверждают свойства корпуса, а не его размер, и читают каждое pinned-значение из единственного источника

- `TAS-09` — Убрать дублирование pinned-значений
- `TAS-10` — Заменить переписи на свойства
- `TAS-11` — Проверить независимость

### M4 — Migration and Gate

Runner доказан на реальных наборах, а возврат к C++-проверке Lua-правил обнаруживается автоматически

- `TAS-12` — Мигрировать наборы GEW-04 и GEW-05
- `TAS-13` — Мигрировать наборы command validators
- `TAS-14` — Расширить гейт паритета
- `TAS-15` — Синхронизировать документацию и снять паузу

## Актуальные нормативные источники

- [HeadlessSimulationContract](../../Architecture/HeadlessSimulationContract.md)
- [BuildAndTooling](../../Architecture/BuildAndTooling.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/TestArchitectureAndLuaSpecs) содержит исходные task-файлы, acceptance criteria и evidence.
