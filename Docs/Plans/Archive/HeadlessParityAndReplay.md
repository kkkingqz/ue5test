---
title: HeadlessParityAndReplay Archive Summary
status: archived
version: 1.0
updated: 2026-08-14
---

# HeadlessParityAndReplay: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Привести `gv2-headless` к двум ролям, зафиксированным в Headless Simulation Contract: parity gate и deterministic replay. План не расширяет роли host-а и не начинает balance/simulation-режим

**Результат:** Привести `gv2-headless` к двум ролям, зафиксированным в Headless Simulation Contract: parity gate и deterministic replay. План не расширяет роли host-а и не начинает balance/simulation-режим

## Этапы и задачи

### M1 — Conformance Consolidation

Каждая portable-проверка существует в одном экземпляре как entry point в `Testing/`-заголовке своего module. `gv2-headless --self-test` и Unreal automation исполняют один и тот же набор; host-локальных assertions о content-правилах не остаётся

- `HPR-01` — Зафиксировать инвентарь дублирования
- `HPR-02` — Определить форму conformance entry point
- `HPR-03` — Перенести value/diagnostic/build-result проверки
- `HPR-04` — Перенести JSON5-проверки
- `HPR-05` — Перенести schema/validation-проверки
- `HPR-06` — Ввести проверку отсутствия host-локальных копий
- `HPR-07` — Согласовать сообщение о расхождении

### M2 — Deterministic Replay

Прогон описывается run manifest и сводится в run digest. Одинаковый manifest даёт одинаковый digest в `gv2-headless` и в UE integration-тесте. Golden-манифесты хранятся как fixtures и проверяются CI

- `HPR-08` — Определить run manifest
- `HPR-09` — Определить run digest
- `HPR-10` — Выводить manifest и digest
- `HPR-11` — Реализовать replay записанного manifest
- `HPR-12` — Подтвердить digest в UE
- `HPR-13` — Завести golden-манифесты
- `HPR-14` — Синхронизировать документацию

## Актуальные нормативные источники

- [HeadlessSimulationContract](../../Architecture/HeadlessSimulationContract.md)
- [BuildAndTooling](../../Architecture/BuildAndTooling.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/HeadlessParityAndReplay) содержит исходные task-файлы, acceptance criteria и evidence.
