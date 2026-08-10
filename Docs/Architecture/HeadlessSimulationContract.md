---
title: Headless Simulation Contract
status: normative
version: 1.0
updated: 2026-08-10
depends_on:
  - LuaRuntimeContract.md
  - CommandsAndEvents.md
  - GameDataRepositoryContract.md
  - ../UI/PresentationSnapshotAndEffects.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
---

# Headless Simulation Contract

`gv2-headless` запускает authoritative Lua gameplay без Unreal Engine. Цель — deterministic scenario tests, balance batches и performance measurement; runner не является альтернативной gameplay implementation.

## Ownership and boundaries

- `GV2RuntimeCore` владеет Lua VM, canonical state, Command Dispatcher, Gameplay Services, EventBus, deterministic clock/PRNG и portable DTO.
- Runtime session исполняется только на owner thread. Одновременные/reentrant entry points запрещены.
- UE adapter и headless host не меняют canonical state напрямую.
- Agent/policy находится вне gameplay VM и получает только read-only Observation DTO.
- Headless direct command ingress доступен только simulation/test host-у.

## Converging command flow

```text
UE Widget → binding validation → CommandRequest ┐
                                                 ├→ Command Dispatcher → validators → services → commit → EventBus
Headless Agent → CommandRequest ────────────────┘
```

Direct command ingress не может вызывать handler или Gameplay Service в обход dispatcher. Command schema, phase gate, validators, mutation и post-commit events идентичны UE path.

## Scenario descriptor

Production descriptor обязан содержать:

```json5
{
  scenario_id: "core:scenario.balance.market_route",
  repository_hash: "...",
  start: { mode: "NewGame" },
  seed: 918273,
  max_steps: 10000,
  policy_id: "core:policy.balance.random_valid_command",
  locale: null,
}
```

`locale` optional и не влияет на gameplay result. Initial vertical slice может принимать CLI seed/step count до появления scenario repository schema.

## Observation and agent

Observation DTO содержит только schema-defined gameplay values/IDs. Functions, Lua tables by identity, repository pointers, UI handles и UObject запрещены.

Agent возвращает `CommandRequest`:

```json5
{
  command_id: "core:command.location.travel",
  args: { target_location_id: "core:location.city.market" },
}
```

Invalid agent output получает typed rejection и не меняет state. Policy randomness имеет отдельный recorded seed и не использует gameplay PRNG скрыто.

## Resources and localization

- Headless package может не содержать image/audio/video payload.
- Metadata-only resource catalog сохраняет `resource_id`, kind, required/optional policy и availability для validation.
- Presentation snapshot/effects могут отбрасываться либо поступать metrics collector-у; gameplay facts от этого не меняются.
- `TextSpec` по умолчанию сохраняется как `text_id + args` без formatting. `--locale` подключает portable localization catalog только для localization/report tests.
- Gameplay запрещено читать resolved localized string или результат media loading как скрытое условие команды. Mandatory resource prepare моделируется deterministic TechnicalInput согласно scenario capability profile.

## Determinism and reports

Каждый run фиксирует scenario ID, exact Lua release, repository hash, package order, seed/PRNG state, initial save identity, gameplay clock и ordered accepted commands.

Result report содержит stable outcome fields и metrics; localized diagnostic text не является machine-readable result.

## Performance model

- Presentation, localization formatting и media decoding выключены по умолчанию.
- Одна VM/session на worker process. Batch orchestrator запускает несколько processes.
- Runtime не выполняет per-frame UI/input work. Driver вызывает следующий step только после завершения предыдущего protected entry point.
- Benchmark измеряет commands/sec, p50/p95 entry duration, allocations/command, GC duration, state size, repository queries и events/command.
- Оптимизация не может обходить Command Dispatcher или создавать отдельную simulator semantics.

## Failure semantics

Lua/runtime fault завершает run как `runtime_fault` и сохраняет structured sanitized fault. Invalid command является обычным typed result. Missing required deterministic capability завершает scenario как configuration failure, а не меняет gameplay silently.

## Acceptance criteria

- Standalone executable собирается и запускается без Unreal Engine libraries.
- UE и headless используют одну portable runtime implementation и exact Lua patch.
- Один recorded CommandRequest sequence даёт одинаковые state/events в UE integration test и headless conformance test.
- Headless не загружает media payload и не требует locale для balance run.
- Parallel process runs с одинаковым input дают одинаковые reports за исключением timing metrics.
