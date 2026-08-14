---
title: Portable Content Core CLI and Host Integration Tasks
status: draft
version: 0.1
updated: 2026-08-13
depends_on:
  - RepositoryResolution.md
  - ../../Proposals/ContentDiagnosticsAndToolingProposal.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/HeadlessSimulationContract.md
decisions:
  - ../../ADR/0006-repository-reload-and-session-pinning.md
  - ../../ADR/0010-portable-runtime-and-headless-simulation.md
---

# M5 — CLI и интеграция host-ов

## Результат этапа

CLI, Headless и UE используют один `GV2ContentCore` и общие fixtures. Из одинаковых inputs они получают одинаковый snapshot/hash либо одинаковый ordered diagnostic set. Публикация candidate остаётся обязанностью host Application.

## Задачи

- [ ] **PCC-31 — Реализовать `gv2-content validate`**
  - Зависимости: PCC-20, PCC-30.
  - Поддержать package/project root и `--format=text|json` через общий BuildRepository path.
  - Done: команда не требует UE, не публикует repository и выдаёт deterministic diagnostics.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-32 — Реализовать `gv2-content inspect`**
  - Зависимости: PCC-27, PCC-30, PCC-31.
  - Показать normalized definition; `--provenance` добавляет winner/shadowed/redirect context.
  - Done: unknown/wrong-kind ID даёт typed tool result; absolute source paths не попадают в portable output.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-33 — Реализовать `gv2-content hash`**
  - Зависимости: PCC-29, PCC-31.
  - Done: команда печатает canonical content hash и совпадает с `RepositoryReadHandle::GetContentHash()`.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-34 — Зафиксировать stable CLI exit codes**
  - Зависимости: PCC-31–PCC-33.
  - Различить success, invalid content и tool/configuration failure.
  - Done: text/JSON formats имеют одинаковый exit code; CI tests не анализируют human message.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-35 — Интегрировать repository с Headless**
  - Зависимости: PCC-30.
  - Headless получает pinned read handle до Lua bootstrap и использует его для portable repository queries.
  - Done: отсутствие/invalid repository блокирует session startup; gameplay не выполняет content I/O.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-36 — Интегрировать repository с Unreal host**
  - Зависимости: PCC-30.
  - Application строит candidate тем же core и передаёт pinned handle Session Coordinator.
  - Done: UE-specific adapter занимается только source acquisition/publication; schema и resolution logic не дублируются.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-37 — Реализовать atomic publication**
  - Зависимости: PCC-30, PCC-36.
  - Validate operation/resolution token на Game Thread; same-hash candidate не увеличивает version.
  - Done: failed/stale candidate сохраняет current snapshot; старый snapshot живёт до освобождения последнего handle.
  - Evidence: <!-- tests/commit/PR -->

- [ ] **PCC-38 — Добавить cross-host parity tests**
  - Зависимости: PCC-31–PCC-37.
  - Один corpus выполняется CLI, standalone/headless и Unreal automation tests.
  - Done: valid inputs дают одинаковый normalized snapshot/hash, invalid — одинаковые codes, spans и order.
  - Evidence: <!-- tests/commit/PR -->

## Проверка milestone

- [ ] CLI работает на машине без Unreal Engine installation.
- [ ] Headless и UE sessions закрепляют immutable snapshot до controlled restart.
- [ ] Failed/stale/same-hash publication semantics покрыты automation tests.
- [ ] CI выполняет portable tests, CLI fixtures и Unreal parity subset.
- [ ] `GameDataRepositoryContract`, lifecycle и tooling documentation соответствуют фактическому API.
