---
title: "GV2 Presentation Authority — повторное архитектурное ревью"
status: informative
version: 1.0
updated: 2026-09-07
depends_on:
  - ImplementationStatus.md
  - ../ADR/0042-presentation-authority-and-publication.md
---

# GV2 Presentation Authority — повторное архитектурное ревью

> **Показывает:** активный раунд проверки `PresentationAuthorityHardening` и соответствия текущего production-кода инвариантам ADR-0042.

## 1. Scope

Проверена актуальная реализация `main` после завершения раунда `PresentationAuthorityHardening` (`PAH-01…09`) и отдельного закрытия `STATUS-012`.

Основной вопрос ревью:

> действительно ли текущая реализация выполняет принятые инварианты ADR-0042 — один immutable session-owned presentation authority, отсутствие discovery после `Ready`, отсутствие semantic resolution в `Commit`, единая keyed-реконсиляция и восстановление из committed logical state.

Проверка проводилась не только по архивному отчёту плана, но и по production source текущего HEAD.

Reviewed HEAD:

```text
036e16d113a5d446cb5575964907de2b86a3788d
```

## 2. Общий вывод

Раунд `PresentationAuthorityHardening` существенно улучшил архитектуру и реально закрыл несколько предыдущих проблем:

- порядок экранов в слоях теперь является явным состоянием и проходит через общий keyed-reconciliation primitive;
- rollback failure стал наблюдаемым;
- появился catastrophic recovery из последнего committed document;
- hardcoded ownership table заменён на `ue_content_roots`;
- unknown `/Game/` ownership стал fail-closed;
- image-resource resolution был перенесён из Commit в Prepare (`STATUS-012`).

Однако ключевой инвариант ADR-0042:

> **один immutable presentation/content snapshot, принадлежащий session coordinator**

фактически ещё не реализован.

Текущая архитектура по-прежнему содержит несколько независимых presentation authorities:

```text
FGV2SessionCoordinator
    └─ PinnedRepository

process-global
    ├─ GSessionSchemaCache
    └─ GSessionImageCatalog

UGV2RuntimeSubsystem
    └─ ScreenRegistry DataAsset

global settings / DataAsset
    └─ UiTheme
```

Дополнительно обнаружен новый дефект того же класса, что уже закрытый `STATUS-012`: **Theme/style semantics повторно разрешаются из Commit**, а существующий INV-P5 gate этого не видит.

Поэтому `PresentationAuthorityHardening` можно считать **функционально сильным, но архитектурно не полностью закрытым**.

---

## 3. Summary findings

| ID | Severity | Категория | Кратко |
|---|---:|---|---|
| `PAH-R1` | **P1** | architecture + implementation | Theme остаётся незахваченным authority и повторно резолвится в Commit |
| `PAH-R2` | **P1** | architecture | Реального coordinator-owned `FSessionContentSnapshot` нет |
| `PAH-R3` | **P1** | authority / package closure | Screen Registry строится из canonical closure, а не exact session package set |
| `PAH-R4` | **P1/P2** | authority bypass | Nested Tabs напрямую читают configured Registry и имеют fallback класса без Resolve |
| `PAH-R5` | **P1/P2** | package isolation | Image catalog валидирует disabled packages до closure filtering |
| `PAH-R6` | **P2** | package identity | `ue_content_roots` влияет на runtime authorization, но не входит в package fingerprint |
| `PAH-R7` | **P1/P2** | verification architecture | INV-P1/INV-P5 gates перечисляют authorities вручную и дали false green |

---

# 4. Findings

## PAH-R1 — P1 — Theme повторяет defect класса `STATUS-012`

### Проблема

`STATUS-012` был корректно закрыт для image resources:

```text
Prepare
    resolve resource
    store FGV2ResolvedImageResource

Commit
    ApplyResolved(...)
    no catalog lookup
```

Но Text/Theme pipeline всё ещё использует старый паттерн:

```text
Prepare/materialization
    UGV2TextPipeline::Resolve(...)
        → UGV2UiThemeSettings::GetConfiguredTheme()
        → resolve text/style semantics

Commit
    FGV2TextPropertyConsumer::Commit(...)
        → UGV2TextPipeline::Apply(...)
        → ResolveStyleClass(...)
        → UGV2UiThemeSettings::GetConfiguredTheme()
```

То есть подготовленное значение не содержит всего semantic decision, необходимого Commit.

### Evidence

Production paths:

