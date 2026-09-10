---
title: Presentation Structural Closure Plan
status: active
version: 2.3
updated: 2026-09-10
depends_on:
  - ../../Proposals/PresentationAuthorityStructuralClosureProposal.md
  - ../../Status/AuditFindings.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/HeadlessSimulationContract.md
  - ../../UI/UIDocumentAndReconciliation.md
decisions:
  - ../../ADR/0043-presentation-apply-boundary.md
  - ../../ADR/0042-presentation-authority-and-publication.md
  - ../../ADR/0018-portable-content-core-module.md
---

# План структурного замыкания презентации

> **Материализует:** [ADR-0043](../../ADR/0043-presentation-apply-boundary.md) и [предложение о структурном замыкании](../../Proposals/PresentationAuthorityStructuralClosureProposal.md), через них — семь находок `PAH-R1…R7` [аудита](../../Status/AuditFindings.md).
> **Задачи:** PSC-01…14; `PSC-09` разделена на `09A`/`09B`, `PSC-10` — на `10A`/`10B`/`10C`.
> **Результат:** применение получает только самодостаточную подготовленную транзакцию; обращение к content/settings authority из Apply невозможно по dependency direction, а не по соглашению.
> **Исполнение:** задачи выполняются последовательно по критическому пути; перед реализацией использовать `superpowers:executing-plans`. Параллельная правка общей C++/UAsset surface запрещена.

## Цель и подход

План заменяет открытые множества и рукописные списки структурными границами:

- один portable `FResolvedPackageSet` служит входом repository, Lua и UE presentation build;
- coordinator строит private candidate и публикует один immutable `FGV2SessionContentSnapshot` только вместе с готовой сессией;
- верхний `GV2` выполняет semantic resolution через snapshot, нижний `GV2PresentationApply` применяет resolved DTO;
- Headless использует portable package set, но не линкует Unreal или presentation;
- build graph, exhaustive operation kind и inventories из фактических declarations являются первичными перечислителями.

Технологии: C++20 portable libraries, Unreal Engine 5 modules, UMG/CommonUI/Slate, CMake/CTest, UE Automation и Unreal Editor API для миграции ассетов.

## Подтверждённое состояние на входе

| Находка | Наблюдаемое нарушение |
|---|---|
| `PAH-R1` | `Commit → ApplyText → ResolveStyleClass → GetConfiguredTheme()` повторно разрешает Theme при применении |
| `PAH-R2` | schemas, images, screens, Theme, Lua sources и GameShell не объединены одним snapshot owner |
| `PAH-R3` | Screen Registry сам повторно обнаруживает canonical package closure |
| `PAH-R4` | Nested Tabs читают configured Registry и допускают generic class fallback |
| `PAH-R5` | Image Catalog читает и декодирует disabled packages до фильтрации |
| `PAH-R6` | `ue_content_roots` меняет authorization, но не package fingerprint |
| `PAH-R7` | authority/discovery gates используют открытые множества имён и дали false green |

| Finding | Задачи закрытия |
|---|---|
| `PAH-R1` | `PSC-04`, `PSC-06`, `PSC-09B`, `PSC-10A`, `PSC-10B`, `PSC-11`, `PSC-13` |
| `PAH-R2` | `PSC-04…06`, `PSC-13` |
| `PAH-R3` | `PSC-02`, `PSC-04`, `PSC-13` |
| `PAH-R4` | `PSC-08`, `PSC-10A`, `PSC-13` |
| `PAH-R5` | `PSC-07`, `PSC-13` |
| `PAH-R6` | `PSC-03`, `PSC-13` |
| `PAH-R7` | `PSC-09A`, `PSC-09B`, `PSC-10A`, `PSC-10B`, `PSC-11`, `PSC-12`, `PSC-13` |

## Зафиксированные интерфейсы

```text
GV2ContentHostSupport::FResolvedPackageSet
└─ ordered FResolvedPackageSource[]
   ├─ package root
   ├─ immutable FPackageDescriptor
   └─ canonical manifest hash

FGV2SessionContentSnapshot
├─ FRepositoryReadHandle
├─ ordered package identities
├─ loaded Lua source set + script_set_hash
├─ eagerly compiled UI schemas
├─ resolved Screen Registry / Image Catalog / Theme / GameShell
├─ GC-safe asset pins
├─ repository_content_hash / ordered package_fingerprints / presentation_hash
└─ session_content_id

GV2 semantic Prepare
  + FGV2PresentationPrepareContext(snapshot)
  → FGV2PreparedPresentationTransaction
  → GV2PresentationApply::Apply(transaction)
  → physical UMG projection
```

`FGV2PreparedPresentationTransaction` может сохранять Stable ID только как identity/diagnostic metadata. Если ID потребовал lookup, resolved payload обязан находиться в той же операции. Apply не получает snapshot, repository, package set, registry, Theme source или prepare context.

## Milestones

