---
title: Headless Parity and Replay Implementation Plan
status: archived
version: 2.0
updated: 2026-08-14
depends_on:
  - ../../../Architecture/HeadlessSimulationContract.md
  - ../../../Architecture/BuildAndTooling.md
---

# План реализации Headless Parity and Replay

> **Архив.** План выполнен полностью (M1–M2) и больше не является источником задач. Нормативное поведение перенесено в [Headless Simulation Contract](../../../Architecture/HeadlessSimulationContract.md) и [Build and Tooling](../../../Architecture/BuildAndTooling.md). Документ сохраняется как implementation record.

## Цель

Привести `gv2-headless` к двум ролям, зафиксированным в [Headless Simulation Contract](../../../Architecture/HeadlessSimulationContract.md): parity gate и deterministic replay. План не расширяет роли host-а и не начинает balance/simulation-режим.

## Исходное состояние

Из ~1670 строк `Headless/Source/main.cpp` около 650 — host-локальные self-тесты ContentCore (`RunContentCoreJson5ParserSelfTest`, `...SchemaRegistry...`, `...ScalarValidation...` и ещё 11). У каждого есть одноимённый UE automation-тест в `Source/GV2/Private/Tests/`, то есть одно правило проверяется двумя независимыми реализациями. Расхождение между ними CI поймать не может.

Общими conformance entry points сегодня исполняются только пять проверок: Stable ID, JSON5 fixtures, representative core, marshaller и `game.repository`.

Прогон выводит JSON-строку с `repository_content_hash`, числом команд и seed, но не имеет ни manifest, ни digest: воспроизводимость нельзя ни зафиксировать, ни сравнить между host-ами.

## Границы

Входят:

- перенос host-локальных проверок в shared conformance entry points;
- run manifest и run digest, одинаковые в headless и UE;
- golden-манифесты как fixtures и их проверка в CI.

Не входят:

- scenario descriptor, policy/agent, Observation DTO;
- числовые бюджеты производительности;
- любая content validation внутри headless — она принадлежит `gv2-content`;
- canonical state, commands и events как таковые: план работает с тем, что уже существует, и расширяется автоматически по мере появления gameplay.

## Milestones

- [x] M1 — [Conformance Consolidation](ConformanceConsolidation.md): одна реализация на каждую portable-проверку.
- [x] M2 — [Deterministic Replay](DeterministicReplay.md): run manifest, digest и golden-прогоны.

## Критический путь

```text
Conformance Consolidation
→ Deterministic Replay
```

M1 самодостаточен и закрывает существующий дефект. M2 опирается на него: digest сравнивается между host-ами тем же способом, что и результаты conformance entry points.

## Общие правила выполнения

1. Правило проверяется одной реализацией; host-локальная копия является дефектом.
2. Перенос проверки не меняет её содержание: сначала перенос без изменения поведения, отдельно — расширение покрытия.
3. Digest и manifest не содержат таймингов, абсолютных путей, localized текста и host-специфичных значений.
4. Каждая задача добавляет negative case, если меняет failure semantics.
5. Новое observable behavior синхронно отражается в `HeadlessSimulationContract` или `BuildAndTooling`.

## Итоговый Definition of Done

- [x] Ни одно правило не проверяется дважды независимыми реализациями.
- [x] `--self-test` и Unreal automation исполняют один и тот же набор entry points.
- [x] Одинаковый manifest даёт одинаковый digest в обоих host-ах.
- [x] Golden-прогон проверяется CI и ломается при изменении наблюдаемого результата.
- [x] Контракты соответствуют фактическому поведению host-а.
