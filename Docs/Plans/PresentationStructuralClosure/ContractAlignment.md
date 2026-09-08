---
title: Contract Alignment Task
status: active
version: 1.1
updated: 2026-09-07
depends_on:
  - README.md
  - ../../Architecture/SystemContextAndComponents.md
  - ../../Architecture/DependencyMap.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/HeadlessSimulationContract.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../UI/WidgetRegistry.md
decisions:
  - ../../ADR/0043-presentation-apply-boundary.md
---

# M0 — Contract Alignment

> **Материализует:** уже принятое решение `ADR-0043` в owner contracts до изменения реализации.
> **Задачи:** PSC-01.
> **Результат:** план не исполняется против устаревших нормативных правил.

## Задачи

- [x] **PSC-01 — Синхронизировать owner contracts**
  - Инвариант: accepted ADR фиксирует решение, а subsystem contract содержит его актуальное полное правило. Сейчас Bootstrap всё ещё загружает configured Screen Registry до session, хотя `ADR-0043` передаёт resolved registry coordinator-owned snapshot.
  - Не считается закрытием: пересказ ADR в плане; ссылка на proposal без изменения owner contracts; описание только happy path без failure/recovery и Headless.
  - Done:
    - `SystemContextAndComponents.md` фиксирует `GV2PresentationApply`, `FResolvedPackageSet`, snapshot owner и направление зависимостей;
    - `DependencyMap.md` фиксирует точный allowlist/denylist Apply-модуля и отсутствие presentation edge у Headless/CMake targets;
    - `BootstrapAndSessionLifecycle.md` фиксирует candidate build order, private use до `Ready`, atomic publication, one-VM replacement и разные исходы failure до/после teardown;
    - `HeadlessSimulationContract.md` разделяет `package_fingerprint`, `repository_content_hash`, `presentation_hash`, `session_content_id` и run digest;
    - `UIDocumentAndReconciliation.md` фиксирует единственный PrepareContext, self-contained transaction и одну Apply entry point;
    - `WidgetRegistry.md`, `ScreenTemplates.md` и `ImageResources.md` больше не называют configured Registry/Theme runtime authority и согласованы с resolved snapshot semantics;
    - `Docs/README.md`, `Architecture/README.md`, `UI/README.md` обновлены только если новый owner route действительно нужен;
    - `validate_docs.py` проходит, а targeted search не находит противоречащих target rule нормативных формулировок.
  - Evidence: перечисленные contracts, [ADR-0043](../../ADR/0043-presentation-apply-boundary.md), `python3 Tools/Documentation/validate_docs.py`.
  - **Реализация (2026-09-07):** девять owner contracts синхронизированы с целевым правилом `ADR-0043`, без единой правки кода — расхождение с фактической реализацией остаётся описано только в [AuditFindings](../../Status/AuditFindings.md) (`PAH-R1…R7`), а не ослабляет сам contract.

    `SystemContextAndComponents.md`: `GV2PresentationApply` добавлен как отдельный logical/physical module (`Core`/`CoreUObject`/`Engine`/`UMG`/`CommonUI`/`Slate`/`SlateCore` — единственный allowlist; explicit denylist `GV2`/`GV2ContentCore`/`GV2ContentHostSupport`/`GV2RuntimeCore`/`DeveloperSettings`/`AssetRegistry`/`ImageCore`); `GV2ContentHostSupport::FResolvedPackageSet` зафиксирован как единый portable вход repository/Lua/UE-presentation build; `FGV2SessionCoordinator`'s bullet переписан с «pinned repository handle... latest accepted Presentation Snapshot» на владение одним `FGV2SessionContentSnapshot`, агрегирующим repository handle, resolved package set, UI-схемы, Screen Registry, Image Catalog, Theme и GameShell.

    `DependencyMap.md`: направление `UE Presentation (semantic) → UE Presentation (apply) → голый Unreal Engine` добавлено в ASCII-диаграмму; новая строка запрета зависимостей и новая строка физических модулей фиксируют allowlist/denylist `GV2PresentationApply` текстом, отдельным от диаграммы (два независимых представления одного правила).

    `BootstrapAndSessionLifecycle.md`: абзац, описывавший Screen Registry как загружаемый `UGV2RuntimeSubsystem::Initialize()` ДО открытия session (буквальная формулировка `PAH-R3`), заменён целевым правилом — Screen Registry/Image Catalog/UI-схемы/Theme/GameShell строятся вместе как один candidate `FGV2SessionContentSnapshot` внутри `StartSession()`, из одного `FResolvedPackageSet`; явно помечено `**Целевое правило**`, чтобы не выдавать его за уже реализованное поведение. Шаги `Cold start` и `New/load session build` переупорядочены: package resolution теперь предшествует catalog/registry построению (было наоборот), а публикация snapshot явно привязана к моменту commit `Ready`.

    `HeadlessSimulationContract.md`: новая таблица различает `package_fingerprint` (portable, per-package, включает `ue_content_roots` — закрывает `PAH-R6`), `repository_content_hash` (portable, часть run digest), `presentation_hash` и `session_content_id` (UE-only, часть snapshot, run digest не видит) — явно записано, что Headless не строит presentation и не может видеть presentation-only identity, поэтому run digest от неё структурно не зависит.

    `UIDocumentAndReconciliation.md`: новый раздел «Apply boundary» описывает целевое разделение `GV2` (semantic Prepare через один `FGV2PresentationPrepareContext`) и `GV2PresentationApply` (единственная entry point `Apply(transaction)`), явно отмечая, что раздел выше по-прежнему описывает СЕГОДНЯШНЮЮ реализацию (`FGV2LayeredUiReconciler` в `GV2`), а не подменяет её задним числом.

    `WidgetRegistry.md`: Theme-абзац и шаги `UGV2TextPipeline` переписаны, чтобы явно отделить semantic resolution (шаги 1–4, Prepare) от Apply (шаг 5) — `GetConfiguredTheme()`/`LoadSynchronous()` названы недостижимыми из Apply-фазы целевым правилом, а не тем, что уже гарантировано.

    `ScreenTemplates.md`: добавлены два `**Целевое правило**`-абзаца — Screen Registry строится из session package set внутри `StartSession()`, не из `Initialize()`-time canonical discovery (`PAH-R3`); вложенный `screen_id` резолвится тем же `PrepareContext`, что и top-level, без generic-class fallback при отсутствующем Registry (`PAH-R4`).

    `ImageResources.md`: `ResolveAndApply`/`ApplyResolved` описаны как два разных пути с разной легитимностью (Prepare/статическая composition vs. документная Commit-транзакция) вместо одного «единственного runtime path», которым `ResolveAndApply` был назван раньше буквально; добавлено целевое правило `PAH-R5` (фильтрация по замыканию до чтения/decode, per-package root, не сканирование всего дерева с последующим отбрасыванием).

    `Docs/UI/README.md`: абзац, называвший `GetConfiguredTheme()` рантайм-механизмом разрешения активной темы, переписан — резолюция происходит один раз при построении snapshot; `UGV2RecoveryScreenWidget`'s минимальная тема ядра явно выделена как отдельный bootstrap/failure-time fallback (у него по определению нет snapshot, а не потому что он читает Theme тем же путём).

    `Docs/README.md` и `Architecture/README.md` НЕ изменены: оба чисто навигационные и не содержат противоречащих target rule нормативных claims (проверено `grep` по `GetConfigured*`/`GSession*`) — новый owner route для них не требовался.

    Targeted search (`grep -rn "GetConfiguredTheme\|GetConfiguredRegistry\|GSessionSchemaCache\|GSessionImageCatalog"` по `Docs/Architecture/` и `Docs/UI/`) после правок не находит противоречащих формулировок вне уже приведённых в соответствие восьми contracts; оставшиеся упоминания живут в `Docs/ADR/0042-*.md`/`0043-*.md`, `Docs/Status/AuditFindings.md` и `Docs/Proposals/` — исторический/диагностический record, не owner contract, вне области этой задачи.

    Верификация: `python3 Tools/Documentation/validate_docs.py` — 185 Markdown files, зелёный. Код не тронут (PSC-01 — documentation-only задача); полная UE/CTest верификация не требуется и не запускалась.

## Проверка milestone

- [x] Каждый target rule имеет один owner contract. (PSC-01, 2026-09-07)
- [x] Accepted ADR и contracts не расходятся по ownership, publication, recovery, module direction и Headless. (PSC-01, 2026-09-07)
- [x] Реализация ещё может отставать и остаётся отражена только в Audit/Status, а не ослаблением contract. (PSC-01, 2026-09-07 — каждое изменение выше явно помечено «Целевое правило» там, где текущий код ещё не соответствует)