```text
Source/GV2/Private/UI/GV2PropertyConsumers.cpp
    FGV2TextPropertyConsumer::Commit

Source/GV2/Private/UI/GV2TextPipeline.cpp
    UGV2TextPipeline::Apply
    UGV2TextPipeline::ApplyRichText
    UGV2TextPipeline::ResolveStyle
    UGV2TextPipeline::ResolveStyleClass
    UGV2TextPipeline::ResolveEffectiveFontSize

Source/GV2/Private/UI/GV2UiTheme.cpp
    UGV2UiThemeSettings::GetConfiguredTheme
```

`GetConfiguredTheme()` способен выполнять:

```cpp
Settings->ThemeAsset.LoadSynchronous();
```

Следовательно application phase структурно способен:

- читать global settings;
- разрешать Theme заново;
- синхронно загружать DataAsset;
- принимать semantic/style decision после Prepare.

### Нарушенные инварианты

ADR-0042:

```text
INV-P1 — после Ready нет discovery/content loading
INV-P2 — runtime decisions идут из одного immutable session authority
INV-P5 — semantic resolution завершается до Commit
```

### Дополнительная поверхность

Проблема не ограничивается Text.

Например:

```text
UGV2TabContainerWidgetBase::ApplyCentralStyle_Implementation()
    → UGV2UiThemeSettings::GetConfiguredTheme()
```

а central style вызывается после collection reconciliation.

### Почему gate не поймал

`validate_presentation_authority_phase.py` перечисляет известные authority accessors вручную:

```text
GetCompiledSchema
GetSchemaCache
GetSessionCatalog
FindOwningPackageForAssetPath
GetPackageLoadOrderFromGameData
ResolveContentRootOwnershipFromGameData
->Resolve(...)
```

Но не включает:

```text
GetConfiguredTheme
ThemeAsset.LoadSynchronous
GetDefault<UGV2UiThemeSettings>
```

Поэтому gate доказал более слабое свойство, чем заявленный инвариант.

### Рекомендуемое исправление

Theme должен стать частью session presentation snapshot.

Минимальная целевая форма:

```text
FSessionContentSnapshot
    ...
    Theme
```

Prepare должен сохранять уже разрешённую typography/style presentation:

```text
FGV2PreparedTextPresentation
{
    FText Text;
    FString NormalizedMarkup;
    resolved style/class/font parameters;
}
```

Commit должен только применять подготовленные данные.

Ни `GetConfiguredTheme()`, ни `LoadSynchronous()` не должны быть достижимы из Commit roots.

---

## PAH-R2 — P1 — принятого `FSessionContentSnapshot` фактически нет

### Принятое решение

Proposal/ADR определяли:

```text
FGV2SessionCoordinator
    └─ shared_ptr<const FSessionContentSnapshot>
```

с агрегированными authorities:

```text
Repository
UiSchemas
Packages
ScreenRegistry
Resources
```

Snapshot должен:

- принадлежать конкретной session;
- быть immutable;
- атомарно публиковаться после полной подготовки;
- быть единственным runtime content authority.

### Фактическая реализация

Coordinator хранит:

```text
PinnedRepository
BindingRegistry
IngressQueue
RuntimeSession
...
```

но `FSessionContentSnapshot` отсутствует.

Schemas:

```cpp
TOptional<FGV2UiSchemaCache> GSessionSchemaCache;
```

Images:

```cpp
TStrongObjectPtr<UGV2ImageResourceCatalog> GSessionImageCatalog;
```

Screen Registry хранится в `UGV2RuntimeSubsystem`.

Theme берётся через global settings.

### Почему session-scoped globals недостаточны

То, что global cache пересобирается в `StartSession` и сбрасывается в `EndSession`, лучше прежнего process-lifetime cache, но это не ownership-by-construction.

Например, при нескольких coordinator/GameInstance в одном process теоретически возможно:

```text
Session A builds global schemas/resources
Session B overwrites globals
Session A EndSession releases globals
Session B loses its authorities
```

Даже если production сейчас не создаёт такой сценарий, типовая архитектура этого не запрещает.

### Рекомендуемое исправление

Ввести реальный объект:

```text
FSessionContentSnapshot
{
    FRepositoryReadHandle Repository;
    FResolvedPackageClosure Packages;
    FCompiledUiSchemaSet UiSchemas;
    FResolvedScreenRegistry Screens;
    FResolvedResourceCatalog Resources;
    FResolvedUiTheme Theme;
}
```

Coordinator:

```text
TSharedPtr<const FSessionContentSnapshot> ActiveContent;
```

