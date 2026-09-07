---
title: Contract Alignment Task
status: active
version: 1.0
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

- [ ] **PSC-01 — Синхронизировать owner contracts**
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

## Проверка milestone

- [ ] Каждый target rule имеет один owner contract.
- [ ] Accepted ADR и contracts не расходятся по ownership, publication, recovery, module direction и Headless.
- [ ] Реализация ещё может отставать и остаётся отражена только в Audit/Status, а не ослаблением contract.
