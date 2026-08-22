---
title: Critical Corrective Hardening — Closure
status: active
version: 1.0
updated: 2026-08-22
depends_on:
  - README.md
  - RepeaterAtomicity.md
  - LocationCompositeCorrectness.md
  - GraphicsContract.md
  - Verification.md
  - ../UiFoundationHardening/README.md
  - ../LocationScreen/README.md
---

# M5 — Closure

> **Материализует:** закрытие плана критических corrective-исправлений.
> **Задачи:** CCF-22…24.
>
> **Результат:** документы больше не заявляют evidence, которого нет, а
> corrective change set проходит полный project gate.

## Задачи

- [ ] **CCF-22 — Синхронизировать `UiFoundationHardening` и `LocationScreen`**
  - Зависимости: CCF-01…21.
  - Done:
    - task/milestone checkboxes соответствуют реально пройденному Evidence;
    - final DoD не противоречит milestone state;
    - старые synthetic acceptance claims не используются вместо новых tests.
  - Evidence:
    - docs diff;
    - `validate_docs.py`.

  ### Проверить

  ```text
  Docs/Plans/UiFoundationHardening/README.md
  Docs/Plans/UiFoundationHardening/CoreRepeater.md
  Docs/Plans/UiFoundationHardening/PresentationPipelines.md
  Docs/Plans/UiFoundationHardening/LocationCompositeSemantics.md
  Docs/Plans/UiFoundationHardening/Verification.md

  Docs/Plans/LocationScreen/README.md
  Docs/Plans/LocationScreen/Verification.md
  ```

  ### Правило

  `[x]` ставится только если соответствующий test действительно проходит на
  текущем corrective HEAD.

  Нельзя:
  - оставлять M4 `[x]`, если actual geometry tests ещё не готовы;
  - отмечать Repeater atomicity по одному `EntryCount`;
  - переписывать архивный `UiFoundation` history.

- [ ] **CCF-23 — Выполнить targeted source audit**
  - Зависимости: CCF-22.
  - Done:
    - отсутствуют известные запрещённые compatibility patterns;
    - каждый remaining match вручную объяснён.
  - Evidence:
    - результаты команд ниже.

  ### Команды

  ```bash
  rg -n "Characters\[0\]|Meters\[0\]" Source/GV2

  rg -n "const_cast.*Resolve.*Repeater" Source/GV2

  rg -n "StaminaMeter" Source/GV2

  rg -n "meter_[0-9]|meter_.*Index" Source/GV2 Scripts GameData

  rg -n "ScalePolicy.*RenderMode|RenderMode.*ScalePolicy" \
    Source/GV2/Private/UI Source/GV2/Public/UI
  ```

  Если match находится:
  1. открыть контекст;
  2. определить, production ли это;
  3. если test/history/comment — допустимо;
  4. если production fallback — milestone не закрывать.

- [ ] **CCF-24 — Полный project gate и финальный отчёт**
  - Зависимости: CCF-01…23.
  - Done:
    - portable tests;
    - headless checks;
    - docs validation;
    - clean GV2Editor build;
    - полный relevant Unreal automation;
    - manual smoke существующего LocationScreen flow.
  - Evidence:
    - итоговая таблица PASS/FAIL.

  ### Шаг 1 — diff hygiene

  ```bash
  git diff --check
  git status --short
  ```

  Проверить отсутствие:
  ```text
  Saved/
  Intermediate/
  Build logs
  случайных .uasset autosaves
  unrelated files
  ```

  ### Шаг 2 — portable

  ```bash
  cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
  cmake --build cmake-build-debug --parallel 2
  ctest --test-dir cmake-build-debug --output-on-failure

  ./cmake-build-debug/Headless/gv2-headless --self-test
  ./cmake-build-debug/Headless/gv2-headless --check-scripts

  python3 Tools/Documentation/validate_docs.py
  ```

  Все команды должны завершиться exit code 0.

  ### Шаг 3 — Unreal build

  ```bash
  "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" \
    GV2Editor Linux Development "$PWD/GV2.uproject" \
    -WaitMutex -NoHotReloadFromIDE
  ```

  ### Шаг 4 — Unreal automation

  Запустить полный релевантный `GV2.Runtime` suite.

  Нельзя принимать результат по одному targeted test: final gate должен
  проверить соседние UI runtime tests.

  ### Шаг 5 — manual smoke

  Только существующее behavior:

  1. запустить Editor;
  2. открыть существующий LocationScreen flow;
  3. убедиться, что Tavern отображается;
  4. выполнить существующий переход в Market;
  5. убедиться, что экран остаётся рабочим;
  6. проверить отсутствие stale character/meter/commands;
  7. выполнить ещё один существующий route/action;
  8. убедиться, что UI не потерял bindings.

  Ничего нового для smoke test не добавлять.

  ### Итоговый отчёт

  ```text
  | Check                         | Result | Evidence |
  |------------------------------|--------|----------|
  | CTest                         | PASS   | ...      |
  | Headless self-test            | PASS   | ...      |
  | Script check                  | PASS   | ...      |
  | Docs validation               | PASS   | ...      |
  | GV2Editor build               | PASS   | ...      |
  | GV2.Runtime automation        | PASS   | ...      |
  | LocationScreen manual smoke   | PASS   | ...      |
  ```

## Проверка milestone

- [ ] Plan/status docs соответствуют текущему evidence.
- [ ] Запрещённые source patterns отсутствуют.
- [ ] Full portable gate зелёный.
- [ ] Clean `GV2Editor` build зелёный.
- [ ] Relevant Unreal automation зелёный.
- [ ] Manual smoke зелёный.
- [ ] Рабочее дерево содержит только intentional corrective changes.

## Условие завершения всего плана

План нельзя считать завершённым, если выполняется хотя бы одно условие:

- Repeater failure оставляет partial widget state;
- `CanApply*` создаёт/мутирует widgets;
- существует production `Characters[0]`;
- существует production `Meters[0]`;
- repeated identity зависит от index/resource fallback;
- composite failure оставляет частично новый экран;
- `ScalePolicy` выводится из `RenderMode`;
- viewport acceptance основан только на DesiredSize/математике;
- 720p проверяется только количеством entries;
- font conformance сравнивает только helper calculations;
- NineSlice не проверен resulting brush;
- documentation claims `[x]` без нового evidence.