Runtime consumers получают authority только через snapshot/context, а не через statics/settings.

---

## PAH-R3 — P1 — Screen Registry использует другой package authority

### Проблема

`UGV2RuntimeSubsystem::Initialize()` делает:

```text
LoadScreenRegistry()
ResolveRepositoryPackageRoots()
Build repository
```

То есть Registry строится раньше, чем определяется exact package set repository/session.

`UGV2ScreenRegistry::Build()` самостоятельно получает package order/ownership через:

```text
GetPackageLoadOrderFromGameData()
ResolveContentRootOwnershipFromGameData()
```

А обе функции используют:

```text
GV2PackageClosure::DiscoverFromGameData()
```

То есть canonical `mods.lock` closure.

### Но session package roots могут быть другими

`ResolveRepositoryPackageRoots()` поддерживает:

```text
EditorPackageRoots
test sample override
canonical production package set
```

Следовательно возможно:

```text
Repository/Lua/Schemas/Images:
    package set A

Screen Registry:
    canonical package set B
```

### Почему это нарушение

Proposal Phase 0C прямо запрещает повторное canonical discovery:

> exact session package set должен быть один; второй discovery того же content fact является вторым authority, даже если сегодня возвращает те же данные.

### Рекомендуемое исправление

Screen Registry должен строиться не из `GameData/mods.lock` самостоятельно, а из snapshot builder input:

```text
ResolvedPackageClosure
    → package load order
    → ue_content_roots
    → screen registry validation
```

Удалить production API:

```text
GetPackageLoadOrderFromGameData()
ResolveContentRootOwnershipFromGameData()
```

из runtime registry build path.

---

## PAH-R4 — P1/P2 — nested Tabs обходят session Registry authority

### Проблема

Top-level screen resolution теперь идёт через:

```text
ResolveScreenClass
→ ScreenRegistry.Resolve(screen_id, Placement)
```

Но `FGV2TabContainerTabsPropertyConsumer::Prepare()` делает:

```cpp
const UGV2ScreenRegistry* ScreenRegistry =
    UGV2ScreenRegistrySettings::GetConfiguredRegistry();
```

То есть напрямую получает configured authoring DataAsset.

Далее имеется fallback:

```cpp
TSubclassOf<UGV2ScreenWidgetBase> TargetWidgetClass =
    UGV2ScreenWidgetBase::StaticClass();

if (ScreenRegistry != nullptr)
{
    ScreenRegistry->Resolve(...);
}
```

Если Registry отсутствует, consumer сохраняет generic base class без обязательного `Resolve`.

### Нарушение

Accepted design требовал:

> разрешённый Screen class нельзя получить без `Resolve(screen_id, Placement)`.

Текущая ветка делает именно такой bypass структурно возможным.

### Рекомендуемое исправление

Property consumer не должен знать про settings/configured registry.

Вместо:

```text
GetConfiguredRegistry()
```

должно быть:

```text
PrepareContext.ResolveScreen(screen_id, Embedded)
```

или:

```text
SessionContentSnapshot.Screens.Resolve(...)
```

Отсутствие resolver/registry:

```text
Prepare failure
```

без generic-class fallback.

---

## PAH-R5 — P1/P2 — disabled resource package влияет на session bootstrap

### Проблема

`UGV2ImageResourceCatalog::BuildFromPackageClosure()` делает:

```text
BuildFromDirectory(Resources/)
filter Entries by PackageIds
```

То есть сначала обрабатываются **все** resources дерева.

До filtering выполняются:

- resource ID parsing;
- duplicate detection;
- PNG decode;
- nine-slice marker validation;
- texture creation;
- render-mode validation.

Любая ошибка прерывает build.

### Негативный сценарий

```text
active closure:
    core + textsystem

Resources/rh/resource/broken.png
    corrupt PNG
```

`rh` отключён, но файл всё равно прочитан и декодирован до filtering.

Результат:

```text
BuildFromDirectory fails
→ RebuildForSession fails
→ active core+textsystem session fails bootstrap
```

### Почему это важно

Отключённый package не должен влиять на candidate snapshot активной session.

Фильтрация после validation не обеспечивает isolation.

### Рекомендуемое исправление

Фильтровать package namespace/root **до чтения/decode**.

Предпочтительно:

```text
for package in ResolvedPackageClosure:
    scan Resources/<package_id>/
```

а не:

```text
scan entire Resources/
then filter
```

---

## PAH-R6 — P2 — `ue_content_roots` отсутствует в package fingerprint

