---
title: Presentation Authority Structural Closure Proposal
status: draft
proposal_state: accepted_for_planning
version: 0.1
updated: 2026-09-07
depends_on:
  - ../Status/AuditFindings.md
  - ../Architecture/SystemContextAndComponents.md
  - ../Architecture/BootstrapAndSessionLifecycle.md
  - ../Architecture/HeadlessSimulationContract.md
  - ../UI/UIDocumentAndReconciliation.md
decisions:
  - ../ADR/0040-universal-ui-property-pipeline.md
  - ../ADR/0041-ui-commit-rollback-model.md
  - ../ADR/0042-presentation-authority-and-publication.md
---

# Структурное замыкание авторитета презентации

> **Предлагает:** заменить согласованные по lifecycle, но независимые presentation authorities одним session-owned snapshot и отделить разрешение семантики от применения физической проекции границей Unreal-модуля.
> **Не предлагает:** второго gameplay runtime, presentation в Headless, schema-specific UI paths или универсального фреймворка компиляции экранов.
> **Трудоёмкость:** L — один новый Unreal module, единый session candidate и миграция существующего UI apply path без изменения Lua authoring surface.

## Проблема

Последовательность закрытых планов показывает один повторяющийся класс дефектов:

```text
UniversalUiPropertyPipeline
→ PipelineClosureCorrection
→ GenericBoundaryHardening
→ GenericUiTransactionFollowUp
→ PresentationAuthorityHardening
→ повторное ревью Presentation Authority
```

Каждый раунд исправлял реальные ошибки и оставлял зелёные тесты. Следующий раунд находил ту же форму в месте, отсутствовавшем в ручном перечне: неполный набор capability, неучтённую rollback boundary, более узкий test filter или новый authority accessor.

Актуальный production-код подтверждает семь проявлений:

- Theme и style повторно разрешаются из `Commit`, вплоть до `LoadSynchronous()`;
- принятого coordinator-owned `FSessionContentSnapshot` нет, а schemas, images, screens и Theme принадлежат разным объектам и globals;
- Screen Registry повторно открывает canonical package closure вместо exact package set сессии;
- nested Tabs получают configured Registry напрямую и имеют fallback класса без обязательного `Resolve`;
- image catalog читает и декодирует resources отключённых пакетов до фильтрации;
- `ue_content_roots` меняет runtime authorization, но не package fingerprint;
- authority/discovery gates перечисляют известные API вручную и проходят зелёными при достижимом `Commit → GetConfiguredTheme() → LoadSynchronous()`.

Корень не в недостатке очередного regex. Инварианты выражены дисциплиной вызывающего поверх открытого графа зависимостей. Проверка видит только уже известные названия, поэтому добавление нового способа получить данные расширяет production surface раньше, чем расширяет перечислитель.

## Цель

После изменения должны одновременно выполняться четыре свойства:

1. Exact package set выводится один раз и является входом repository, Lua и UE presentation build.
2. У активной сессии ровно один immutable content snapshot; второй runtime authority невозможно получить через global/settings accessor.
3. `Commit` физически не зависит от discovery, settings и semantic resolvers; prepared transaction несёт все принятые решения.
4. Headless сохраняет текущие роли и не линкует Unreal или presentation.

## Соответствие findings и механизмов

| Finding | Закрывающий механизм |
|---|---|
| `PAH-R1` | Theme входит в snapshot; Prepare сохраняет resolved styles/renderers, Apply не видит Theme source |
| `PAH-R2` | Реальный `FGV2SessionContentSnapshot` принадлежит coordinator и публикуется атомарно |
| `PAH-R3` | Screen Registry строится из единственного `FResolvedPackageSet` сессии |
| `PAH-R4` | Nested screen разрешается только через `PrepareContext.ResolveScreen`; fallback класса отсутствует |
| `PAH-R5` | Resource builder обходит только roots пакетов exact closure до чтения и decode |
| `PAH-R6` | Canonical hash полного manifest входит в package fingerprint без знания UE-полей portable-слоем |
| `PAH-R7` | Apply вынесен за module/type boundary; ручной список authority accessors перестаёт быть первичной гарантией |

## Рассмотренные варианты

### Локальные исправления и расширение regex

Исправляет текущие семь findings с минимальным diff, но сохраняет ручной actual-set. Следующий accessor или indirect helper снова может дать false green. Вариант отклонён как повторение уже измеренной стратегии.

### Snapshot и phase-context внутри `GV2`

Устраняет globals и улучшает ownership. Однако `Commit` остаётся в модуле, которому доступны settings, loaders и authority types. Инвариант по-прежнему держится API-дисциплиной и source gate. Вариант полезен как промежуточное состояние миграции, но недостаточен как конечная архитектура.

