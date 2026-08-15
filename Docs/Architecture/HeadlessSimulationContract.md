---
title: Headless Simulation Contract
status: normative
version: 2.3
updated: 2026-08-15
depends_on:
  - LuaRuntimeContract.md
  - CommandsAndEvents.md
  - GameDataRepositoryContract.md
  - ../UI/PresentationSnapshotAndEffects.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0013-unified-text-pipeline.md
  - ../ADR/0024-lua-spec-runner.md
---

# Headless Simulation Contract

`gv2-headless` запускает authoritative Lua gameplay без Unreal Engine. Runner не является альтернативной gameplay implementation и не имеет собственных gameplay rules.

CLI-поверхность, exit codes и участие в CI описаны в [Build and Tooling](BuildAndTooling.md).

## Цели

Host выполняет ровно две зафиксированные роли.

**Parity gate.** Headless — быстрый UE-free гейт, доказывающий, что portable-поведение идентично UE. Каждая portable-проверка существует в одном экземпляре и исполняется обоими host-ами; расхождение результата является ошибкой сборки, а не расхождением тестов.

**Deterministic replay.** Headless воспроизводит записанную последовательность команд и подтверждает, что одинаковый вход даёт одинаковый результат. Это единственная роль, которую UE-host выполнить не может: воспроизведение не требует редактора, ассетов и presentation.

Всё, что не служит этим двум ролям, в host не добавляется.

### Границы

- Валидация контента принадлежит `gv2-content`. Headless не содержит собственных assertions о schema/envelope/reference-правилах и не является вторым content validator.
- Флаг `--check-scripts` является изолированным авторским tooling-режимом быстрой проверки корректности дерева `Scripts/` (манифест, граф зависимостей, синтаксис и экспорт модулей); он не относится ни к parity gate, ни к deterministic replay, не запускает геймплей и не расширяет runtime-ролей host-а.
- Presentation, localization и media остаются выключенными; host не приобретает UI-обязанностей.
- Числовые бюджеты производительности контрактом не задаются: измеримой нагрузки пока нет, а фиксировать пороги по заглушке запрещено.

### Направление

Balance/simulation-режим — scenario descriptors, policy/agent и Observation DTO — остаётся заявленным направлением, а не требованием. Он раскрывается отдельным планом после появления canonical state и Command/Event path; до этого момента настоящий contract не предъявляет к нему требований и не описывает его DTO.

## Ownership and boundaries

- `GV2RuntimeCore` владеет Lua VM, canonical state, Command Dispatcher и portable DTO. Host не меняет canonical state напрямую.
- Runtime session исполняется только на owner thread; одновременные и reentrant entry points запрещены.
- Host загружает тот же UTF-8 Lua module tree и manifest из `Scripts/`, что и UE adapter. Копия Lua-кода или per-module filename list внутри runner запрещены.
- Host строит repository тем же `BuildRepository()` path и закрепляет read handle до создания VM. Отсутствующий или невалидный repository завершает запуск до bootstrap.
- Источник команд находится вне gameplay VM и передаёт только value-only `CommandRequest`.

## Converging command flow

```text
UE Widget → binding validation → CommandRequest ┐
                                                 ├→ Command Dispatcher → validators → services → commit → EventBus
Headless driver → CommandRequest ───────────────┘
```

Direct command ingress не может вызывать handler или Gameplay Service в обход dispatcher. Command schema, phase gate, validators, mutation и post-commit events идентичны UE path.

## Conformance

Portable-проверка существует в одном экземпляре и исполняется обоими host-ами. Host-локальная копия запрещена: две независимые реализации одного правила расходятся молча и не ловятся CI.

Допустимы ровно две формы, и граница между ними определяется предметом проверки ([ADR-0024](../ADR/0024-lua-spec-runner.md)); третьей — новой host-локальной проверки любого рода — нет:

| Предмет проверки | Форма |
|---|---|
| Правило, целиком выраженное в Lua: state, mutation window, registries, world, domain objects, подписки | Lua-спека в `Tests/Lua/`; формат — в [Build and Tooling](BuildAndTooling.md) |
| C++ API и механизмы: сериализация manifest/digest, replay, marshaller, JSON5/schema-парсинг, session lifecycle, сам spec runner | C++ entry point в `Testing/`-заголовке своего module |

Новое правило, выраженное в Lua, запрещено проверять новым C++ entry point. Наборы, унаследованные до ADR-0024, перечислены в гейте явно и мигрируют по мере того, как их всё равно приходится изменять; список унаследованных не расширяется.

`--self-test` исполняет и C++ entry points, и все спеки. Unreal automation исполняет их же: тонкая обёртка вокруг C++ entry point допустима, собственные assertions о том же правиле — нет; спеки покрываются одним automation-тестом на под-дерево, а не тестом на спеку.