### Наблюдение

`ue_content_roots` теперь влияет на runtime authorization Screen assets:

```json5
ue_content_roots: [
    "/Game/core",
    "/Game/UI"
]
```

Но поле специально не входит в portable `FPackageDescriptor`.

Следовательно `ComputePackageFingerprint()` его не видит.

### Последствие

Изменение:

```text
/Game/core
→ /Game/another_root
```

изменяет presentation security semantics пакета, но не изменяет package fingerprint в `mods.lock.json5`.

### Требуется архитектурное решение

Нужно однозначно определить смысл fingerprint.

#### Вариант A

Fingerprint означает только portable gameplay/content identity.

Тогда это допустимо, но должно быть явно записано:

```text
UE presentation metadata is intentionally outside portable fingerprint
```

и желательно иметь отдельную UE-host integrity identity.

#### Вариант B

Fingerprint означает identity всего package.

Тогда `ue_content_roots` должен входить в fingerprint.

Сейчас поле находится в основном `package.json5` и меняет runtime behavior, поэтому текущее исключение требует явного ADR-level обоснования.

---

## PAH-R7 — P1/P2 — verification gates перечисляют authorities вручную

### Что сделано хорошо

PAH добавил сильные verification tools:

```text
validate_pre_ready_content_discovery.py
validate_presentation_authority_phase.py
runtime authority counter
```

У gates есть negative self-tests.

### Но structural actual-set неполный

Discovery grammar перечисляет:

```text
filesystem iterators
FindFiles*
LoadFileTo*
ifstream
Discover*
```

и не видит:

```text
TSoftObjectPtr::LoadSynchronous
settings → DataAsset resolution
```

Authority grammar перечисляет конкретные known authority functions, но не Theme.

### Фактический false green

Сегодня одновременно возможны:

```text
all PAH gates green
```

и:

```text
Commit
→ GetConfiguredTheme()
→ LoadSynchronous()
```

Это доказывает, что gate проверяет не полный инвариант.

### Рекомендуемое исправление

Не расширять regex бесконечно:

```text
|GetConfiguredTheme
|SomeFutureCatalog
|AnotherSettings
```

Это снова превращает verification в ручной whitelist.

Нужно сделать так, чтобы presentation semantic authorities были доступны только через одну boundary:

```text
FSessionContentSnapshot /
FGV2PresentationAuthority
```

Тогда gate проверяет:

```text
Commit roots cannot reach authority interface
```

вместо перечисления всех возможных authority implementations.

---

# 5. Что действительно закрыто предыдущим раундом

## 5.1 Layer ordering / reconciliation

Слои GameShell теперь проходят через общий:

```text
FGV2KeyedCollection::ReconcilePrepared
```

Предыдущий порядок сохраняется, а failure path способен восстановить exact order.

Старый `UIR-R1` можно считать закрытым.

## 5.2 Rollback observability and catastrophic recovery

`CommitReconcile` propagates rollback failure, а `Reconcile()` при marker rollback failure вызывает:

```text
PerformCatastrophicRecovery()
```

Восстановление выполняется из:

```text
LastCommittedDocument
```

а не из копии физического UMG tree.

Старый `RB-R1` закрыт по правильной архитектурной модели.

## 5.3 Asset ownership

Hardcoded C++ ownership table удалён.

`ue_content_roots`:

- package-owned;
- overlap разных packages reject-ится;
- неизвестный `/Game/` root reject-ится;
- external engine/plugin domain выделен отдельно.

Старый `PKG-R2` закрыт.

## 5.4 Image semantic re-resolution

`STATUS-012` закрыт правильно:

```text
Prepare resolves image resource
Prepared consumer stores resolution
Commit uses ApplyResolved
```

Проблема теперь не в image path, а в том, что тот же defect class остался у Theme.

---

# 6. Verification status

Архив `PresentationAuthorityHardening` фиксирует локальную проверку:

```text
UE Automation: 118/118
ctest: 82/82
gv2-headless --check-scripts: ok
validate_docs.py: green
```

Это полезный recorded evidence.

На reviewed HEAD GitHub combined status/workflow runs отсутствуют, поэтому это нельзя считать независимо видимым CI result текущего commit.

Текущие открытые `ImplementationStatus.md`:

```text
STATUS-001
STATUS-002
STATUS-003
STATUS-011
```

Новые findings этого ревью в `ImplementationStatus.md` пока не зарегистрированы.

---

# 7. Рекомендуемый corrective plan