### Snapshot плюс физическая граница применения

Один snapshot закрывает ownership, а нижележащий `GV2PresentationApply` делает запрещённое направление зависимостей ошибкой сборки. Это выбранный вариант. Он дороже один раз, но уменьшает число механизмов: globals и параллельные builders удаляются, а существующий Prepare/Commit сохраняется как единственный pipeline.

## Целевая архитектура

```text
GV2ContentHostSupport
    ResolvePackageSet()
             │
             ├──────────────► gv2-headless
             │                    ├─ repository
             │                    └─ Lua runtime
             │
             ▼
GV2 / Application
    build repository + loaded Lua sources + presentation candidate
             │
             ▼
FGV2SessionContentSnapshot (immutable, coordinator-owned)
             │
             ▼
Prepare(snapshot, desired document)
             │
             ▼
FGV2PreparedPresentationTransaction
             │
             ▼
GV2PresentationApply::Commit(transaction, live UMG tree)
```

`GV2` зависит от `GV2PresentationApply`. Обратная зависимость запрещена. Headless зависит только от portable modules и не видит оба UE-specific типа.

## Единый resolved package set

`GV2ContentHostSupport` получает portable value:

```text
FResolvedPackageSet
└─ ordered FResolvedPackageSource[]
   ├─ package root
   ├─ immutable FPackageDescriptor
   └─ canonical manifest hash
```

Repository builder получает descriptors, Lua loader — те же ordered roots, UE session builder — тот же набор целиком. Повторный `DiscoverFromGameData()` downstream запрещён. Editor override, automation fixture и production lock различаются только способом построения `FResolvedPackageSet`, а не отдельными ветками потребителей.

Headless использует тот же portable тип для repository и Lua sources. Это не добавляет ему UI-обязанностей и не меняет deterministic gameplay semantics.

## Package fingerprint и Headless

Portable parser вычисляет canonical hash полного `package.json5`, не интерпретируя host-specific fields. `ComputePackageFingerprint` учитывает этот hash вместе с portable descriptor. Поэтому `ue_content_roots` и будущие host extensions меняют package fingerprint автоматически, но не протаскивают UE-типы в `GV2ContentCore`.

Разделяются три identity:

| Identity | Содержимое | Потребитель |
|---|---|---|
| `repository_content_hash` | definitions, schemas, overrides, provenance | Lua, UE, Headless, replay |
| `package_fingerprint` | portable descriptor плюс полный canonical manifest | package lock в обоих host-ах |
| `presentation_hash` | resolved screens, resources, Theme и UE asset identities | только UE session snapshot |

`session_content_id` объединяет repository/package/presentation identities для UE diagnostics и session replacement. Headless run digest сохраняет только portable составляющие и не меняется от visual assets.

## Session content snapshot

`FGV2SessionContentSnapshot` — immutable C++ object, не `UDataAsset` и не копия repository definitions:

```text
FGV2SessionContentSnapshot
├─ FRepositoryReadHandle
├─ ordered package identities
├─ loaded Lua source set + script set hash
├─ eagerly compiled UI schema set
├─ FGV2ResolvedScreenRegistry
├─ FGV2ResolvedImageCatalog
├─ FGV2ResolvedUiTheme
├─ resolved GameShell class
├─ GC-safe asset pin set
└─ session_content_id
```

Snapshot агрегирует authorities. Definitions и provenance остаются только в repository handle. Absolute package roots используются builder-ом и не сохраняются как runtime API.

Source assets сохраняют authoring-роли:

- `UGV2ScreenRegistry` — ввод для bootstrap builder, не runtime registry;
- `UGV2UiTheme` — ввод для разрешения Theme, не runtime service;
- settings выбирают bootstrap inputs до сборки candidate, но недоступны presentation runtime.

Resolved structures не имеют mutating/build/load methods. Объекты Unreal удерживаются отдельным GC-safe pin set на lifetime snapshot.

UI schemas обнаруживаются, разбираются и компилируются полностью при candidate build. Invalid schema даёт typed bootstrap failure; неизвестная схема после `Ready` не запускает filesystem fallback.

## Lifecycle и публикация

```text
Resolve exact package set
→ build repository candidate
→ load Lua sources
→ compile all UI schemas
→ build Screen Registry from the same package set
→ scan resources only inside enabled package roots
→ resolve Theme, styles, renderer classes and GameShell
→ validate cross-component references
→ freeze session snapshot
→ start Lua VM from snapshot repository/sources
→ Prepare initial presentation
→ Commit initial presentation
→ publish Ready
```

Все шаги до `Ready` работают с private candidate. `FGV2SessionCoordinator` становится единственным владельцем опубликованного `TSharedPtr<const FGV2SessionContentSnapshot>`; `UGV2RuntimeSubsystem` хранит только coordinator и физическую проекцию.

