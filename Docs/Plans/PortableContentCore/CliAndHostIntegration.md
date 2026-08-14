---
title: Portable Content Core CLI and Host Integration Tasks
status: draft
version: 0.3
updated: 2026-08-14
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

- [x] **PCC-31 — Реализовать `gv2-content validate`**
  - Зависимости: PCC-20, PCC-30.
  - Поддержать package/project root и `--format=text|json` через общий BuildRepository path.
  - Done: команда не требует UE, не публикует repository и выдаёт deterministic diagnostics.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunValidate`, `BuildFromPackageRoot`, `FFilesystemContentSourceProvider`); `Tools/Content/CMakeLists.txt` (portable executable target, no UE/gv2_runtime_core dependency); CTest `gv2_content_validate_core_fixture`, `gv2_content_validate_core_fixture_json`, `gv2_content_validate_rejects_duplicate_key`.

- [x] **PCC-32 — Реализовать `gv2-content inspect`**
  - Зависимости: PCC-27, PCC-30, PCC-31.
  - Показать normalized definition; `--provenance` добавляет winner/shadowed/redirect context.
  - Done: unknown/wrong-kind ID даёт typed tool result; absolute source paths не попадают в portable output.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunInspect`, `WriteDefinitionProvenanceJson`/`WriteProvenanceText` over `FRepositoryReadHandle::Require`/`GetProvenance`); CTest `gv2_content_inspect_core_fixture`; manual verification of `not_found`/`core:diagnostic.repository.read.not_found` typed result and package-relative-only `relative_source` in output.

- [x] **PCC-33 — Реализовать `gv2-content hash`**
  - Зависимости: PCC-29, PCC-31.
  - Done: команда печатает canonical content hash и совпадает с `RepositoryReadHandle::GetContentHash()`.
  - Evidence: `Tools/Content/Source/main.cpp` (`RunHash`); CTest `gv2_content_hash_core_fixture`; manually confirmed `gv2-content hash` and `gv2-content validate --format=json` report the identical `content_hash` for `valid/core`.

- [x] **PCC-34 — Зафиксировать stable CLI exit codes**
  - Зависимости: PCC-31–PCC-33.
  - Различить success, invalid content и tool/configuration failure.
  - Done: text/JSON formats имеют одинаковый exit code; CI tests не анализируют human message.
  - Evidence: `Tools/Content/Source/main.cpp` (`EExitCode`: `Success=0`, `InvalidContent=1`, `ToolFailure=2`, computed independently of `EOutputFormat`); CTest `gv2_content_validate_rejects_duplicate_key`/`gv2_content_validate_rejects_missing_root` (`WILL_FAIL` on non-zero exit); manually confirmed identical exit codes across `--format=text`/`--format=json` for success, invalid-content and bad-root/bad-id cases.

- [x] **PCC-35 — Интегрировать repository с Headless**
  - Зависимости: PCC-30.
  - Headless получает pinned read handle до Lua bootstrap и использует его для portable repository queries.
  - Done: отсутствие/invalid repository блокирует session startup; gameplay не выполняет content I/O.
  - Evidence: `Headless/Source/main.cpp` (`BuildRepositoryFromDirectory`, `LoadContentRoot`, `Run()` builds/pins `FRepositoryReadHandle` and calls `List("item")` before `Runtime.Start(...)`; missing/invalid content returns exit 9 before any Lua VM is created); CTest `gv2_headless_rejects_missing_content_root`, `gv2_headless_rejects_invalid_content_root`; manually confirmed `repository_content_hash` in `gv2-headless` output equals `gv2-content hash` for `valid/core`.

