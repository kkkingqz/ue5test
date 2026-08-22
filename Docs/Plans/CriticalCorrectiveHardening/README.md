---
title: Critical Corrective Hardening Plan
status: active
version: 1.0
updated: 2026-08-22
depends_on:
  - ../UiFoundationHardening/README.md
  - ../LocationScreen/README.md
  - ../../UI/ImageResources.md
  - ../../UI/ScreenTemplates.md
  - ../../UI/UIDocumentAndReconciliation.md
decisions:
  - ../../ADR/0013-unified-text-pipeline.md
  - ../../ADR/0017-centralized-ui-presentation-paths.md
  - ../../ADR/0035-ui-foundation-and-composition.md
---

# План критических corrective-исправлений

> **Материализует:** исправление критических расхождений UI contract и реализации.
>
> **Задачи:** CCF-01…24.
>
> **Результат:** существующий UI-код не оставляет частичное состояние при
> ошибках, repeated content имеет устойчивую identity, composites не используют
> legacy fallback paths, graphics policy имеет один источник поведения, а
> verification проверяет фактическое UMG/Slate behavior.
>
> **Ограничение:** этот план не добавляет новый функционал.

## Цель

План предназначен для исправления уже существующего кода без расширения
архитектуры и без появления новых игровых возможностей.

Главная задача — убрать места, где формально существующий API соответствует
документации, но фактическое runtime behavior нарушает уже принятые
инварианты.

Порядок исправления намеренно идёт снизу вверх:

```text
Core Repeater / identity
        ↓
Location composites
        ↓
Graphics contract
        ↓
Real verification
        ↓
Docs / closure
```

Такой порядок обязателен. Нельзя сначала «подправить тесты», чтобы они
соответствовали текущему поведению, а затем считать production code
правильным.

## Состояние на входе

Перед началом работ исполнитель обязан проверить фактический `HEAD`:

```bash
git rev-parse HEAD
git status --short
```

План составлялся по актуальному `main` на момент подготовки, но конкретные
строки кода могут измениться. Поэтому перед каждой задачей необходимо
убедиться, что описанный дефект всё ещё существует.

Если дефект уже исправлен другим commit:

1. production code повторно не менять;
2. добавить или проверить regression test;
3. записать Evidence;
4. только после этого отметить задачу выполненной.

## Главный рабочий цикл

Каждая задача выполняется по схеме:

```text
1. Прочитать contract и существующий код.
2. Написать regression test.
3. Запустить test и увидеть ожидаемый FAIL.
4. Внести минимальную production-правку.
5. Повторно запустить тот же test → PASS.
6. Запустить соседний suite → PASS.
7. Проверить diff.
8. Только после этого отметить checkbox.
```

Если новый test сразу проходит:

- не менять production code;
- убедиться, что test действительно проверяет нужный invariant;
- если invariant уже соблюдается — задача закрывается test/evidence-only.

Если test падает по другой причине, чем описано в задаче:

- production code не менять;
- сначала исправить test setup.

## Принятые решения

- План является corrective-only.
- Архивные планы не переписываются задним числом.
- Существующие Core UI contracts являются исходной нормой.
- `CanApply*` не имеет права изменять UObject/widget state.
- Failed validation/apply не оставляет частично изменённое визуальное состояние.
- Repeated content идентифицируется только стабильным explicit key.
- Array index не является identity.
- Resource ID не является identity сущности, если ресурс может меняться при
  сохранении самой сущности.
- Нельзя иметь одновременно новый Repeater path и старый `[0]` fallback path.
- `ScalePolicy` определяет поведение widget.
- `RenderMode` ресурса определяет только совместимость ресурса.
- Verification считается достаточным только при проверке фактического
  instantiated widget behavior.
- Checkbox ставится только после указанного Evidence.

## Границы

Входят:

- atomicity существующего Core Repeater;
- validation stable keys;
- удаление legacy single-character path;
- удаление legacy single-meter path;
- pure `CanApply*`;
- atomic composite apply;
- существующая placeholder/reset semantics;
- устранение `RenderMode → ScalePolicy` inference;
- исправление существующих asset properties;
- реальные geometry/rendering/reuse regression tests;
- синхронизация активных plan/status docs.

Не входят:

- новые экраны;
- новые UI primitives;
- новые gameplay mechanics;
- новые schema capabilities;
- новая system-level transaction framework;
- transaction journal для Lua gameplay state;
- presentation Effects;
- полноценный Menu↔Game session replacement;
- CommonUI migration;
- новая Back/Confirm routing;
- async image loading/cache;
- packaging/deployment redesign;
- Content Editor hardening;
- новый моддинг functionality;
- новые ADR, если не меняется уже принятый invariant;
- общий cleanup/refactoring «заодно».

Если для выполнения задачи внезапно требуется пункт из списка выше — работу по
задаче остановить и вынести проблему отдельно. Нельзя расширять этот план
самостоятельно.