Ошибка любого builder-а уничтожает candidate целиком. Предыдущая active session не меняется. Cold start показывает минимальную UE-native recovery surface, которая не читает configured Theme и не требует snapshot. Catastrophic recovery активной сессии использует её snapshot и `LastCommittedDocument`, выполняя обычный свежий Prepare.

Active snapshot и snapshot replacement candidate в одном process имеют независимые lifetimes. Освобождение candidate либо прежней active session не затрагивает второй snapshot. Проверка lifetime не обязана одновременно запускать две gameplay VM и не ослабляет one-VM lifecycle.

## Prepare и самодостаточная транзакция

`Prepare` — единственная фаза, получающая `FGV2PresentationPrepareContext`, а через него snapshot. Она разрешает:

- `screen_id + Placement` → loaded Widget class;
- `resource_id` → resolved brush/render policy;
- `text_id/style token` → text, normalized markup и resolved style policy;
- Theme tokens → concrete CommonUI/Slate styles и renderer classes;
- schema IDs → compiled schemas;
- package ownership → уже проверенный descriptor.

`FGV2PreparedPresentationTransaction` может хранить Stable ID только как identity/diagnostic metadata. ID, требующий lookup при применении, запрещён: рядом обязан находиться resolved payload.

Viewport-dependent размер остаётся физическим вычислением. Prepare сохраняет разрешённую font/scale policy, а Apply вычисляет итоговый размер из неё и текущей геометрии без обращения к Theme.

Nested Tabs используют тот же `PrepareContext.ResolveScreen(screen_id, Embedded)`, что и top-level screen. Отсутствие resolver или descriptor — typed Prepare failure; generic base-class fallback удаляется.

## Модуль `GV2PresentationApply`

Модуль содержит только:

- resolved presentation DTO и prepared mutation operations;
- физические UMG/CommonUI widget bases;
- Commit, rollback, keyed reconciliation и восстановление физической проекции;
- чистые layout/viewport calculations, не требующие content authority.

Он может зависеть от `Core`, `CoreUObject`, `Engine`, `UMG`, `CommonUI`, `Slate` и `SlateCore`. Запрещены зависимости на `GV2`, `GV2ContentHostSupport`, `DeveloperSettings`, `AssetRegistry`, `ImageCore` и filesystem/content authoring modules.

В модуле отсутствуют settings objects, `TSoftObjectPtr` как вход runtime-операции, file/config discovery и semantic registries. Он получает loaded objects, value styles и prepared operations от верхнего модуля.

Перенос существующих `UCLASS` меняет `/Script/GV2` paths. Миграция обязана использовать временные Core Redirects, загрузить и пересохранить все затронутые Widget Blueprints через Unreal Editor API, доказать отсутствие ссылок на старые class paths и удалить временные redirects до завершения плана. Постоянный compatibility layer не сохраняется.

Это не второй UI framework: текущий Universal UI Property Pipeline остаётся единственным. Меняется расположение boundary и полнота prepared payload, а не authoring grammar или widget capability model.

## Проверка архитектуры

Главная гарантия обеспечивается тремя независимыми уровнями.

### Компилятор и build graph

- `GV2PresentationApply.Build.cs` не может зависеть от authority/bootstrap modules.
- Скрипт выводит actual module graph из всех `*.Build.cs` и отвергает обратное ребро; список production files не задаётся руками.
- Все `Commit*` roots выводятся из production source и обязаны быть реализованы в `GV2PresentationApply`; верхний модуль только передаёт готовую транзакцию единственному public apply entry point.

### Замкнутые типы

- Authority resolution доступен только через `FGV2PresentationPrepareContext`; global accessor отсутствует.
- Prepared operation kinds образуют один exhaustive variant/enum; новый kind без применения является compile error.
- Apply module не имеет типа repository, package roots, registry source, Theme source или prepare context, поэтому prepared operation не может сохранить такой handle.
- Source-derived inventory public prepared payload fields отвергает soft references и resolver/context pointers.

### Production-path scenarios

Обязательные negative/integration cases:

1. `Commit → GetConfiguredTheme()` не компилируется через project dependency boundary и отвергается module gate.
2. `Commit → LoadSynchronous()` отвергается apply-module gate.
3. Nested Tab без разрешённого Registry descriptor даёт Prepare failure.
4. Editor package roots отличаются от canonical lock; repository, Lua, schemas, screens и resources используют exact Editor set.
5. Повреждённый resource отключённого пакета не читается и не мешает старту активной сессии.
6. Active snapshot и replacement candidate не перезаписывают и не освобождают authorities друг друга; тест не запускает вторую gameplay VM.
7. Изменение `ue_content_roots` меняет package fingerprint; UE-specific asset change меняет presentation hash, но не headless run digest.
8. Runtime authority counter показывает обращения во время Prepare и ноль обращений во время Commit.
9. Полный UE production path строит initial screen из snapshot, а не только тестовая вспомогательная функция.

