---
title: Self-Contained Payload Tasks
status: active
version: 1.2
updated: 2026-09-07
depends_on:
  - README.md
  - Snapshot.md
  - ../../ADR/0040-universal-ui-property-pipeline.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# M3 — Self-Contained Payload

> **Материализует:** `PAH-R1`, `D3/D4` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md) и необходимую перед module extraction типовую границу.
> **Задачи:** PSC-09A…09B, PSC-10.
> **Результат:** Prepare и Apply больше не являются методами одного authority-aware объекта; нижний DTO содержит всё необходимое физическому применению.

## Почему этап идёт до module extraction

Сейчас `IGV2PropertyConsumer` совмещает `Prepare` и `Commit`, а `UGV2TextPipeline` — text/theme resolution и widget mutation. Такой тип нельзя перенести в lower module без обратной зависимости на `GV2`.

Сначала верхний слой формирует immutable transaction из resolved operations. На этом этапе physical Apply ещё может временно вызываться через adapters в `GV2`; окончательную гарантию даёт следующий milestone, когда реализация Apply физически переезжает в `GV2PresentationApply`.

## Задачи

- [ ] **PSC-09A — Типовая граница: контекст подготовки, каркас модуля и транзакция**
  - Зависимости: PSC-06, PSC-08.
  - Инвариант: объект, способный обратиться к snapshot/registry/Theme, не исполняет physical mutation; объект применения не имеет authority capability.
  - Не считается закрытием: перенос существующего `IGV2PropertyConsumer` целиком; callback из lower operation в upper resolver; `void*`/generic service locator; временная обратная module dependency; изменение `UCLASS` paths.
  - Done:
    - `FGV2PresentationPrepareContext` принадлежит верхнему `GV2` и является единственным типовым входом к snapshot resolution;
    - создаётся DTO-only каркас `Source/GV2PresentationApply/` с окончательным dependency allowlist; `GV2` зависит от него, обратное ребро отсутствует;
    - `FGV2PreparedPresentationTransaction` сразу объявлен в `GV2PresentationApply/Public`, состоит из immutable resolved operations и не включает PrepareContext;
    - `IGV2PropertyConsumer` разделён: upper preparers создают operations, lower applicator/operations выполняют Commit/Reset/Rollback;
    - public lower DTO использует только value/resolved UE types из разрешённого dependency set;
    - task не меняет ни одного `/Script/GV2` class path;
    - явно записано, какие виды операций ещё не проходят через транзакцию на конец задачи, — это состояние закрывает `PSC-09B`, и оно названо, а не подразумевается.
  - Evidence: `Source/GV2PresentationApply/GV2PresentationApply.Build.cs`, public prepared transaction/operation declarations, разделённые property interfaces, module graph gate.

- [ ] **PSC-09B — Весь production-путь проходит через транзакцию**
  - Зависимости: PSC-09A.
  - Разделение вынесено из `PSC-09A` не по размеру, а потому что у крупной задачи нет промежуточной точки, с которой видно, что пошло не так: `PSC-09A` устанавливает границу типов, `PSC-09B` заводит через неё всё. Та же причина, по которой разделены `PSC-11` и `PSC-12`.
  - Инвариант: транзакция является единственным протоколом между разрешением и применением; второго пути, минующего её, не существует ни для одного вида операции.
  - Не считается закрытием: перевод части видов при сохранении прежнего пути для остальных; `UGV2TextPipeline`, оставшийся вторым полным pipeline; дубли типов и compatibility alias вместо переноса; callback из нижней операции наверх.
  - Done:
    - `UGV2TextPipeline` разделён на upper text/theme preparation и lower widget application; прежний тип не остаётся вторым полным pipeline;
    - все authority-free value/interface types, необходимые lower DTO и будущим physical widget bases, перемещаются вниз либо заменяются одним canonical lower type; дубли и compatibility aliases не остаются;
    - keyed collections, nested screens и screen replacement выражены тем же transaction protocol, без callback в upper module;
    - существующий production path уже строит новую transaction; временный `GV2` adapter только делегирует её применение и не выполняет semantic lookup;
    - source-derived field inventory отвергает function/callback, PrepareContext, repository/package/registry/theme source pointers и generic service handles; у инвентаря есть отрицательный самотест;
    - множество видов операций, обязанных проходить через транзакцию, берётся обходом перечисления видов, а не списком в задаче;
    - task не меняет ни одного `/Script/GV2` class path.
  - Evidence: разделённые text interfaces, production initial-screen test, payload inventory/self-test, exhaustive kind walk.

- [ ] **PSC-10 — Замкнуть resolved payload для всех operation kinds**
  - Зависимости: PSC-09B.
  - Инвариант: если semantic ID/token был проверен в Prepare, Apply использует именно полученный resolved payload и не повторяет решение.
  - Не считается закрытием: покрытие только Text/Image; ID без соседнего payload; `TSoftObjectPtr` с поздним разыменованием; pointer на resolver/context; switch с `default` либо ручной список kinds.
  - Done:
    - operation kinds образуют один enum/variant; compiler exhaustive visitor без `default` отвергает новый kind без Apply semantics;
    - text operation несёт resolved localized text/markup, concrete style/renderer class и font/scale policy;
    - image/resource operation несёт loaded object/brush/render policy, а не только `resource_id`;
    - screen/nested operation несёт resolved loaded class и validated placement descriptor;
    - primitive, binding, collection, reset, rollback и projection-recovery operations содержат достаточные value payloads;
    - Stable IDs остаются только identity/diagnostic metadata рядом с resolved payload;
    - soft references, resolver/context pointers и callable callbacks отсутствуют во всех nested public payload fields; recursive declaration inventory имеет negative self-test;
    - viewport-dependent font/layout size вычисляется Apply как pure function от prepared policy и текущей geometry, без Theme/config lookup;
    - legacy runtime `GetConfiguredTheme()`/`GetConfiguredRegistry()` и эквивалентные configured accessors удалены после перевода всех operation kinds; settings/DataAssets остаются только candidate-builder inputs;
    - unresolvable value даёт typed Prepare failure и не создаёт partial transaction;
    - production test проходит каждый фактический operation kind; actual set выводится из enum/variant, expected behavior — из независимой classification table.
  - Evidence: prepared operation declarations, compiler exhaustive switch/visitor, recursive field inventory, Text/Image/Nested/Collection production tests.

## Проверка milestone

- [ ] Верхний слой только разрешает semantics и формирует transaction; lower-facing types не могут вызвать authority.
- [ ] Все operation kinds перечисляет enum/variant, а не задача или test list.
- [ ] Ни один payload не требует lookup/load при Apply.
- [ ] `UCLASS` paths ещё не менялись; production path работает через временный delegating adapter.