- [x] M0 — [Contract Alignment](ContractAlignment.md): owner contracts отражают уже принятый ADR до изменения кода. PSC-01. (2026-09-07)
- [x] M1 — [Package Set](PackageSet.md): exact package set и полный canonical manifest hash. PSC-02…03. (2026-09-08)
- [x] M2 — [Snapshot](Snapshot.md): полный candidate/snapshot, atomic publication, recovery и snapshot-backed PrepareContext. PSC-04…08. (2026-09-08)
- [x] M3 — [Self-Contained Payload](Payload.md): установлена типовая граница, весь путь заведён через транзакцию, замкнут resolved payload видов операций, центральной стилизации и image resource. PSC-09A…09B, PSC-10A…10C. (2026-09-09)
- [x] M4 — [Apply Boundary](ApplyBoundary.md): физическое применение вынесено в нижний модуль, затем атомарно мигрированы `UCLASS` paths. PSC-11…12. (2026-09-10)
- [ ] M5 — [Structural Gates and Closure](GatesAndClosure.md): механические перечислители, cross-host verification и двухкоммитная архивация. PSC-13…14.

## Критический путь

```text
PSC-01✔ → PSC-02✔ → PSC-03✔ → PSC-04✔ → PSC-05✔ → PSC-06✔
       → PSC-07✔ → PSC-08✔ → PSC-09A✔ → PSC-09B✔ → PSC-10A✔
       → PSC-10B✔ → PSC-10C✔ → PSC-11✔ → PSC-12✔ → PSC-13 → PSC-14
```

- `PSC-04` начинается только после exact package set и manifest identity: snapshot нельзя строить из старого canonical rediscovery.
- `PSC-09A` зависит от `PSC-06` (контекст подготовки) и `PSC-08` (разрешение экрана), но не от `PSC-07`: фильтрация ресурсов отключённых пакетов не влияет на разделение Prepare/Apply. Порядок исполнения последователен по общему ограничению плана, а не по этой связи.
- `PSC-09A`/`PSC-09B`/`PSC-10A`/`PSC-10B`/`PSC-10C` предшествуют физическому переносу: текущие `IGV2PropertyConsumer`, `UGV2TextPipeline`, central style path и image resource path смешивают Prepare и Commit, поэтому нижний модуль без предварительного DTO boundary не может быть независимым.
- `PSC-10C` стоит между `PSC-10B` и `PSC-11` не по объёму, а по предпосылке: `PSC-11` требует, чтобы ни один переносимый класс не достигал авторитета, а `UGV2ImageWidgetBase::NativePreConstruct()` достигает его через `GetSessionCatalog()`. Пока это так, переносить класс в модуль, где catalog недостижим по построению, нельзя.
- `PSC-11` не меняет ни одного `/Script/GV2` path. Все Widget Blueprint продолжают ссылаться на прежние классы; верхние `UCLASS` объявляют value-only ролевые интерфейсы нижнего модуля, через которые единственный façade достаёт их, не называя их типов.
- `PSC-12` одним change set переносит `UCLASS`, мигрирует все найденные Asset Registry ассеты и удаляет временные redirects. Промежуточное сломанное дерево не фиксируется.

## Владение файлами

| Область | Задачи |
|---|---|
| Owner contracts и routers | `PSC-01`, затем синхронно соответствующие code tasks |
| `GV2ContentHostSupport`, package discovery, `ModsLock` | `PSC-02`, `PSC-03` |
| `GV2SessionCoordinator`, `GV2RuntimeSubsystem`, snapshot builder и PrepareContext | `PSC-04…06` |
| `GV2ImageResourceCatalog` | `PSC-07` |
| `GV2ScreenRegistry`, nested screen preparation | `PSC-08` |
| Prepared DTO, property consumers, text/image/screen resolution | `PSC-09A`, `PSC-09B`, `PSC-10A` |
| `IGV2UiStyleConsumer` и фактическое множество реализаций `ApplyCentralStyle` | `PSC-10B` |
| `FGV2ImagePresentation`, `GetSessionCatalog()` и widget lifecycle image resolution | `PSC-10C` |
| `GV2PresentationApply`, `*.Build.cs`, physical apply | `PSC-11`, `PSC-13` |
| Widget `UCLASS`, `Content/**`, временные Core Redirects | `PSC-12` |
| Audit/proposal/plan archive records и indexes | `PSC-14` |

## Общие ограничения