## Milestones

- [x] M1 — [Repeater Atomicity and Identity](RepeaterAtomicity.md): failed
  reconciliation не мутирует live widgets, repeated entries имеют только
  stable explicit keys. CCF-01…05.
- [x] M2 — [Location Composite Correctness](LocationCompositeCorrectness.md):
  pure preflight, отсутствие `[0]` fallback paths, atomic composite apply,
  корректные reset/placeholders. CCF-06…12.
- [x] M3 — [Graphics Contract](GraphicsContract.md): `ScalePolicy` остаётся
  единственным runtime behavior source, assets и brush state соответствуют
  contract. CCF-13…15.
- [x] M4 — [Verification](Verification.md): реальные geometry, consumer font,
  NineSlice и Tavern→Market tests. CCF-16…21.
- [x] M5 — [Closure](Closure.md): синхронизация plan/status docs и полный
  project gate. CCF-22…24.

## Критический путь

```text
M1 ───► M2 ───► M3 ───► M4 ───► M5
```

M4 нельзя начинать как acceptance gate до завершения M1–M3.

Причина: verification обязан фиксировать нормативное поведение после
исправления production code, а не закреплять compatibility behavior.

## Общие правила выполнения

1. Перед первым изменением:
   ```bash
   git status --short
   git rev-parse HEAD
   ```
2. Рабочее дерево должно быть чистым или исполнитель должен точно понимать
   происхождение каждого локального изменения.
3. Не использовать `git reset --hard` для очистки чужой работы.
4. Не менять unrelated formatting.
5. Не обновлять dependencies.
6. Не менять CMake/Build.cs без прямой необходимости задачи.
7. Сначала regression test, затем production fix.
8. Для failure test проверяется не только return value, но и state до/после.
9. Для reuse test сравнивается pointer, а не только число элементов.
10. Для `.uasset` правок использовать существующий Unreal authoring path,
    затем compile и save.
11. Любой удаляемый compatibility path проверяется через `rg`, чтобы он не
    остался в другом месте.
12. После каждой задачи:
    ```bash
    git diff --check
    git status --short
    git diff
    ```
13. В commit не включать `Saved/`, `Intermediate/`, generated logs и случайные
    editor artifacts.
14. Нельзя ослаблять assertion только ради зелёного теста.
15. Нельзя добавлять новый fallback, если исправляется старый fallback.

## Базовые команды проверки

### Portable

```bash
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --parallel 2
ctest --test-dir cmake-build-debug --output-on-failure

./cmake-build-debug/Headless/gv2-headless --self-test
./cmake-build-debug/Headless/gv2-headless --check-scripts

python3 Tools/Documentation/validate_docs.py
```

### Unreal build

```bash
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" \
  GV2Editor Linux Development "$PWD/GV2.uproject" \
  -WaitMutex -NoHotReloadFromIDE
```

### Unreal runtime automation

Использовать существующий `GV2.Runtime` suite. Если точное имя targeted test
неизвестно, сначала найти его:

```bash
rg -n "IMPLEMENT_.*AUTOMATION_TEST|GV2\.Runtime" Source
```

Не угадывать имя test.

## Итоговый Definition of Done

- [x] Failed Repeater reconciliation не изменяет ни один reused widget.
- [x] Repeater failure не меняет child order/count.
- [x] Empty/duplicate/missing repeated key отклоняется до mutation.
- [x] Character identity не выводится из resource ID.
- [x] Meter identity не выводится из array index.
- [ ] `CanApply*` не создаёт UObject и не меняет widget tree/state.
- [ ] Scene character collection имеет только Repeater rendering path.
- [ ] Player meters имеют только Repeater rendering path.
- [ ] В production code отсутствуют `Characters[0]` и `Meters[0]` fallback paths.
- [ ] Failed composite apply сохраняет предыдущие model и visuals.
- [ ] Captured `Applied` меняется только после успешного visual commit.
- [ ] `ResetScreenField()` очищает captured и visible state.
- [ ] Existing placeholder semantics подтверждены regression tests.
- [ ] `ScalePolicy` нигде не выводится из resource `RenderMode`.
- [ ] Existing assets явно содержат требуемый `ScalePolicy`.
- [ ] Failed image apply сохраняет предыдущий valid brush state.
- [ ] Реальный `WBP_LocationScreen` проверен на шести viewport sizes.
- [ ] 1280×720 проверяется по actual command geometry.
- [ ] 21:9 проверяется по actual SceneView allocation.
- [ ] Text/RichText/Button/Input/Dropdown проверены по фактическому font size.
- [ ] NineSlice проверен через resulting brush.
- [ ] Tavern→Market проверяет exact target presentation и Screen reuse.
- [ ] Активные plan/status docs соответствуют фактическому evidence.
- [ ] Portable gate зелёный.
- [ ] `GV2Editor` build зелёный.
- [ ] Полный релевантный Unreal automation suite зелёный.