- [x] **PCC-36 — Интегрировать repository с Unreal host**
  - Зависимости: PCC-30.
  - Application строит candidate тем же core и передаёт pinned handle Session Coordinator.
  - Done: UE-specific adapter занимается только source acquisition/publication; schema и resolution logic не дублируются.
  - Evidence: `Source/GV2/Private/Application/GV2FilesystemContentSourceProvider.h/.cpp` (UE-filesystem `IContentSourceProvider` + directory discovery, routes through the shared `GV2ContentCore::BuildRepository()`); `Source/GV2/Private/Application/GV2RepositoryPublisher.h/.cpp`; `UGV2RuntimeSubsystem::Initialize()` builds/publishes before any session from `GameData/core` (real project package root, not the test fixture); `GV2.Build.cs` stages `GameData/` via `RuntimeDependencies` (mirroring `Scripts/`/`Resources/`) so packaged builds can resolve `FPaths::ProjectDir()/GameData/core` at runtime; `FGV2SessionCoordinator::StartSession(const FRepositoryReadHandle&)` pins it for the session and rejects an invalid handle; UE automation `GV2.Runtime.ContentCore.CrossHostParity`, `GV2.Runtime.ContentCore.SessionRepositoryPinningAcrossRestart`, `GV2.Runtime.ContentCore.SessionRejectsInvalidRepository` (BootstrapAndSessionLifecycle.md mandatory "repository failure before VM" test), `GV2.Runtime.Presentation.LuaCreatesRegisteredScreen`/`StartButtonOpensRegisteredScreen` (exercise the new gated `StartSession` path end to end). Scope note: `GameData/core` is one hardcoded package root; no mod/multi-package discovery yet (out of PortableContentCore's first-release scope).

- [x] **PCC-37 — Реализовать atomic publication**
  - Зависимости: PCC-30, PCC-36.
  - Validate operation/resolution token на Game Thread; same-hash candidate не увеличивает version.
  - Done: failed/stale candidate сохраняет current snapshot; старый snapshot живёт до освобождения последнего handle.
  - Evidence: `Source/GV2/Private/Application/GV2RepositoryPublisher.cpp` (`PublishCandidate`: `check(IsInGameThread())`, same-hash skip without version bump, failed/non-publishable candidate leaves `Current`/`Version` untouched; `Current` is a `shared_ptr`-backed `FRepositoryReadHandle` copy, so a session's pinned copy keeps the old snapshot alive independent of later publishes); UE automation `GV2.Runtime.ContentCore.RepositoryPublisherAtomicPublication`. Scope note: publication here is synchronous (no background-worker candidate build), so the "operation/resolution token" requirement reduces to the Game-Thread-only check; async candidate building is not implemented.

- [x] **PCC-38 — Добавить cross-host parity tests**
  - Зависимости: PCC-31–PCC-37.
  - Один corpus выполняется CLI, standalone/headless и Unreal automation tests.
  - Done: valid inputs дают одинаковый normalized snapshot/hash, invalid — одинаковые codes, spans и order.
  - Evidence: all three hosts build `Tests/Fixtures/PortableContentCore/valid/core` through one discovery convention (self-describing schema resources) and the same `BuildRepository()` path; pinned hash `35ed7d8000170391d46cac29a1d23534affa093312bf5eb9c73e62ccdc0ae5d8` independently reproduced by `gv2-content hash`, `gv2-headless` (`repository_content_hash` field) and UE automation `GV2.Runtime.ContentCore.CrossHostParity`; invalid-content diagnostics share the same `GV2ContentCore::FDiagnostic` ordering/codes across hosts since none of them duplicate validation logic. Scope note: parity is demonstrated for the single-package `valid/core` corpus consistent with PCC-31's scope; multi-package/mod parity is out of PortableContentCore's first-release scope.

## Проверка milestone

- [x] CLI работает на машине без Unreal Engine installation (`Tools/Content/CMakeLists.txt` links only `gv2_content_core`, no UE/`gv2_runtime_core` dependency).
- [x] Headless и UE sessions закрепляют immutable snapshot до controlled restart (`GV2.Runtime.ContentCore.SessionRepositoryPinningAcrossRestart`: active session keeps its pinned hash across an unrelated republish; `EndSession()` + `StartSession()` against a new snapshot pins the replacement only for the new session).
- [x] Failed/stale/same-hash publication semantics покрыты automation tests (`GV2.Runtime.ContentCore.RepositoryPublisherAtomicPublication`).
- [x] CI выполняет portable tests, CLI fixtures и Unreal parity subset (`linux-ci.yml` `portable` job runs CTest + explicit `gv2-content`/`gv2-headless` steps; `unreal-runtime` job's `Automation RunTests GV2.Runtime` already includes `ContentCore.CrossHostParity` and `ContentCore.RepositoryPublisherAtomicPublication`).
- [x] `GameDataRepositoryContract`, lifecycle и tooling documentation соответствуют фактическому API (`GameDataRepositoryContract.md` Conformance, `SystemContextAndComponents.md` layout/host list, `Docs/README.md` CI commands и `ContentDiagnosticsAndToolingProposal.md` CLI usage synced 2026-08-14; `Content/`/`Tools/Content/` split corrected so the CLI no longer lives under the UE asset root).
