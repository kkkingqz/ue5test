---
title: "ADR-0010: Portable Runtime and Headless Simulation"
status: accepted
date: 2026-08-10
---

# ADR-0010: Portable Runtime and Headless Simulation

> **Решение:** Portable gameplay runtime, UE/headless hosts и общий Command Dispatcher path.
> **Нормативный текст:** [Headless Simulation Contract](../Architecture/HeadlessSimulationContract.md).

## Context

Gameplay Lua должен исполняться без Unreal Engine для deterministic balance simulation. Текущий prototype VM host использует UE containers и Game Thread assertions. Такое связывание делает standalone runner невозможным и создаёт риск двух различных gameplay implementations.

Медиа, localization и physical input являются host concerns. При этом headless driver обязан проходить тот же Command Dispatcher, validators, Gameplay Services и EventBus, что и UE semantic input.

## Decision

- `GV2RuntimeCore` является portable C++/Lua library без UObject, UMG, `FText` и UE containers в public/runtime sources.
- Runtime session является owner-thread-only. UE назначает owner равным Game Thread; headless worker — своему thread.
- UE и `gv2-headless` используют одни исходники Lua runtime, module loader, marshaller, repository contracts и Command Dispatcher.
- UE semantic input после binding validation преобразуется в `CommandRequest`. Headless driver может подавать `CommandRequest` напрямую через test/simulation-only API. Оба пути сходятся до Command Dispatcher и не обходят gameplay validation/mutation rules.
- Physical media хранится host-ом. Lua и portable state содержат только `resource_id`; headless использует metadata-only resource catalog и не загружает media payload.
- Lua публикует `TextSpec`, а locale resolution принадлежит host localization adapter. Gameplay не ветвится по resolved text.
- Balance agent находится вне authoritative gameplay VM и работает через read-only Observation DTO → CommandRequest.
- Начальный parallelism — отдельные worker processes, одна session/VM на process. Multi-VM process требует отдельного решения после измерений.

## Consequences

- Gameplay и balance simulation используют один authoritative code path.
- Headless build не зависит от Unreal installation и может отключать presentation/localization/media work.
- UE adapter обязан конвертировать UE DTO в portable value model на границе.
- Resource/localization catalogs имеют общий logical source, но разные host-specific compiled outputs.
- Физическое разбиение и build pipeline усложняются: portable library собирается CMake и как dependency UE module.

## Rejected alternatives

- UnrealEditor `-nullrhi` как единственный headless режим: остаётся тяжёлая UE dependency и плохо масштабируется для balance batches.
- Отдельная упрощённая gameplay implementation для simulator: создаёт semantic drift.
- Имитировать Widget clicks в simulator: связывает balance driver с UI document revision и binding handles.
- Выполнять agent policy внутри authoritative VM: policy получает возможность случайно нарушить state mutation boundary.
