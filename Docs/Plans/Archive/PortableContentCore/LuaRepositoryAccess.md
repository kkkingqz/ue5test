---
title: Portable Content Core Lua Repository Access Tasks
status: archived
version: 1.0
updated: 2026-08-14
depends_on:
  - CliAndHostIntegration.md
  - ../../../Architecture/LuaRuntimeContract.md
  - ../../../Architecture/GameDataRepositoryContract.md
decisions:
  - ../../../ADR/0005-value-only-async-boundary.md
  - ../../../ADR/0006-repository-reload-and-session-pinning.md
  - ../../../ADR/0007-lua-module-environment.md
  - ../../../ADR/0018-portable-content-core-module.md
---

# M6 — Lua Repository Access

## Результат этапа

Lua читает definitions из pinned immutable snapshot через `game.repository`. Один marshaller обслуживает оба portable value-типа. Headless и UE получают одинаковые значения, одинаковый порядок и одинаковые typed errors из одного corpus.

Этап не расширяет scope [Portable Content Core Proposal](../../../Proposals/PortableContentCoreProposal.md): его ownership-диаграмма уже содержит путь `Immutable Repository Snapshot → GV2RuntimeCore / Lua queries`, а раздел ownership фиксирует, что `GV2RuntimeCore` читает pinned snapshot через typed read interface и не участвует в repository build. `game.repository` также уже нормативно объявлен в `LuaRuntimeContract` (`game` façade) и `GameDataRepositoryContract` (`Lua API`). M6 закрывает разрыв между этими нормативными описаниями и реализацией: до него pinned handle доходит до session coordinator и headless, но gameplay не может прочитать ни одного definition.

## Принятые решения

Решения приняты до начала этапа и не переоткрываются внутри задач.

### Единый marshaller

`LuaRuntimeContract` нормативно называет компонент `FGV2LuaMarshaller` и перечисляет его обязанности, но в коде существуют только свободные функции `PushValue`/`PushObject`/`ReadFlatScalarObject` внутри `GV2RuntimeSession.cpp`. Добавление второго push path для `GV2ContentCore::FValue` закрепило бы это расхождение и создало бы два места, где определяются `game.null`, absent-семантика, deep copy и value limits.

Решение: выделить настоящий `FGV2LuaMarshaller` и провести через него оба value-типа. Contract исправляется в ту же задачу, если фактическая структура отличается от описанной.

### Provenance не экспонируется в Lua

`PortableContentCoreProposal` включает `GetProvenance` в минимальный C++ API, но `GameDataRepositoryContract` в разделе `Lua API` его не объявляет. Решение — оставить так и зафиксировать причину.

Provenance содержит package-relative source paths, spans, schema identity и ordered shadowed providers. Это authoring-метаданные, и их публикация в Lua:

- отдаёт за trust boundary раскладку пакетов и сам факт наличия shadowed entries;
- создаёт второй diagnostics surface рядом с `gv2-content inspect`, который уже является каноническим authoring-путём;
- позволяет gameplay ветвиться по тому, какой package выиграл override, что противоречит full-override семантике: winner обязан быть неотличим от исходного definition.

Lua surface остаётся ровно четырьмя функциями. Provenance добавляется только после конкретного gameplay-сценария, отдельной задачей и с синхронным обновлением contract.

## Задачи

- [x] **PCC-39 — Выделить `FGV2LuaMarshaller`**
  - Зависимости: PCC-30.
  - Перенести существующие `PushValue`/`PushObject`/`ReadFlatScalarObject` из `GV2RuntimeSession.cpp` в отдельный marshaller и провести через него как `GV2RuntimeCore::FValue`, так и `GV2ContentCore::FValue`.
  - Done: один push path обслуживает оба типа; `game.null`, absent-семантика, deep copy, depth/node limits и отказ от non-finite numbers определены в одном месте; canonical key order объектов сохраняется; `LuaRuntimeContract` раздел `C++ marshalling` соответствует фактическому имени и структуре; параллельный marshalling path отсутствует.
  - Evidence: `FGV2LuaMarshaller` выделен в `GV2RuntimeCore`, покрыт conformance-тестом `GV2.Runtime.Lua.MarshallerConformance` и self-test headless.

- [x] **PCC-40 — Согласовать value limits parser и marshaller**
  - Зависимости: PCC-39.
  - `MaxValueDepth`/`MaxValueNodes` рантайма и `FParseLimits::MaxNestingDepth` сейчас расходятся, поэтому валидный по schema definition может не пересечь boundary.
  - Done: выбран один canonical предел (`MaxNestingDepth = 64`, `MaxContainerEntries = 10000`, `MaxNodes = 10000`); definition, прошедший build, гарантированно пересекает boundary; превышения лимитов отклоняются typed diagnostics на build stage (`core:diagnostic.json5.limit.nesting_depth`, `core:diagnostic.schema.limit.node_count`); negative fixture `invalid/nesting_depth_exceeded` подтверждает выбранное поведение.
  - Evidence: Fixture `invalid/nesting_depth_exceeded`, тест `GV2.Runtime.ContentCore.ParseLimits` и `pcc_shared_fixture_contract`.

- [x] **PCC-41 — Передать pinned read handle в `FRuntimeSession`**
  - Зависимости: PCC-30, PCC-36.
  - `Start(...)` принимает `FRepositoryReadHandle` обязательным параметром без default value; session хранит копию весь свой lifetime.
  - Done: невалидный handle даёт typed fault до создания Lua VM (`RepositoryNotReady`); `Stop()` освобождает handle; активная session не переключает snapshot при последующем Application publish; headless и UE используют один и тот же путь передачи.
  - Evidence: `FRuntimeSession::Start` / `GetPinnedRepository`, тест `GV2.Runtime.Session.PinnedHandleLifetime` и CTest тесты `gv2_headless_*`.

