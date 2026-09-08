---
title: Apply Boundary Tasks
status: active
version: 1.3
updated: 2026-09-07
depends_on:
  - README.md
  - Payload.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# M4 — Apply Boundary

> **Материализует:** `D2/D4` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md).
> **Задачи:** PSC-11…12.
> **Результат:** физическое применение находится в нижнем модуле с одной entry point; последующая `UCLASS`-миграция не оставляет сломанного commit.

## Физическая граница

`GV2PresentationApply` содержит resolved DTO, prepared operations, Commit/Reset/Rollback, keyed reconciliation, projection recovery, physical widget bases и pure layout/viewport calculations.

Разрешённые module dependencies:

```text
Core, CoreUObject, Engine, UMG, CommonUI, Slate, SlateCore
```

Запрещённые dependencies/capabilities:

```text
GV2, GV2ContentHostSupport, DeveloperSettings, AssetRegistry, ImageCore,
content authoring modules, filesystem/config discovery, soft/synchronous loading
```

Build graph доказывает отсутствие project authority types. Поскольку `CoreUObject/Engine` сами предоставляют soft-loading API, отдельный source-tree gate запрещает эти capabilities внутри модуля и честно остаётся вторичным к module boundary.

## Задачи

- [ ] **PSC-11 — Завершить `GV2PresentationApply` и запретить обратную зависимость**
  - Зависимости: PSC-10A, PSC-10B.
  - Инвариант: Apply получает только `FGV2PreparedPresentationTransaction`; новый authority с любым именем недоступен lower module по dependency direction.
  - Не считается закрытием: соглашение без `Build.cs`; несколько public apply paths; вызов upper callback; сохранение Commit/rollback логики в thin adapters; утверждение, что module graph сам запрещает `LoadSynchronous()`.
  - Done:
    - `Source/GV2PresentationApply/GV2PresentationApply.Build.cs` использует точный allowlist выше и не имеет conditional authority/editor dependencies;
    - одна public façade `FGV2PresentationApply::Apply(FGV2PreparedPresentationTransaction&, FGV2PresentationApplyResult&)` является единственной production entry point применения целой transaction;
    - Commit, Reset, rollback, keyed reconciliation, physical projection recovery и pure viewport/layout calculations реализованы ниже façade;
    - верхний `GV2` выполняет semantic Prepare и одним вызовом передаёт готовую transaction; временные методы существующих `UCLASS` только делегируют и не содержат mutation/lookup logic;
    - transitive include closure будущих moved widget bases классифицировано механически: authority-free value/physical interfaces принадлежат lower module, authority-aware types остаются в `GV2` за DTO boundary, duplicate bridge types отсутствуют;
    - exported-public inventory модуля выводится из его `Public/` declarations и классифицирует façade, DTO/results и локальные widget/lifecycle methods; вторая transaction apply entry point запрещена;
    - actual UBT graph выводится из всех `*.Build.cs`; forbidden edge и conditional edge отвергаются, negative self-test добавляет synthetic edge;
    - actual CMake target/source graph доказывает, что portable/Headless targets не линкуют и не компилируют `GV2PresentationApply`;
    - весь source tree модуля перечисляется обходом каталога; secondary forbidden-capability gate отвергает `TSoftObjectPtr` runtime input, `LoadSynchronous`, `StaticLoadObject`, Asset Registry, filesystem/config/settings access;
    - каждый forbidden-capability case имеет synthetic negative self-test;
    - runtime authority counter показывает accesses во время Prepare и ноль вокруг единственной Apply façade для каждого operation kind;
    - production initial screen, replacement, nested collection, rollback и catastrophic recovery проходят через façade;
    - task не меняет ни одного Widget `UCLASS` module/path;
    - **предпосылка проверена до переноса**: ни один класс, подлежащий переносу, не достигает авторитета — ни через виды операций, ни через `ApplyCentralStyle`. Это результат `PSC-10A`/`PSC-10B`; здесь он не переделывается, а подтверждается обходом, потому что класс с обращением к теме в нижнем модуле не собирается, и обнаружить это на этапе переноса значит обнаружить слишком поздно.
  - Evidence: `Source/GV2PresentationApply/`, `Source/GV2/GV2.Build.cs`, `Source/CMakeLists.txt`, `Headless/CMakeLists.txt`, graph/API/capability gates и production tests.

- [ ] **PSC-12 — Атомарно мигрировать Widget `UCLASS` paths и ассеты**
  - Зависимости: PSC-11.
  - Инвариант: до task дерево целиком использует `/Script/GV2`; после task — `/Script/GV2PresentationApply`; ни один commit не содержит смешанную или неразрешимую модель.
  - Не считается закрытием: перенос classes в `PSC-11`; постоянные redirects; известный список ассетов; пересохранение только `/Game/UI`; source-only проверка без загрузки Blueprint; попытка довести прерванную миграцию вручную вместо возврата к точке отката.
  - Done:
    - перед изменением paths зафиксированы baseline Asset Registry inventory и успешная загрузка/компиляция всех Widget Blueprint, наследующих или ссылающихся на переносимые classes;
    - множество переносимых `UCLASS` выводится из фактической inheritance/dependency closure физических widget bases, а не из списка задачи;
    - в одном рабочем change set добавляются временные Core Redirects, classes переносятся, каждый affected asset загружается, компилируется и сохраняется через Unreal Editor API;
    - после resave Asset Registry/full-package sweep не находит old `/Script/GV2` class references;
    - redirects удаляются до commit, Editor перезапускается/перезагружает packages без них, повторный полный load/compile sweep проходит;
    - widget blueprint count и component contract сравниваются с baseline, который не меняется в том же task;
    - `unreal-mcp` сообщает успешные load/compile/save для фактического affected set; failed/unavailable MCP блокирует `[x]`;
    - commit содержит C++ path move и все affected UAssets вместе; промежуточное состояние не фиксируется;
    - **точка возврата названа явно и проверена до начала**: ею является коммит `PSC-11`, дерево на нём целиком использует прежние пути; при обрыве миграции на любом шаге — включая частично пересохранённые ассеты и неснятые редиректы — восстановление выполняется возвратом рабочего дерева к этому коммиту целиком, а не доведением наполовину мигрированного состояния.
  - Evidence: `Source/GV2PresentationApply/`, удалённые/перенесённые `Source/GV2/Public|Private/UI` classes, `Content/`, временный diff `Config/DefaultEngine.ini`, Asset Registry reports и Unreal MCP results.

## Проверка milestone

- [ ] Project authority type не может попасть в Apply module через UBT edge.
- [ ] UE loading/settings/filesystem capability отвергается отдельным full-source-tree gate.
- [ ] Headless/CMake graph не содержит Apply module или его sources.
- [ ] До `PSC-12` class paths не меняются; после него старые paths и redirects отсутствуют.
- [ ] Все найденные Widget Blueprint загружаются и компилируются после чистого reload.
