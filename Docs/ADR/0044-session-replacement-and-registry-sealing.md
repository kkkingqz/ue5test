---
title: "ADR-0044: Session Replacement and Registry Sealing"
status: accepted
date: 2026-09-12
---

# ADR-0044: Session Replacement and Registry Sealing

> **Решение:** замена сессии имеет две необратимо разделённые границы, а создание и sealing engine registries выполняет один private Lua lifecycle owner. До `commit-to-replace` сохраняется целая Ready-сессия A; после него A не восстанавливается, VM B создаётся одна и публикуется только вместе с успешной initial presentation.

## Context

Существующая документация одновременно требовала одну Lua VM, сохранение Ready-сессии A при отказе candidate B и создание новой candidate session до teardown A. Эти требования несовместимы: полноценная candidate session уже содержит вторую VM. Реализация дополнительно разделяла решение о teardown между coordinator и UE adapter, а initial presentation получала snapshot через ambient getter, выбирающий между active и candidate.

Registry lifecycle был описан как фиксированный порядок, но C++ вызывал отдельные Lua paths вручную. Такой список не является перечислителем фактически установленных registries: новый publisher может не попасть в sealing и остаться незамеченным до state build.

Нужно сохранить [критерий C++ scope](0020-cpp-scope-criterion.md), [opaque save boundary](0021-opaque-save-container.md) и [единый presentation snapshot](0043-presentation-apply-boundary.md), не создавая второй gameplay framework в native code.

## Decision

### D1 — Две границы replacement

До `commit-to-replace` разрешены только подготовка immutable native inputs/candidate authorities и read-only preflight в VM A. Ни runtime session B, ни VM B не создаются. Ошибка, supersede или отмена сохраняют VM, generation, snapshot, bindings и физическую UI-проекцию A.

`commit-to-replace` закрывает input A, инвалидирует bindings, уничтожает её проекцию, исполняет teardown и уничтожает VM A. Граница необратима. Только после неё создаются runtime session и VM B.

После полного bootstrap B initial document готовится с явно переданным candidate context. `publish-ready` одним commit публикует generation, snapshot, bindings, projection и `Ready`. Ошибка после `commit-to-replace` уничтожает B и переводит приложение в UE-native recovery; откат к уже уничтоженной A запрещён.

### D2 — Один owner перехода

`FGV2SessionCoordinator` владеет active session, native candidate и private move-only replacement token. Только owner routine может выполнить `commit-to-replace`, teardown проекции и `publish-ready`. UE adapter исполняет разрешённые coordinator-ом операции с UObject, но не принимает самостоятельного lifecycle-решения.

Initial document sink получает `FGV2PresentationPrepareContext` обязательным параметром. Ambient lookup, выбирающий active или candidate snapshot, запрещён. Public snapshot getter возвращает только snapshot текущей Ready-generation.

### D3 — Typed requests и terminal outcomes

Публичная lifecycle-поверхность состоит из typed session/save/load requests, cancellation и read-only status. Session request содержит закрытый mode `Menu | NewGame | LoadSave`, точную repository identity, seed и load revision/slot только для `LoadSave`. Terminal outcome — ровно `Completed | Failed | Cancelled | Superseded`; cancellation отдельно возвращает `Accepted | TooLate | Stale`.

Operation ID является непрозрачным значением. Private replacement token, Lua function names и callback references boundary не пересекают. Equivalent active request может join; один pending conflicting request имеет last-wins semantics; shutdown имеет приоритет.

### D4 — Один private Lua owner registry lifecycle

Закрытый engine descriptor одновременно перечисляет factory, façade slot, resolve/seal operation и frozen predicate каждого engine registry. `registry_lifecycle.install()` до module `register` создаёт registries из descriptor и устанавливает read-only façade slots. После hooks `registry_lifecycle.seal()` по тому же descriptor разрешает ссылки, выполняет contract order и требует `is_frozen() == true` у каждого участника.

Descriptor и privileged installation handle остаются lexical private внутри sealed bootstrap modules. Mods регистрируют только entries через опубликованные registry operations и не добавляют descriptor participants. C++ вызывает один fixed protected sealing entry point и получает только success либо typed fault с phase и registry path. Functions, registry objects и callbacks boundary не пересекают.

Module export freezing остаётся обязанностью loader и не включается в registry descriptor.

### D5 — State и presentation не становятся native authority

Temporary canonical state и composition policy остаются внутри Lua. Native lifecycle передаёт только typed scalar start inputs, repository handle, Lua sources и opaque save bytes. Resolved Screen Registry компилируется в независимое snapshot-owned value по ADR-0043 D1; это материализация принятого решения, не новый authority.

Off-tree prepared UI принадлежит верхней transaction до `Commit`/`Abort`; runtime UObject mutation выполняется только на Game Thread. Эти lifetime rules не дают coordinator права интерпретировать presentation payload.

## Consequences

- Нельзя обещать одновременно rollback к A и раннее уничтожение её VM/UI. До первой границы A цела; после неё существует только recovery.
- Native candidate может строиться рядом с A, потому что не содержит Lua VM и mutable gameplay-state.
- Initial и subsequent presentation используют один тип явного context; исчезает getter с выбором authority по скрытому состоянию.
- Фактическое множество registries выводится из composition descriptor, а не из тестового или C++-списка.
- Добавление нового request kind требует расширить закрытый enum и exhaustive dispatch; generic `CallLuaFunction` или строковый lifecycle router не допускаются.
- Headless использует тот же portable bootstrap/sealing и lifecycle results, но не строит UE presentation candidate.

## Rejected alternatives

- **Side-by-side VM A и B.** Нарушает инвариант одной VM и усложняет владение state, storage и operations.
- **Rollback к A после её teardown.** Требует сохранять либо реконструировать canonical state в C++, что нарушает ADR-0020/0021, и всё равно не восстанавливает точную VM identity.
- **Ambient snapshot getter.** Делает правильность initial presentation зависимой от порядка присваиваний, а не от типа входа.
- **Ручной C++ список `freeze()` paths.** Не перечисляет фактических publishers и дублирует Lua composition knowledge.
- **Расширяемый mods descriptor registries.** Превращает lifecycle protocol в generic registry-of-everything и позволяет обойти owner contracts конкретных registries.