- [x] **PCC-42 — Реализовать `game.repository` query API**
  - Зависимости: PCC-39, PCC-41.
  - Реализовать `get`, `require`, `list`, `exists` поверх `Find`/`Require`/`List`; таблица `game.repository` закрыта от подмены и расширения.
  - Done: каждый query возвращает detached deep copy; identity таблиц между вызовами не гарантируется; query не выполняет I/O, parsing или hash; redirect source разрешается в final active definition; alias `game.data` отсутствует; generic `query(index_id, key)` не добавлен.
  - Evidence: Реализация `game.repository` (`get`, `require`, `list`, `exists`) в `GV2RuntimeSession.cpp`, automation-тест `GV2.Runtime.Lua.RepositoryAccess`.

- [x] **PCC-43 — Зафиксировать error convention repository API**
  - Зависимости: PCC-42.
  - `get` не выбрасывает Lua error никогда и возвращает `nil` плюс typed table с `code`; `require` выбрасывает всегда, стабильный code является первым токеном сообщения.
  - Done: `not_found`, `tombstoned`, `invalid_handle` и невалидная грамматика ID (`invalid_id`) различимы по code; machine logic не разбирает human-readable message; convention описана в `LuaRuntimeContract` и согласована с разделом `Lua API` в `GameDataRepositoryContract`.
  - Evidence: Реализация кодов ошибок в `RepositoryGet`/`RepositoryRequire` в `GV2RuntimeSession.cpp`, тесты в `GV2.Runtime.Lua.RepositoryAccess`.

- [x] **PCC-44 — Зафиксировать canonical order `list`**
  - Зависимости: PCC-42.
  - `ByKind` наполняется в порядке `ResolvedDefinitions`, тогда как contract требует canonical byte order.
  - Done: порядок гарантирован явно — сортировкой в snapshot либо доказанным и проверенным инвариантом builder-а; тест использует несколько IDs одного kind; неизвестный или невалидный kind возвращает пустой список, а не ошибку.
  - Evidence: Явная сортировка `ByKind` в `RepositoryBuilder.cpp`, тесты канонического порядка и пустых списков в `GV2.Runtime.Lua.RepositoryAccess`.

- [x] **PCC-45 — Зафиксировать отсутствие provenance в Lua surface**
  - Зависимости: PCC-42.
  - Записать решение и его причины в `LuaRuntimeContract` и `GameDataRepositoryContract`.
  - Done: `game.repository` содержит ровно четыре функции; тест подтверждает отсутствие provenance, package identity, absolute и package-relative source paths в возвращаемых Lua значениях; authoring-путь остаётся за `gv2-content inspect`.
  - Evidence: Разделы об изоляции provenance в `LuaRuntimeContract.md` и `GameDataRepositoryContract.md`, тесты отсутствия полей provenance и наличия ровно 4 функций `game.repository` в `GV2.Runtime.Lua.RepositoryAccess`.

- [x] **PCC-46 — Добавить cross-host тесты Lua repository access**
  - Зависимости: PCC-41–PCC-45.
  - Один corpus исполняется `gv2-headless --self-test` и Unreal automation.
  - Done: покрыты happy path `get`/`require`/`exists`/`list`; unknown ID, tombstoned ID и redirect source; невалидная грамматика ID; мутация возвращённой таблицы не влияет на повторный query; чтение внутри активной session переживает unrelated republish; case на выбранный value limit.
  - Evidence: `RunLuaRepositoryAccessConformance()` в `GV2LuaRepositoryConformance.cpp`, self-test в `gv2-headless`, automation-тест `GV2.Runtime.Lua.RepositoryConformanceCrossHost`.

- [x] **PCC-47 — Подключить первый потребитель API**
  - Зависимости: PCC-46.
  - Модуль в `Scripts/` читает definition из repository и доводит значение до Screen Field существующим presentation path.
  - Done: цепочка `GameData/core` → snapshot → pinned handle → Lua → Screen Field → Screen Template подтверждена automation-тестом; отдельный test-only runtime API не добавлен; gameplay-state, commands и events не затронуты.
  - Evidence: `Scripts/debug/start.lua` обращается к `game.repository.get("core:item.weapon.iron_sword")`; `GV2.Runtime.Lua.SafeEnvironmentAndProtectedEntry` (`FGV2PortableRuntimeTest`, section "PCC-47: Verify definition read from repository reaches Screen Field") подтверждает, что значение `target`/`value` в `buttons`/`player_name` Screen Fields пришло из repository definition, а `GV2.Runtime.Presentation.LuaCreatesRegisteredScreen` подтверждает создание зарегистрированного экрана из этого запроса.

## Не входит в этап

- `game.state`, `game.commands`, `game.events`, `game.instances` и остальные сервисы фасада;
- save/load, миграции и canonical gameplay-state;
- provenance, generic index query и любой authoring API в Lua;
- async candidate build, parallel workers и cancellable operation token;
- mod discovery, load order и multi-package corpus.

## Проверка milestone

- [x] Lua читает definitions из pinned snapshot без host-specific кода в `GV2` и `gv2-headless`.
- [x] Один marshaller обслуживает оба portable value-типа; второй push path отсутствует.
- [x] Error convention одинакова в обоих host-ах и проверяется по stable code, а не по message.
- [x] Активная session продолжает видеть свой snapshot после unrelated republish.
- [x] `LuaRuntimeContract` и `GameDataRepositoryContract` соответствуют фактическому Lua API.
