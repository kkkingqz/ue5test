---
title: Test Architecture and Lua Specs Implementation Plan
status: archived
version: 2.0
updated: 2026-08-15
depends_on:
  - ../../../Architecture/HeadlessSimulationContract.md
  - ../../../Architecture/BuildAndTooling.md
decisions:
  - ../../../ADR/0020-cpp-scope-criterion.md
  - ../../../ADR/0024-lua-spec-runner.md
---

# План реализации Test Architecture and Lua Specs

> **Архив.** План выполнен полностью (M1–M4) и больше не является источником задач. Нормативное поведение перенесено в [Headless Simulation Contract](../../../Architecture/HeadlessSimulationContract.md) и [Build and Tooling](../../../Architecture/BuildAndTooling.md). Документ сохраняется как implementation record.

## Цель

Сделать расширение контента и добавление gameplay-правил операциями, не требующими C++. Правило — файл в `Scripts/`, проверка — файл в `Tests/Lua/`, контент — файл в `GameData/`.

План материализует [ADR-0024](../../../ADR/0024-lua-spec-runner.md) и чинит сцепление тестов с контентом, обнаруженное при реализации GEW-04/GEW-05.

## Исходное состояние

Добавление одной локации в GEW-05 потребовало правок примерно в десяти местах. Причины две и они независимы.

**Тесты сцеплены с контентом.** `GameData/core` и `Tests/Fixtures/PortableContentCore/valid/core` побайтово идентичны — продакшн-контент одновременно является тестовым корпусом. Pinned-хэш продублирован помимо `Tests/Fixtures/expected_core_content_hash.txt` ещё в `Headless/CMakeLists.txt`, `Headless/Source/main.cpp` и `GV2ContentCoreRepositoryResolutionTests.cpp`. Часть тестов утверждает переписи корпуса (`Definitions->AsArray().size() == 17`, `KindCounts.size() == 6`, `Screens.size() == 3`), а не свойства, поэтому любая новая сущность ломает счёт.

**Проверка Lua-правила требует C++.** Conformance-обвязка занимает 8794 строки, из них 5326 — один `GV2LuaLifecycleConformance.cpp`. GEW-04 и GEW-05 добавили примерно 50 строк Lua-логики и 1014 строк C++ тестовой обвязки.

## Принятые решения

- Спеки живут в `Tests/Lua/` — отдельном дереве, не входящем в `Scripts/` и не попадающем в упакованную игру.
- Тестовый корпус создаётся замораживанием текущей копии `valid/core` и дальше живёт независимо от `GameData/core`.
- Планы `GameplayEventsAndWorld` (этапы M3–M5) приостановлены на время выполнения: писать новую C++-обвязку, зная, что её придётся переносить, смысла нет.

## Границы

Входят: Lua spec runner и его исполнение обоими host-ами, разведение продакшн-контента и тестового корпуса, единый источник для каждого pinned-значения, замена переписей на утверждения свойств, миграция наборов, добавленных GEW-04/GEW-05, и расширение гейта паритета.

Не входят: массовая миграция существующих 8794 строк, изменение рантайма, GUI, изменение формата контента.

## Milestones

- [x] M1 — [Lua Spec Runner](LuaSpecRunner.md): проверка Lua-правила без единой строки C++.
- [x] M2 — [Frozen Test Corpus](FrozenTestCorpus.md): контент игры и тестовый корпус независимы.
- [x] M3 — [Content Independent Assertions](ContentIndependentAssertions.md): тесты не знают размер корпуса и не дублируют хэши.
- [x] M4 — [Migration and Gate](MigrationAndGate.md): доказательство на реальных наборах и запрет возврата.

## Критический путь

```text
Lua Spec Runner
→ Migration and Gate
```

```text
Frozen Test Corpus
→ Content Independent Assertions
```

Две ветви независимы и лечат разные болезни. M2–M3 короче и дают немедленный эффект: после них добавление контента перестаёт трогать тесты. M1 дороже, но снимает постоянную цену за каждое правило.

## Общие правила выполнения

1. Спека не зависит от других спек и от порядка исполнения.
2. Провал спеки даёт стабильный идентификатор `<spec>.<case>`, одинаковый в обоих host-ах.
3. Тест не утверждает количество сущностей в корпусе, если само количество не является предметом проверки.
4. Каждое pinned-значение объявлено ровно в одном файле; остальные потребители читают его оттуда.
5. Миграция набора не меняет содержание проверок: перенос отдельно, расширение покрытия отдельно.

## Итоговый Definition of Done

- [x] Новое gameplay-правило проверяется без добавления C++.
- [x] Добавление контента в `GameData/core` не требует правок в `Tests/` и `Source/`.
- [x] Каждое pinned-значение имеет ровно один источник.
- [x] Ни один тест не утверждает перепись корпуса (кроме одного явно объяснённого случая — покрытие ровно шести kind в `MinimalCoreSchemas`).
- [x] Новый C++ conformance entry point для Lua-правила обнаруживается CI.
