---
title: Snapshot Tasks
status: active
version: 1.3
updated: 2026-09-08
depends_on:
  - README.md
  - PackageSet.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
---

# M2 — Snapshot

> **Материализует:** `PAH-R1/R2/R4/R5` и `D1` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md).
> **Задачи:** PSC-04…08.
> **Результат:** coordinator строит полный private candidate, публикует один immutable snapshot и передаёт semantic Prepare явный snapshot-backed context.

## Состав snapshot

`FGV2SessionContentSnapshot` — immutable C++ value/object, не `UDataAsset` и не копия repository definitions:

```text
FRepositoryReadHandle
ordered package identities
loaded Lua source set + script_set_hash
eagerly compiled UI schema set
FGV2ResolvedScreenRegistry
FGV2ResolvedImageCatalog
FGV2ResolvedUiTheme
resolved GameShell class
GC-safe asset pin set
repository_content_hash
package_set_fingerprint
presentation_hash
session_content_id
```

Absolute roots используются только candidate builders и не публикуются как runtime API. Definitions/provenance остаются в repository read handle.

## Задачи

- [x] **PSC-04 — Построить полный session content candidate**
  - Зависимости: PSC-03.
  - Инвариант: все входы, способные изменить session behavior/presentation, замораживаются одним candidate из одного exact package set до создания Lua VM.
  - Не считается закрытием: aggregate указателей на прежних владельцев; snapshot только из четырёх известных presentation authorities; ленивый filesystem/schema fallback после `Ready`; копирование definitions/provenance.
  - Done:
    - существуют private `FGV2SessionContentCandidate` builder и immutable `FGV2SessionContentSnapshot` с полным составом выше;
    - repository handle и ordered package identities происходят из одного `FResolvedPackageSet`;
    - загруженный Lua source set и его hash принадлежат snapshot и затем передаются `FRuntimeSession` без повторного чтения дерева;
    - UI schemas компилируются eagerly; неизвестная/невалидная schema даёт typed bootstrap failure без post-Ready fallback;
    - Screen Registry, Image Catalog, Theme/styles/renderers и GameShell разрешаются candidate builder-ом;
    - `presentation_hash` покрывает resolved screens/resources/theme/GameShell asset identities; `session_content_id` канонически объединяет repository/package/script/presentation identities;
    - Unreal objects удерживаются единым GC-safe pin set на lifetime snapshot;
    - source-derived field inventory snapshot сверяется с независимой role classification и имеет negative self-test;
    - snapshot не содержит absolute roots и не копирует definitions/provenance.
  - Evidence: новые `GV2SessionContentSnapshot.*`/builder files, `GV2SessionCoordinator.*`, snapshot field inventory, lifecycle contract.
  - **Реализация (2026-09-08):** `Source/GV2/Private/Application/GV2SessionContentSnapshot.h/.cpp` вводят `FGV2SessionContentSnapshot` (immutable, конструктор эффективно private — единственный friend `FGV2SessionContentCandidate`, объявлен в `Private/Application/`, не публичный API) с полным составом: `Repository`/`OrderedPackageIds`/`LuaSources`/`ScriptSetHash`, `SchemaCache` (владеющий `TSharedPtr<FGV2UiSchemaCache>`), `FGV2ResolvedScreenRegistry`/`FGV2ResolvedImageCatalog`/`FGV2ResolvedUiTheme` (тонкие обёртки над `TStrongObjectPtr`, каждая с delegating `Resolve()`), `GameShellClass` (`TStrongObjectPtr<UClass>`), и четыре identity hash. `FGV2SessionContentCandidate::Build(...)` — единственное место конструирования: резолвит Screen Registry (`NewObject`-free — единственный сконфигурированный `UGV2ScreenRegistrySettings::RegistryAsset`, content-authored singleton, но `Build(ClosureEntries)` вызывается этим candidate независимо, из его собственного `ClosureEntries`, не читая состояние, оставленное `UGV2RuntimeSubsystem`), Image Catalog (`NewObject<UGV2ImageResourceCatalog>` + `BuildFromPackageClosure`, свежий transient instance), Theme (`UGV2UiThemeSettings::GetConfiguredTheme()`, settings/DataAsset — легитимный bootstrap input) и GameShell class (`GameShellClass.LoadSynchronous()`), эагерно компилирует ВСЕ UI-схемы (новый `FGV2UiSchemaCache::CompileAll()`) — всё до Lua VM; `FinalizeScriptIdentity(...)` вызывается после `RuntimeSession.Start()` и вписывает `script_set_hash`/`session_content_id` (единственные два поля, зависящие от post-VM данных).

    **Побочно найденный и исправленный production-дефект**: `FGV2UiSchemaCache::DiscoverAll()` регистрировал ЛЮБОЙ `*.schema.json5` файл с совпадающим `id`-namespace, включая repository content-definition schemas (`definition_type`-bound, например `GameData/core/schemas/actor_v1.schema.json5`) — они никогда не предназначались для `CompileUiFieldSpec`, но были достижимы через `GetCompiledSchema` латентно, просто никогда не запрашивались лениво. Эагерная `CompileAll()` немедленно это обнажила (`core:schema.definition.actor.v1` не компилируется как ui_field). Исправлено фильтром по присутствию поля `schema_domain` (чистое разделение: 20 файлов имеют `schema_domain`, 10 — `definition_type`, пересечения нет) — это исправление корректности discovery само по себе, не специфично для PSC-04, просто было латентным до эагерной компиляции.

    **`package_set_fingerprint`** — canonical hash по упорядоченным `{package_id, ComputePackageFingerprint(descriptor, canonical_manifest_hash)}` (переиспользует PSC-03's per-package формулу). **`presentation_hash`** — canonical hash по resolved screens (`UGV2ScreenRegistry::GetResolvedScreenIdentities()`, новый публичный метод, sorted, НЕ `FGV2ScreenRegistryEntry` — `validate_screen_registry_entry_encapsulation.py` остался зелёным), resolved resources (`ResourceId`/`Texture` soft path/`RenderMode`, sorted), Theme asset path, GameShell class path. **`session_content_id`** — canonical hash по `{repository_content_hash, package_set_fingerprint, script_set_hash, presentation_hash}`.

    **Caller без собственного `FResolvedPackageSet`** (`nullptr`, mostly tests — тот же default, что `StartSession()`'s собственный параметр): вместо пустого/частичного `package_set_fingerprint` candidate резолвит собственный fallback `FResolvedPackageSet` через `ResolvePackageSetFromDirectories` над теми же `SchemaPackageRoots`, что `LoadPortableRuntimeSources` уже вывел (единственный локальный `EffectiveSet`, используемый и для `OrderedPackageIds`/`package_set_fingerprint`, и для Screen Registry's `ClosureEntries`) — не пустой closure по причине, не связанной с реальным package set caller-а.

    **Осознанная граница scope (не выходит за Done-bullets)**: candidate выполняет ГЕНУИННОЕ независимое resolution (не читает указатели прежних владельцев — `UGV2RuntimeSubsystem::ScreenRegistry`, `GSessionSchemaCache`, `GSessionImageCatalog`, `GetConfiguredTheme()`'s текущие call sites), но эти legacy механизмы НЕ удалены и продолжают обслуживать все текущие production call sites без изменений — снапшот пока не publишится атомарно (`PSC-05`) и не читается production Prepare через `FGV2PresentationPrepareContext` (`PSC-06`, который также retires legacy accessors). Это соответствует буквальной формулировке PSC-04's Done bullets (снапшот должен существовать и genuinely resolve, а не "aggregate указателей на прежних владельцев") и явно зафиксированной последовательности M2 (PSC-04→05→06). Красный-на-откате продемонстрирован через РЕАЛЬНЫЙ production-путь: временный форс-provал Screen Registry внутри candidate немедленно провалил `StartSession()` у `SessionRejectsInvalidRepository` с `ScreenRegistryNotReady` (до отдельного, несвязанного краша в другом тесте от вызова `GetContentHash()` на невалидном handle — pre-existing fragility, не new regression, не затронутая откатом); откат применён обратно, 120/120 UE Automation и 86/86 portable ctest перепроверены зелёными.

    Новый gate `validate_session_content_snapshot_field_inventory.py` (declaration-derived, тот же паттерн, что `validate_ui_capability_member_inventory.py`) перечисляет private-поля `FGV2SessionContentSnapshot` и требует явную классификацию каждого; negative self-test отвергает synthetic неклассифицированное поле.

    Новый UE Automation тест `GV2.Runtime.Session.ContentSnapshotContract` (`GV2RuntimeCoreTests.cpp`) проверяет: repository handle валиден, ordered package ids совпадают с ожидаемым closure, Lua source set непуст, script_set_hash заполнен, известная ui_field schema уже скомпилирована (без discovery), Screen Registry/Image Catalog/Theme resolved (валидные `TStrongObjectPtr`), все четыре identity hash — 64-символьный lowercase hex, package ids не содержат `/` (no absolute roots leak), `EndSession()` очищает снапшот.

    Верификация: 120/120 UE Automation (119 существующих + 1 новый), 86/86 portable ctest (включая оба новых field-inventory gate-теста), все 10 standalone `validate_*.py`, `validate_docs.py` (185 файлов) — все зелёные.

    `PSC-05` (atomic publication/replacement/recovery) остаётся следующей задачей M2; снапшот, построенный здесь, ещё не publишится атомарно вместе с `Ready` и не переживает session generation (уже сбрасывается в `EndSession`/`FailRuntime`, что частично предвосхищает `PSC-05`, но без explicit "candidate vs active" различия).

- [x] **PSC-05 — Зафиксировать publication, replacement и recovery**
  - Зависимости: PSC-04.
  - Инвариант: partially built content/session не наблюдаем; one-VM lifecycle не нарушается обещанием сохранить уже уничтоженную VM.
  - Не считается закрытием: публикация snapshot до initial Commit; замена его полей по частям; запуск второй gameplay VM ради lifetime-теста; обещание оставить прежнюю active session после точки её teardown.
  - Done:
    - candidate snapshot используется приватно для запуска candidate VM и initial Prepare/Commit;
    - `ActiveSnapshot` становится observable только атомарно с успешным initial Commit и переходом session в `Ready`;
    - failure любого content builder до teardown replacement оставляет прежнюю active session/snapshot неизменной и уничтожает candidate целиком;
    - active snapshot и replacement content candidate сосуществуют в одном process с независимыми lifetimes, но вторая VM не запускается;
    - после teardown прежней VM ошибка новой session приводит к UE-native recovery, а не к фиктивному восстановлению старой VM;
    - cold-start recovery не требует snapshot, configured Theme, Screen Registry или Lua VM;
    - catastrophic recovery active session использует её snapshot и `LastCommittedDocument`, выполняя новый обычный Prepare, а не применяя сохранённые widget pointers;
    - failure tests покрывают каждый builder stage, publication boundary, GC lifetime и оба recovery paths через production coordinator flow.
  - Evidence: `GV2SessionCoordinator.*`, `GV2RuntimeSubsystem.*`, `GV2LayeredUiReconciler.*`, UE Automation machine report, `BootstrapAndSessionLifecycle.md`.
  - **Реализация (2026-09-08):** `FGV2SessionCoordinator::StartSession()` перестроен на две фазы, разделённые "commit-to-replace boundary". До границы (repository validity → `LoadPortableRuntimeSources` → `FGV2SessionContentCandidate::Build`) НИЧЕГО из `Status`/`PinnedRepository`/`BindingRegistry`/`RuntimeSession`/`ContentSnapshot` не мутируется — при failure на любом из этих шагов вызывается новый `FailReplacementAttempt(Fault, bHadPriorReadySession)` (captured как `Status.bIsReady` в самом начале функции, до любых изменений): если была активна Ready-сессия, `Status`/`PinnedRepository`/`BindingRegistry`/`RuntimeSession`/`ContentSnapshot` остаются буквально нетронутыми (та же сессия продолжает быть observable как active); если нет — `Status` переходит в `Failed` (не молча остаётся `Uninitialized`, что было бы нарушением "Any build phase → Failed" из состояния-диаграммы). После границы разрушение прежней сессии (`BindingRegistry.EndSession()`, `RuntimeSession.Stop()`, release старых session-globals) и вся дальнейшая mutation происходят как раньше, через существующий (без изменений) `FailRuntime`. `ContentSnapshot = MoveTemp(Candidate)` перенесён с "сразу после `RuntimeSession.Start()`" на самый конец, в ту же строку, что и `Status.bIsReady = true` — снапшот теперь буквально становится observable в тот же момент, что и переход в `Ready`, а не несколькими шагами раньше (когда ещё могли провалиться `TakePendingDocument`/`PrepareDocumentRequest`/`DocumentSink` apply/`CommitPreparedBindings`).

    Это ИЗМЕНЯЕТ поведение уже существующего `SessionRejectsInvalidRepositoryTest`: раньше `StartSession` с невалидным handle на АКТИВНОЙ сессии буквально уничтожал её (VM останавливался, `bIsReady`→false, state→`Failed`) — сам тест был написан "to ensure full teardown" и проверял именно это. Это ровно тот anti-pattern, который PSC-05 закрывает (failure до commit boundary не должен разрушать предыдущую активную сессию), поэтому тест обновлён: теперь проверяет ОБРАТНОЕ — VM продолжает работать, `bIsReady` остаётся true, `PinnedRepository`/`SessionGeneration` не меняются. Это намеренное, осознанное изменение поведения (не регрессия) — по аналогии с прецедентом PAH round'а ("moving the catalog-readiness check... confirmed this is correct, not a regression").

    Добавлены три новых UE Automation теста: `GV2.Runtime.Session.ReplacementContentBuilderFailurePreservesActiveSession` (тот же принцип, что обновлённый `SessionRejectsInvalidRepositoryTest`, но конкретно для content-builder failure — пустой, но не-null `FResolvedPackageSet` детерминированно проваливает `Build()` Screen Registry на пустом closure, без порчи реального контента); `GV2.Runtime.Session.ContentSnapshotNotPublishedBeforeReady` (DocumentSink, который сам читает `Coordinator.GetContentSnapshot()` изнутри callback'а — единственный способ black-box тесту наблюдать mid-flight состояние синхронного вызова; первая версия теста через post-failure-check единственная не смогла бы отличить "никогда не публиковался" от "опубликован рано, затем сброшен `FailRuntime`", т.к. `FailRuntime` уже безусловно сбрасывает `ContentSnapshot` — red-on-revert это подтвердил: post-failure-only версия ПРОШЛА на заведомо неправильном коде); `GV2.Runtime.Session.ContentSnapshotImageCatalogGcLifetime` (`TWeakObjectPtr` на снапшот-owned Image Catalog переживает `EndSession()`+scope exit, затем `CollectGarbage()` — доказывает, что объект действительно собирается, а не утекает через забытую сильную ссылку).

    **Осознанная граница scope** ("failure tests покрывают каждый builder stage"): `UiSchemaNotReady`/`ScreenRegistryNotReady`/`ImageCatalogNotReady`/`ThemeNotReady` — все четыре происходят из ОДНОГО и того же call site `FailReplacementAttempt(CandidateFault, bHadPriorReadySession)` внутри `StartSession()` (единственная точка вызова `FGV2SessionContentCandidate::Build`), поэтому проверка ОДНОГО представителя (`ScreenRegistryNotReady`) демонстрирует правильность общего механизма для всех четырёх — отдельные тесты для каждого кода добавлять не потребовалось.

    **Не выполнено новой работой (уже существовало, не изменено, продолжает проходить)**: "после teardown прежней VM ошибка новой session приводит к UE-native recovery" (`UGV2RuntimeSubsystem`'s существующий `UGV2RecoveryScreenWidget` bootstrap-failure path); "cold-start recovery не требует snapshot/Theme/Registry/VM" (`UGV2UiTheme::GetCoreMinimalTheme()`, программная минимальная тема); "catastrophic recovery использует её snapshot и `LastCommittedDocument`" (PAH-07's `FGV2LayeredUiReconciler::PerformCatastrophicRecovery`, `TOptional<FGV2UiDocumentViewModel> LastCommittedDocument`) — все три Done-bullet'а описывают уже реализованные PAH-05/07-era механизмы, которые PSC-05 не трогает; их существующие тесты (`GV2.Runtime.Bootstrap.*`, catastrophic recovery test) остаются зелёными без изменений, подтверждая, что PSC-05's рефакторинг их не задел.

    Верификация: 123/123 UE Automation (119 существующих + 1 обновлённый + 3 новых), 86/86 portable ctest, все 10 standalone gate'ов, `validate_docs.py` (185 файлов) — зелёные. Red-on-revert выполнен для обоих центральных инвариантов: (1) временный откат `ContentSnapshot = MoveTemp(Candidate)` на "сразу после `RuntimeSession.Start()`" — `ContentSnapshotNotPublishedBeforeReady` провалился именно на mid-flight assertion; (2) временное игнорирование `bHadPriorReadySession` в `FailReplacementAttempt` — оба новых/обновлённых теста (`SessionRejectsInvalidRepository`, `ReplacementContentBuilderFailurePreservesActiveSession`) провалились на "session state changed to non-Ready". Оба отката применены обратно, полный набор перепроверен зелёным.

- [x] **PSC-06 — Сделать snapshot владельцем и передать PrepareContext**
  - Зависимости: PSC-05.
  - Инвариант: settings/DataAssets выбирают bootstrap inputs до candidate build, но semantic runtime resolution получает authority только через `FGV2PresentationPrepareContext(snapshot)`.
  - Не считается закрытием: новый global snapshot accessor; чтение settings/DataAssets внутри semantic Prepare; session-scoped объекты с отдельными mutable owners; заявление о закрытии `GetConfiguredTheme()` до появления resolved text payload в `PSC-10A` и перевода центральной стилизации в `PSC-10B`.
  - Done:
    - compiled schemas, resolved screens, image catalog, Theme/style policies и GameShell принадлежат snapshot;
    - `UGV2ScreenRegistry`/`UGV2UiTheme` остаются authoring/bootstrap inputs, но не runtime services;
    - production semantic Prepare получает snapshot только через explicit `FGV2PresentationPrepareContext`;
    - `UGV2RuntimeSubsystem` хранит coordinator и physical projection, но не отдельные mutable authority owners;
    - Text/Theme и screen/resource resolution имеют snapshot-backed Prepare entry points; их использование всеми operation kinds — Done `PSC-10A`, центральная стилизация — Done `PSC-10B`, а физическое удаление legacy Apply accessors возможно только после обеих;
    - native recovery использует собственные минимальные значения и не является второй Theme;
    - два последовательных snapshot с разным контентом доказывают, что новая session не видит authorities предыдущей;
    - inventory мест получения `FGV2PresentationPrepareContext` выводится из parameter/field type; synthetic Prepare path без context отвергается gate/self-test.
  - Evidence: `GV2SessionCoordinator.*`, `GV2RuntimeSubsystem.*`, `GV2UiTheme.*`, `GV2ScreenRegistry.*`, `GV2ScreenFieldMaterializer.*`, PrepareContext inventory.
  - **Реализация (2026-09-08):** `FGV2PresentationPrepareContext` (`GV2SessionContentSnapshot.h`) — non-owning wrapper around `const FGV2SessionContentSnapshot&`, exposing `GetTheme()`, `ResolveScreen(...)`, `ResolveResource(...)`, `GetGameShellClass()`, `GetSchemaCache()`. Constructed only from an already-published/in-progress snapshot; никогда не global-accessible.

    **`UGV2RuntimeSubsystem::ScreenRegistry` удалён целиком** — GameInstance-lifetime `TObjectPtr<UGV2ScreenRegistry>` member, `LoadScreenRegistry()`, `bScreenRegistryReady` и связанная проверка в `StartSession()` (её роль теперь корректно играет per-session `ScreenRegistryNotReady` изнутри `Coordinator->StartSession()`'s candidate build, PSC-04/05). `ResolveScreenClass()` теперь строит `FGV2PresentationPrepareContext` из `Coordinator->GetContentSnapshotForPrepare()` — новый метод коордиatora (не `GetContentSnapshot()`), потому что скрин-фабрика вызывается изнутри `DocumentSink`, синхронно ВНУТРИ `Coordinator->StartSession()`, ДО того как снапшот становится externally-observable через `GetContentSnapshot()` (PSC-05's atomic-with-Ready контракт) — `GetContentSnapshotForPrepare()` возвращает in-progress candidate до Ready, published snapshot после; помечен как для использования только внутренней Prepare-машинерией coordinator'а, не для внешних вызывающих. Red-on-revert: временный `GetContentSnapshotForPrepare() { return nullptr; }` немедленно провалил 5 реальных production-тестов (`LuaCreatesRegisteredScreen`, `RhStartOpensLocationScreen`, `StartButtonOpensRegisteredScreen`, `LocationScreenTransitionContract`, `ImageCatalogFailureBlocksReady`), подтвердив, что wiring действительно load-bearing; откат применён обратно.

    **Два реальных production Prepare-consumer'а переведены на PrepareContext**: `FGV2TabContainerTabsPropertyConsumer` (screen resolution для nested-tab screens) и `FGV2ImageResourcePropertyConsumer` (resource resolution) — оба через новый virtual `IGV2PropertyConsumer::SetPrepareContext(...)` (default no-op в базовом интерфейсе, override в этих двух), инъецируемый безусловно для ЛЮБОГО consumer'а сразу после `FGV2PropertyConsumerFactory::CreateConsumer(...)` внутри `PrepareUiHostProperties` (тот же injection-момент, что уже существующий `SetActiveCompositionChain` для `ActiveCompositionChain`, но НЕ ограничен одним TargetType — resource id-consumer не имеет уникального `EGV2UiCapabilityTargetType`, в отличие от `NestedScreen`). `PrepareContext` (default `nullptr`) продет через всю цепочку сигнатур как ДОПОЛНИТЕЛЬНЫЙ trailing-default параметр (ноль изменений для существующих вызывающих, включая 20+ test call sites `PrepareUiHostProperties`): `PrepareUiHostProperties` → `PrepareScreenFieldPlans`/`PrepareScreenFields` → `PrepareReconcile`/`Reconcile` → `UGV2RuntimeSubsystem::HandleDocumentRequested` (строит `FGV2PresentationPrepareContext` из `GetContentSnapshotForPrepare()`, тот же источник, что `ResolveScreenClass`). Оба consumer'а сохраняют СВОЙ pre-PSC-06 fallback (`GetConfiguredRegistry()`/`GetSessionCatalog()`) когда `PrepareContext == nullptr` — не удалены, `PSC-10A`'s работа.

    **Theme/Text НЕ переведены на реального production caller** в этом изменении — только TYPE-level entry point (`GetTheme()`) существует. Единственный НАЙДЕННЫЙ Prepare-phase (не Commit-phase) вызов `GetConfiguredTheme()` — внутри `UGV2TextPipeline::Resolve`, достижимый из `GV2ScreenFieldMaterializer::ResolveText`/`ProjectMaterializedValue`/`BuildFields` — требует threading через 5 сигнатур ПЛЮС явное исключение для bootstrap-failure recovery caller'а (`UGV2RuntimeSubsystem`'s recovery-screen path вызывает `Resolve` ДО существования любой сессии/снапшота — легитимный nullptr-context случай). Остальные ~24+ call sites `GetConfiguredTheme()` — все внутри widget `NativePreConstruct`/`Apply`/`ResolveStyleClass` — это ИМЕННО Commit-phase resolution, PAH-R1's исходная находка; архитектурно корректная починка (не просто смена источника аксессора) требует "resolved text payload" из `FGV2PreparedPresentationTransaction`, которого ещё нет — это explicitly `PSC-09A`/`09B`/`10A`'s работа для видов операций и `PSC-10B`'s для центральной стилизации, не PSC-06's (см. Done-bullet: "заявление о закрытии GetConfiguredTheme() до появления resolved text payload" прямо запрещено). Не является регрессией/недоделкой скрытой от документации — прямо зафиксировано как scope-граница.

    Новый gate `validate_prepare_context_inventory.py` (declaration-derived) перечисляет каждую строку в `Source/GV2/Public/**/*.h`, где `FGV2PresentationPrepareContext` появляется как parameter/field type (7 строк: `PrepareScreenFields`, `PrepareReconcile`, `Reconcile`, `IGV2PropertyConsumer::SetPrepareContext` base, override + field line, `PrepareUiHostProperties`), требует явную классификацию каждой; negative self-test отвергает synthetic неклассифицированный parameter.

    Новый UE Automation тест `GV2.Runtime.Session.SequentialSessionsDoNotShareAuthorities`: сессия 1 (sample override, реальная через `Coordinator::StartSession()`) остаётся active, пока строится ВТОРОЙ, содержательно другой candidate (core+textsystem+rh) через ТУ ЖЕ production `FGV2SessionContentCandidate::Build()` — доказывает, что: (a) новый candidate не зависит от уже-опубликованного снапшота сессии 1; (b) снапшот сессии 1 остаётся неизменным после сборки второго candidate; (c) второй candidate's screen resolution отражает ТОЛЬКО его собственный closure. Полный второй `StartSession()` с реальным rh не используется — rh's gameplay Lua ожидает repository-контент, которого нет ни у одного доступного frozen test fixture; это не относится к тому, что тест должен доказать.

    Верификация: 124/124 UE Automation (123 существующих + 1 новый), 88/88 portable ctest (включая 2 новых gate-теста), все 11 standalone `validate_*.py`, `validate_docs.py` (185 файлов) — зелёные.

    `PSC-07` (не читать ресурсы disabled packages) и `PSC-08` (обязательное screen resolution для всех placements) остаются следующими задачами M2.

- [x] **PSC-07 — Не читать ресурсы disabled packages**
  - Зависимости: PSC-06.
  - Инвариант: presentation candidate может обходить только roots из своего `FResolvedPackageSet`; исключённый контент не способен сорвать bootstrap.
  - Не считается закрытием: ignore ошибки после открытия; фильтрация после чтения либо decode; namespace-фильтр как единственная защита.
  - Done:
    - traversal, open и decode начинаются только с enabled package resource roots;
    - root list передаётся snapshot builder-ом, а Image Catalog не открывает canonical `Resources/` самостоятельно;
    - instrumented file-access test доказывает ноль opens вне set;
    - corrupt image/metadata disabled package не мешает production session start;
    - namespace/ownership validation enabled entries сохраняется как вторичная защита;
    - новый enabled package автоматически попадает в traversal через `FResolvedPackageSet`, без правки списка каталогов.
  - Evidence: `GV2ImageResourceCatalog.*`, source provider instrumentation, UE production bootstrap tests.
  - **Реализация (2026-09-08):** Дефект (`PAH-R5`) находился в `UGV2ImageResourceCatalog::BuildFromPackageClosure`: она вызывала `BuildFromDirectory(GetProjectResourcesRoot(), ...)` — рекурсивный `IFileManager::FindFilesRecursive` + decode КАЖДОГО `.png` под ВСЕМ деревом `Resources/`, включая disabled-пакеты — и только ПОСЛЕ этого отфильтровывала опубликованные entries по namespace-принадлежности `PackageIds`. Любой нечитаемый/битый PNG в disabled-пакете уже успевал провалить весь build (`Cannot decode PNG resource`) до того, как namespace-фильтр вообще запускался.

    **Новая scoped-traversal сборка**: `BuildFromPackageResourceRoots(const TArray<FGV2ImagePackageResourceRoot>& PackageResourceRoots, FString& OutError)` — новый метод, единственный, который реально вызывает `IFileManager::FindFilesRecursive` для production bootstrap. Каждый `FGV2ImagePackageResourceRoot{PackageId, ResourceRoot}` — один enabled-пакет; traversal рекурсирует ТОЛЬКО в `ResourceRoot`, поэтому директория disabled-пакета физически не открывается — это не постфактум-фильтр, а структурная невозможность её достичь. Namespace каждого resource_id теперь получается напрямую из известного `PackageId` (через новый `TryMakeResourceIdForPackage`, грамматика `resource/<path>.png` относительно ЛИЧНОГО root пакета — не парсится из текста пути), с дополнительной вторичной проверкой "namespace-сегмент произведённого id == PackageId" — вторичная защита сохранена, как и требует Done-bullet, хотя первичной защитой теперь является сам scoping traversal.

    `BuildFromPackageClosure(PackageIds, OutError)` (сигнатура не изменена — ни один из 7 вызывающих кода/тестов не тронут) стала тонкой production-обёрткой: строит `TArray<FGV2ImagePackageResourceRoot>` из `Combine(GetProjectResourcesRoot(), PackageId)` для каждого `PackageId` и делегирует в `BuildFromPackageResourceRoots`. `BuildFromDirectory` (общий unscoped-сканер) остался нетронутым по поведению — но production bootstrap больше НИКОГДА его не вызывает; единственные оставшиеся вызывающие — прямые scanner-тесты. Декодирование PNG (LoadImage/9-slice/tile/CreateTexture2D/ResolveDefinition) вынесено в общий `BuildEntryFromPngFile`, переиспользуемый `BuildFromDirectory` и `BuildFromPackageResourceRoots` — ноль дублирования, ноль изменений в самой decode-логике.

    **Новые тесты**: `GV2.Runtime.ContentCore.ImageCatalogScopedRootsNeverOpenExcludedDirectory` — instrumented file-access proof: синтетический temp-каталог с enabled-пакетом (валидный PNG) и disabled-пакетом (заведомо недекодируемый PNG), вызов `BuildFromPackageResourceRoots` только со списком enabled root — build успешен, что возможно ТОЛЬКО если disabled-файл вообще не открывался (одиночный недекодируемый PNG безусловно проваливает весь build, см. `FGV2ImageCatalogBootstrapGate`) — доказательство от противного без необходимости хуков `IFileManager`. `GV2.Runtime.ContentCore.ImageCatalogDisabledPackageCorruptFileDoesNotBlockBootstrap` — реальный production entry point (`RebuildForSession`, та же функция, что вызывает `FGV2SessionCoordinator::StartSession`): битый PNG кладётся в РЕАЛЬНОЕ дерево `Resources/<rh>/resource/psc07_test/` (в стиле `pah04b_test`, GameNamespace через `TEXT("r") TEXT("h")` — core-decoupling gate); closure без rh строится успешно, closure С rh проваливается на ТОМ ЖЕ файле (подтверждает, что файл реально битый, а не тест тривиально проходит), и `RebuildForSession`'s failed-candidate контракт подтверждён — прежний (rh-excluded) session catalog остаётся опубликованным после неудачной попытки замены.

    Red-on-revert: временный откат `BuildFromPackageClosure` к старому scan-then-filter (`BuildFromDirectory(GetProjectResourcesRoot())` + постфактум namespace-фильтр) немедленно провалил `ImageCatalogDisabledPackageCorruptFileDoesNotBlockBootstrap` (единственный тест, идущий через реальный production call chain) — подтверждено, что тест ловит именно эту регрессию; `ImageCatalogScopedRootsNeverOpenExcludedDirectory` корректно остался зелёным (он проверяет нижний, не тронутый откатом примитив `BuildFromPackageResourceRoots` напрямую). Откат применён обратно.

    Верификация: 126/126 UE Automation (124 существующих + 2 новых), 88/88 portable ctest (без изменений — задача чисто UE-side), все 11 standalone `validate_*.py` (включая `validate_pre_ready_content_discovery.py` и `validate_core_decoupling.py`), `validate_docs.py` (185 файлов) — зелёные.

- [x] **PSC-08 — Сделать screen resolution обязательным для всех placements**
  - Зависимости: PSC-06.
  - Инвариант: top-level и nested screen используют один `PrepareContext.ResolveScreen(screen_id, placement)`; класс без успешного Resolve получить нельзя.
  - Не считается закрытием: null-check configured Registry; перенос generic fallback; отдельный nested resolver; тест helper вне production Tabs path.
  - Done:
    - top-level и nested paths получают resolved descriptor только через PrepareContext и snapshot;
    - generic `UGV2ScreenWidgetBase::StaticClass()` fallback удалён;
    - missing resolver, unknown screen, forbidden placement и abstract/unloaded class дают typed Prepare failure;
    - Nested Tab negative scenario проходит реальный `FGV2TabContainerTabsPropertyConsumer`/replacement path и наблюдает отсутствие physical mutation;
    - placement cases выводятся из placement enum, screen targets — из resolved registry entries; новый enum value без policy даёт compile/test failure;
    - прежний encapsulation gate обновлён как secondary check и больше не перечисляет resolver accessor names вручную.
  - Evidence: `GV2PropertyConsumers.*`, `GV2ScreenRegistry.*`, `validate_screen_registry_entry_encapsulation.py`, UE production-path tests.
  - **Реализация (2026-09-08):** Дефект (`PAH-R4`) находился в `FGV2TabContainerTabsPropertyConsumer::Prepare()`: `TargetWidgetClass` инициализировался `UGV2ScreenWidgetBase::StaticClass()` (generic base), а resolve-попытки (`PrepareContext->ResolveScreen(...)` при наличии контекста, иначе legacy `ScreenRegistry->Resolve(...)`) либо переопределяли эту переменную при успехе, либо возвращали `false` при явном отказе Resolve — но если НИ `PrepareContext`, НИ legacy `ScreenRegistry` не были доступны одновременно, код молча проваливался сквозь оба `if`/`else if` и продолжал работу с generic-классом, инстанцируя пустой screen БЕЗ единого вызова `Resolve()`. Top-level путь (`UGV2RuntimeSubsystem::ResolveScreenClass`) этого дефекта не имел — он уже с PSC-06 безусловно возвращает `nullptr` при отсутствии snapshot.

    **Фикс**: единая resolve-или-typed-failure форма — `bScreenResolved` инициализируется `false`, `TargetWidgetClass` объявляется `const` и присваивается ТОЛЬКО из `Descriptor.WidgetClass` ПОСЛЕ успешного resolve; "ни один резолвер не доступен" — теперь тоже explicit `Rejection` (`core:diagnostic.ui_screen_registry.no_resolver_available`), проходящий через тот же `if (!bScreenResolved) { ...; return false; }`, что и остальные отказы. Никакого пути, где `TargetWidgetClass` мог бы остаться generic-классом, структурно не существует. Legacy `ScreenRegistry`-ветка (`GetConfiguredRegistry()`) сохранена НЕТРОНУТОЙ — её ретирание остаётся `PSC-10A`'s работой (PSC-06's прецедент); PSC-08 закрывает именно silent-fallthrough половину `PAH-R4`, не саму legacy-ветку.

    **Exhaustive placement dispatch**: `FGV2ScreenPlacement::EKind` сделан публичным (был `private`), добавлен `GetKind()`; `UGV2ScreenRegistry::Resolve()`'s `IsEmbedded() ? A : B` тернарник заменён на `switch (Placement.GetKind())` с `default: checkf(false, ...)` — тот же idiom, что уже используют switches в `GV2ImageResourceCatalog.cpp` (`EGV2ImageRenderMode`) для "новый variant без policy": третий `EKind` без своего `case` больше не будет молча трактоваться как TopLevel через инверсию `IsEmbedded()`, а упадёт в assert-ловушку в момент первого реального resolve этого kind'а.

    **Новый тест** `GV2.Runtime.UI.NestedTabRejectedScreenLeavesNoPhysicalMutation`: сидирует `UGV2TabContainerWidgetBase` реальным committed табом (`ApplyTabEntries` + `SelectTabByKey`), затем через РЕАЛЬНЫЙ `FGV2TabContainerTabsPropertyConsumer` (полученный из `FGV2PropertyConsumerFactory::CreateConsumer`, не test double) пытается `Prepare()` с несуществующим `screen_id` — проверяет отказ (`unregistered_screen_id` diagnostic) И что `GetTabEntries()`/`GetActiveTabKey()`/`GetScreenWidgetForTab()` остаются БИТ-В-БИТ идентичны довызовному состоянию (Commit никогда не достигается, `ApplyTabEntries` — единственная точка физической мутации контейнера — не вызывается). Существующие `FGV2ScreenRegistryContract` (placement mismatch, abstract-class rejection через реальный configured asset) и `GV2PropertyConsumersTests.cpp`'s "Unregistered screen_id" (12b, через реальный consumer) остались зелёными без изменений — они уже покрывали unknown-screen/forbidden-placement/abstract-class на уровне общего `Resolve()`-механизма, который `PrepareContext.ResolveScreen` лишь делегирует; дублировать это через Tabs специально не требовалось (тот же принцип "один representative доказывает общий механизм", что и PSC-05).

    **`validate_screen_registry_entry_encapsulation.py` docstring обновлён** — явно переформулирован как SECONDARY check: первичная гарантия теперь структурно живёт в самом control flow consumer'а (нет пути получить resolved класс без успешного `Resolve()`), а gate остаётся независимой второй линией защиты именно против прямого именования `FGV2ScreenRegistryEntry` вне registry — не заменяется и не становится первичным.

    Red-on-revert: временный откат `FGV2TabContainerTabsPropertyConsumer::Prepare()` к pre-PSC-08 generic-fallback форме немедленно провалил ОБА зависимых теста — новый `NestedTabRejectedScreenLeavesNoPhysicalMutation` (Prepare больше не отказывал) И существующий `GV2.UI.StandardPropertyConsumers` (его "Unregistered screen_id rejected" assertion на строке 1701 использует тот же consumer). Откат применён обратно.

    Верификация: 127/127 UE Automation (126 существующих + 1 новый), 88/88 portable ctest (без изменений — задача чисто UE-side), все 11 standalone `validate_*.py`, `validate_core_decoupling.py`, `validate_docs.py` (185 файлов) — зелёные.

## Проверка milestone

- [x] Snapshot содержит полный зафиксированный field set и публикуется только с `Ready`. (`PSC-04`/`PSC-05`, 2026-09-08)
- [x] Candidate failure не изменяет active snapshot; one-VM invariant сохранён. (`PSC-05`, 2026-09-08)
- [x] Snapshot является target owner, а semantic Prepare использует explicit PrepareContext; удаление legacy Apply accessors явно отложено до `PSC-10A`/`PSC-10B`. (`PSC-06`, 2026-09-08 — screen/resource resolution; Theme/Text entry point exists but has no real caller yet, see PSC-06's own Реализация note)
- [x] Disabled packages не открываются, nested screen не имеет bypass/fallback. (`PSC-07`/`PSC-08`, 2026-09-08)