Не стоит начинать с ещё одного набора локальных fixes.

Рекомендуемая последовательность:

## Phase A — настоящий session authority

1. Ввести `FSessionContentSnapshot`.
2. Владение snapshot — только `FGV2SessionCoordinator`.
3. В snapshot включить:
   - Repository;
   - exact resolved package closure;
   - UI schemas;
   - resolved Screen Registry;
   - image resources;
   - Theme/presentation style authority.
4. Candidate snapshot полностью строится до `Ready`.
5. Publication snapshot — atomic.

## Phase B — удалить parallel authority surfaces

Удалить runtime dependence на:

```text
GSessionSchemaCache
GSessionImageCatalog
UGV2ScreenRegistrySettings::GetConfiguredRegistry()
UGV2UiThemeSettings::GetConfiguredTheme()
GV2PackageClosure::DiscoverFromGameData()
```

из presentation runtime/Prepare/Commit paths.

Authoring/config APIs могут оставаться только bootstrap builders.

## Phase C — exact package isolation

Все package-owned presentation data строятся только из:

```text
ResolvedPackageClosure
```

Resource builder:

```text
closure packages
→ package-specific resource roots
→ parse/decode/validate
```

Disabled packages не читаются.

## Phase D — full prepared semantic values

Для каждого semantic presentation type проверить:

```text
Prepare resolves semantic meaning exactly once
Commit only applies prepared physical value
```

Особенно:

```text
Text
RichText
Theme/style
Button style
Checkbox style
Input style
Dropdown style
Popover renderer/style
```

## Phase E — verification redesign

Gate должен проверять архитектурную boundary, а не список известных authorities.

Целевое свойство:

```text
No Commit root can transitively reach FSessionContentSnapshot authority-resolution methods.
```

А bootstrap/discovery gate:

```text
No presentation runtime function reachable after Ready can reach filesystem/config/asset loading APIs.
```

Добавить mutation tests минимум для:

1. `Commit → GetConfiguredTheme()` — red;
2. `Commit → LoadSynchronous()` — red;
3. nested Tab without Registry authority — reject;
4. Editor package roots differing from canonical closure — Registry matches exact session closure;
5. corrupt resource in disabled package — active session still starts;
6. second simultaneous coordinator cannot overwrite/release first session authorities.

---

# 8. Suggested status entries

Если используется `ImplementationStatus.md`, новые подтверждённые gaps можно оформить как:

```text
STATUS-013
Presentation Theme resolution remains reachable from Commit.

STATUS-014
Accepted coordinator-owned FSessionContentSnapshot is not implemented;
presentation authorities remain split across coordinator, globals, subsystem and settings.

STATUS-015
Screen Registry package closure differs from exact session package closure.

STATUS-016
Nested screen consumer can access configured Registry directly and retains a class fallback without mandatory Resolve.

STATUS-017
Image resource catalog validates excluded packages before package-closure filtering.
```

`PAH-R6` лучше сначала решить архитектурно: это может быть либо gap, либо сознательно отдельная portable/UE fingerprint semantics.

---

# 9. Final assessment

Текущий код значительно сильнее состояния до `PresentationAuthorityHardening`.

Особенно удачны:

- ordered keyed layer reconciliation;
- exact-order rollback;
- observable rollback failure;
- catastrophic rebuild from committed document;
- fail-closed asset ownership;
- устранение повторного image-resource resolve в Commit.

Но основной root cause предыдущих серий — **множественность presentation authorities** — пока устранён не полностью.

Архивный вывод:

> presentation использует один session-owned immutable authority

не соответствует фактическому source достаточно строго.

Текущая реализация ближе к:

```text
several authorities with coordinated lifecycle
```

чем к:

```text
one session-owned immutable authority
```

Самая сильная новая finding — Theme:

> image pipeline уже был исправлен по принципу «Prepare resolves, Commit applies», но Text/Theme pipeline всё ещё повторно выводит semantic/style decision в Commit и способен дойти до `LoadSynchronous()`.

Это означает, что решение PAH-08:

> отдельный Presentation Compiler / Executor не требуется, потому что INV-P5 удерживается

нужно считать **не доказанным**, пока authority boundary не будет закрыта полностью.

При этом вводить Compiler/Executor немедленно всё ещё не требуется.

Сначала следует реализовать настоящий `FSessionContentSnapshot` и удалить parallel authority surfaces. После этого повторная проверка Prepare/Commit boundary даст уже достоверный ответ, нужен ли дополнительный compiler/executor layer.