Каждый structural gate имеет исполняемый negative self-test. Число UE automation tests берётся из машинного отчёта полного `Automation RunTests GV2`, не из префиксного поднабора или строк лога.

## Порядок реализации

1. Принять ADR о physical dependency direction, составе snapshot и изменении прежнего вывода о достаточности фазового гейта.
2. Ввести `FResolvedPackageSet` и canonical manifest hash в `GV2ContentHostSupport`; доказать одинаковый порядок/fingerprint в UE и Headless.
3. Построить private `FGV2SessionContentCandidate` и immutable snapshot из exact package set; закрыть package isolation и multi-session lifetime.
4. Перевести schemas, screens, images, Theme и GameShell на snapshot; удалить globals и runtime settings accessors.
5. Создать `GV2PresentationApply`, перенести физическое применение и выполнить контролируемую миграцию Widget Blueprint class paths.
6. Сделать prepared payload самодостаточным для всех semantic kinds и удалить resolution из Commit.
7. Заменить ручные authority lists build-graph/type inventories, выполнить adversarial scenarios и полную cross-host/UE верификацию.
8. Синхронизировать contracts/status, независимо проверить исход каждого finding и только затем архивировать audit и proposal.

Snapshot вводится до переноса модуля: так миграция не смешивает исправление ownership с изменением UCLASS paths, а каждый этап имеет наблюдаемую границу.

## Риски и ограничения

| Риск | Ограничение |
|---|---|
| Новый слой превращается во второй pipeline | Apply принимает операции существующего generic pipeline; новой schema/authoring grammar нет |
| Перенос UCLASS ломает Blueprint assets | временные redirects, Editor load/compile/resave, Asset Registry sweep, затем удаление redirects |
| Snapshot копирует repository data | хранится read handle; definitions/provenance не копируются |
| UObject уничтожается раньше snapshot | единый GC-safe pin set, отдельный lifecycle test с двумя sessions |
| Headless получает UE dependency | `FResolvedPackageSet` остаётся portable; snapshot и Apply module существуют только в UE host |
| Startup становится тяжелее | работа уже обязательна для корректности; async/deferred loading остаётся отдельным measurement-gated proposal |
| Gate снова проверяет известные имена | первичны dependency direction и отсутствие типов; token scan остаётся только defense-in-depth |

## Не входит

- изменение Lua gameplay, Command/Event path или canonical state;
- presentation/media loading в Headless;
- новый Screen Definition или authoring syntax;
- возврат schema-specific DTO/adapter;
- async loading до прохождения существующего measurement gate;
- исправление `STATUS-001…003` и контракта обязательности полей сцены, не относящихся к authority closure.

## Definition of Done

- Все findings `PAH-R1…R7` имеют доказанный исход; подтверждённый gap не остаётся только в audit.
- У session coordinator ровно один published immutable snapshot; process-global presentation authorities отсутствуют.
- Repository, Lua и presentation получают один `FResolvedPackageSet`; downstream discovery отсутствует.
- Ни один disabled package не читается presentation builders.
- Commit не имеет зависимости или runtime-доступа к content/settings authority.
- Каждый semantic prepared value содержит resolved payload, достаточный для применения.
- `GV2PresentationApply` не линкуется с authority/bootstrap modules; обратное ребро останавливает build.
- Headless собирается без UE/Presentation, сохраняет текущий run digest и использует тот же portable package-set resolver.
- Widget Blueprints загружаются, компилируются и сохраняются после module migration; старые `/Script/GV2` class references отсутствуют.
- Полные CTest, `gv2-headless --check-scripts`, UE Automation и documentation validation зелёные; negative self-tests реально отклоняют внесённые нарушения.

## Архитектурные изменения перед планированием

Потребуется новый ADR: он не отменяет `INV-P1…P5`, а материализует их через единый session candidate, immutable snapshot и запрещённое dependency direction. ADR обязан явно пересмотреть вывод архивного `PresentationAuthorityConsolidationProposal`, что отдельная физическая граница применения не заслужена: условие повторного открытия наступило, потому что новый Theme authority дал false green существующим фазовым гейтам.

Синхронно обновляются [System Context and Components](../Architecture/SystemContextAndComponents.md), [Dependency Map](../Architecture/DependencyMap.md), [Bootstrap and Session Lifecycle](../Architecture/BootstrapAndSessionLifecycle.md), [Headless Simulation Contract](../Architecture/HeadlessSimulationContract.md) и UI contracts, владеющие Prepare/Commit и resource/theme semantics.