1. Lua gameplay, Command/Event path, canonical state и authoring grammar не меняются.
2. Universal UI Property Pipeline остаётся единственным pipeline; второй UI framework или schema-specific DTO запрещены.
3. Headless не получает UE/presentation dependency и не загружает media.
4. Apply-модуль может зависеть только от `Core`, `CoreUObject`, `Engine`, `UMG`, `CommonUI`, `Slate`, `SlateCore`.
5. Apply-модулю запрещены зависимости на `GV2`, `GV2ContentHostSupport`, `DeveloperSettings`, `AssetRegistry`, `ImageCore`, filesystem/content authoring modules.
6. Каждый task закрывается только после red-on-revert evidence через production path; ручной перечень является reminder, не доказательством.
7. Для каждого универсального утверждения назван перечислитель из compiler, enum/variant, declaration inventory, implementation set или module/source graph.
8. Ассеты меняются только через `unreal-mcp`; единственная задача с UAsset mutation — `PSC-12`.
9. Нормативные документы обновляются в том же code change set; полностью реализованная возможность не добавляется в `ImplementationStatus`.

## Итоговый Definition of Done

- [x] Contracts описывают target ownership, lifecycle, failure semantics, module direction и Headless identity до начала реализации. (`PSC-01`, 2026-09-07)
- [x] Один `FResolvedPackageSet` строится до всех consumers; repository, Lua и presentation не выполняют повторный package discovery. (`PSC-02`, 2026-09-08)
- [x] Canonical hash полного manifest входит в package fingerprint; `ue_content_roots` меняет fingerprint, но не Headless run digest. (`PSC-03`, 2026-09-08)
- [x] Полный `FGV2SessionContentSnapshot` содержит все поля из зафиксированного интерфейса и не копирует definitions/provenance. (`PSC-04`, 2026-09-08)
- [x] Candidate остаётся private; active snapshot публикуется атомарно с успешным initial Commit/`Ready`; failure и recovery не наблюдают частичный snapshot. (`PSC-05`, 2026-09-08)
- [x] Runtime authorities принадлежат snapshot, а semantic Prepare получает их через explicit PrepareContext; legacy Apply accessors удаляются вместе с resolved payload обоих путей. (`PSC-06`, `PSC-10A`, `PSC-10B`, 2026-09-09 — `GetConfiguredTheme()`/`GetConfiguredRegistry()` физически удалены, PrepareContext обязателен на пути реконсиляции)
- [x] Disabled package не обходится, не читается и не декодируется presentation builders. (`PSC-07`, 2026-09-08)
- [x] Top-level и nested screen разрешаются одним PrepareContext без generic fallback. (`PSC-08`, 2026-09-08)
- [x] Prepare и Apply разделены типами; lower-facing DTO существующего property/text pipeline не содержит authority capability. (`PSC-09A` ввела границу на image resource, `PSC-09B` распространила её на фактическое множество `IGV2PropertyConsumer` kinds и `UGV2TextPipeline`, 2026-09-08)
- [x] Каждый operation kind существующего property/text pipeline проходит через транзакцию. Central style пока остаётся отдельным runtime-путём и явно принадлежит `PSC-10B`. (`PSC-09B`, 2026-09-08 — source-derived coverage/field-inventory gates)
- [x] Каждый operation kind несёт resolved payload; viewport calculation использует prepared policy, а не Theme lookup. (`PSC-10A`, 2026-09-08 — единый enum/variant, font/scale policy как pure function, PrepareContext прокинут до BuildFields, exhaustive kind-walk test)
- [x] Центральная стилизация входит в ту же prepared transaction и не читает тему в рантайме; `GetConfiguredTheme()`/`GetConfiguredRegistry()` отсутствуют без исключений. `GetCoreMinimalTheme()` разрешён двум structurally различным ролям — UE-native cold-start recovery (у которого snapshot отсутствует по определению) и bootstrap-разрешению самого snapshot, пришпиливающему минимальную тему как текстовый fallback сессии, — и запрещён остальным production paths. (`PSC-10B`, 2026-09-09 — role/variant central style, обязательный PrepareContext, derived-set boundary gate)
- [x] Image resource разрешается только на стороне Prepare; widget lifecycle не консультирует catalog и не мутирует brush по `resource_id`. (`PSC-10C`, 2026-09-09 — process-global каталог сессии удалён, безусловное правило гейта на lifecycle-колбэки, red-on-revert двумя детекторами)
- [x] `GV2PresentationApply` содержит единственную public transaction Apply entry point, всю физическую часть Commit/rollback/reconciliation и восстановление проекции; решение, что писать и что откатывать, остаётся выше, потому что читает `FGV2PreparedUiObject` и compiled schema — типы, запрещённые нижнему модулю графом сборки. Dependency и forbidden-capability gates отвергают нарушения. (`PSC-11`, 2026-09-09)
- [x] Все Widget Blueprint загружены, скомпилированы и пересохранены после class-path migration; старые paths и временные redirects отсутствуют. (`PSC-12`, 2026-09-10 — 46/46 clean compile, 17 affected assets в migration commit)
- [ ] Compiler/type/module/source enumerators и production scenarios закрывают `PAH-R1…R7`; Headless link graph остаётся UE-free. (`PSC-13`)
- [ ] Полная verification зелёная, каждый finding имеет исход, active audit и plan готовы к обязательной post-completion архивации. (`PSC-14`)