Расхождение результата между host-ами останавливает сборку. Отсутствие host-локальных копий, полнота покрытия обоими host-ами и запрет C++-проверок Lua-правил проверяются в CI скриптом `Tools/Content/validate_host_conformance_parity.py` (CTest `host_conformance_parity_contract`).

### Сессии спек

Спека исполняется внутри VM и обращается к `game.*` напрямую. Под-дерево `Tests/Lua/` определяет, на какой сессии оно исполняется:

- `Tests/Lua/world/` — продакшн-сессия с реальным `Scripts/bootstrap/manifest.lua` и реальным репозиторием. Кейс, которому нужно изменить state, обязан открыть mutation window явно.
- `Tests/Lua/commands/` — изолированная fixture-сессия: реестр валидаторов продакшн-сессии заморожен и пуст, поэтому проверки регистрации валидаторов и команд требуют собственной сессии.

Спеки исполняются на сессии, отдельной от той, что производит run digest: мутация состояния внутри спеки не должна попадать в наблюдаемый результат прогона.

## Run manifest и digest

Прогон описывается структурой `FRunManifest`:

```text
lua_release_num (int), repository_content_hash (64 hex), seed (uint64), accepted_commands [ command_id, sequence, args ]
```

Результат прогона описывается `FRunResult` (`bSuccess`, `ExecutedCommandsCount`, `FinalScreenId`, `FinalScreenFields`, `StateHash`, `FaultCode`) и сводится в `FRunDigest` — детерминированную каноническую SHA-256 свёртку наблюдаемого результата, включающую `state_hash` (хэш канонического состояния). Digest строго исключает тайминги, порядок завершения worker-ов, идентичность хоста, абсолютные пути файловой системы и локализованный текст.

`ReplayRunManifest` воспроизводит записанную в манифесте последовательность команд:
- Несовпадение `repository_content_hash` завершает прогон как configuration failure до создания Lua VM.
- Несовпадение `lua_release_num` завершает прогон как replay failure с typed fault `core:fault.run_manifest.lua_release_mismatch`; ни одна команда не исполняется.
- Все команды диспетчеризируются последовательно через единый `Command Dispatcher`.
- Одинаковый manifest даёт бит-в-бит идентичный digest в `gv2-headless` и в UE integration-тесте (`GV2.Runtime.Session.CrossHostDigestParity`).

Конкретные exit codes перечислены в [Build and Tooling](BuildAndTooling.md).

Manifest и digest выводятся в machine-readable JSON-строку в stdout (поля `state_hash`, `digest_hash` и объект `digest`), а также могут сохраняться в файлы через флаги `--output-manifest` и `--output-digest`. Эталонные golden-манифесты и дайджесты хранятся в `Tests/Fixtures/GoldenRuns/` и проверяются в CI.

## Resources and localization

- Headless package может не содержать image/audio/video payload.
- Metadata-only resource catalog сохраняет `resource_id`, kind, required/optional policy и availability для validation.
- Presentation snapshot и effects могут отбрасываться; gameplay facts от этого не меняются.
- `TextSpec` сохраняется как `text_id + args + optional style` без formatting.
- Gameplay запрещено читать resolved localized string или результат media loading как скрытое условие команды.

## Determinism

Одинаковые repository hash, package order, seed и последовательность команд обязаны давать одинаковый результат независимо от host-а и числа прогонов.

Localized diagnostic text не является machine-readable результатом.

## Performance model

- Presentation, localization formatting и media decoding выключены.
- Одна VM/session на process; batch выполняется несколькими processes.
- Runtime не выполняет per-frame UI/input work: следующий step начинается только после завершения предыдущего protected entry point.
- Оптимизация не может обходить Command Dispatcher или создавать отдельную simulator semantics.

## Failure semantics

Lua/runtime fault завершает run как runtime failure и сохраняет structured sanitized fault. Invalid command является обычным typed result и не завершает run. Отсутствие обязательной deterministic capability завершает run как configuration failure и не меняет gameplay silently.

## Acceptance criteria

- Standalone executable собирается и запускается без Unreal Engine libraries.
- UE и headless используют одну portable runtime implementation и exact Lua patch.
- UE и headless исполняют одинаковый manifest-driven `Scripts/` module graph; missing/unlisted source, hidden dependency или cycle завершает startup как configuration failure.
- Один corpus даёт одинаковый `repository_content_hash` в headless, CLI и Unreal automation.
- Каждая portable-проверка имеет ровно одну реализацию; host-локальная копия отсутствует.
- Расхождение результата conformance entry point между host-ами останавливает сборку.
- Одинаковый run manifest даёт одинаковый run digest в headless и UE; digest не зависит от таймингов и host-а.
- Прогон выводит manifest и digest в machine-readable виде.
- Headless не выполняет собственную content validation и не загружает media payload.
