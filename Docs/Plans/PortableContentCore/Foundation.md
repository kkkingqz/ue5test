---
title: Portable Content Core Foundation Tasks
status: draft
version: 0.3
updated: 2026-08-13
depends_on:
  - README.md
  - ../../Architecture/StableIDSpecification.md
  - ../../Architecture/SystemContextAndComponents.md
decisions:
  - ../../ADR/0018-portable-content-core-module.md
---

# M1 — Foundation

## Результат этапа

Пустой core package проходит через `BuildRepository()` и создаёт пустой immutable candidate. Shared implementation target собирается CMake и UBT без Unreal headers, Lua API и filesystem ownership внутри core. Тонкая UBT module-bootstrap translation unit может иметь private `Core` dependency согласно [System Context and Components](../../Architecture/SystemContextAndComponents.md).

## Shared fixture corpus

PCC fixtures имеют один canonical root `Tests/Fixtures/PortableContentCore`. `fixtures.index` является bytewise-sorted path-only inventory для test harness и не вводит package manifest или Content Core input grammar. CMake экспортирует root как `GV2_PCC_FIXTURE_ROOT`; Unreal automation разрешает тот же каталог через `FPaths::ProjectDir()` и не содержит отдельной копии inputs.

Corpus уже содержит empty-core fixture для M1, representative `core`, additive/full-override `test_mod`, duplicate JSON key и broken winning override. Expected diagnostic codes, normalized snapshot и content hash запрещено фиксировать в PCC-01: соответствующие golden results добавляются только задачами, которые реализуют их semantics.

## Задачи

- [x] **PCC-01 — Зафиксировать MVP fixtures**
  - Зависимости: нет.
  - Создать минимальные core/test-mod и valid/invalid inputs, которые будут постепенно наполняться следующими этапами.
  - Зафиксировать expected stable diagnostic codes и ожидаемые snapshot/hash только после появления соответствующих возможностей.
  - Done: fixtures доступны обоим build systems и не дублируются между UE и standalone tests.
  - Evidence: canonical `Tests/Fixtures/PortableContentCore/fixtures.index`; CTest `pcc_shared_fixture_contract`; Unreal automation `GV2.Runtime.ContentCore.SharedFixtureCorpus`.

- [x] **PCC-02 — Создать target `GV2ContentCore`**
  - Зависимости: PCC-01.
  - Добавить один portable source set в CMake и UBT.
  - Done: target собирается без Unreal headers/libraries; оба host-а используют одни и те же исходники.
  - Evidence: CMake static library `gv2_content_core` in `Headless/CMakeLists.txt`; UBT module `GV2ContentCore` in `Source/GV2ContentCore/GV2ContentCore.Build.cs`; compiled cleanly in CMake and UBT.

- [x] **PCC-03 — Добавить portable value model**
  - Зависимости: PCC-02.
  - Поддержать null, bool, int64, finite double, UTF-8 string, array и object с явным ownership.
  - Done: unit tests подтверждают copy/move, nested values, int64/double distinction и отсутствие UObject/Lua types в public API.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/Value.h`, `Source/GV2ContentCore/Private/Value.cpp`; storage использует ownership-safe `std::variant`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.ValueModel`.

- [x] **PCC-04 — Добавить diagnostic model**
  - Зависимости: PCC-02, PCC-03.
  - Поля: stable diagnostic code, severity, package ID, relative source, span, optional definition/schema identity и JSON pointer.
  - Done: absent context не кодируется пустыми placeholders; comparator выдаёт deterministic order.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/Diagnostic.h`, `Source/GV2ContentCore/Private/Diagnostic.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.DiagnosticModel` (Passed).

- [x] **PCC-05 — Определить `BuildResult` API**
  - Зависимости: PCC-03, PCC-04.
  - Результат содержит либо complete candidate, либо ordered diagnostics, но не оба одновременно.
  - Done: impossible partial-success states не представлены public API; invalid build не раскрывает candidate.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/BuildResult.h`, `Source/GV2ContentCore/Private/BuildResult.cpp`, `Source/GV2ContentCore/Public/GV2ContentCore/RepositoryBuilder.h`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.BuildResultAPI`.

- [x] **PCC-06 — Определить resolved package descriptor**
  - Зависимости: PCC-03, PCC-04.
  - Минимальные поля: `package_id`, namespace, `load_index`, immutable relative sources и exact schema bindings.
  - Done: descriptor не обнаруживает файлы и не вычисляет mod order; invalid duplicate IDs/load indexes и missing core дают typed diagnostics.
  - Evidence: `Source/GV2ContentCore/Public/GV2ContentCore/PackageDescriptor.h`, `Source/GV2ContentCore/Private/PackageDescriptor.cpp`; CTest `gv2_headless_self_test`; Unreal Automation Test `GV2.Runtime.ContentCore.PackageDescriptorValidation` (Passed).

## Проверка milestone

- [x] Portable library успешно собирается CMake.
- [x] Portable library успешно собирается UBT.
- [x] Portable public/implementation sources не содержат Unreal, Lua и platform filesystem types; UE module bootstrap изолирован.
- [x] Empty core descriptor создаёт empty candidate; invalid descriptor возвращает только diagnostics.
- [x] Основные команды проверки добавлены в CI или имеют локальный documented equivalent.
