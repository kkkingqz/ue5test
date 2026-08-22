---
title: Critical Corrective Hardening — Graphics Contract
status: active
version: 1.0
updated: 2026-08-22
depends_on:
  - README.md
  - LocationCompositeCorrectness.md
  - ../UiFoundationHardening/PresentationPipelines.md
  - ../../UI/ImageResources.md
---

# M3 — Graphics Contract

> **Материализует:** исправление runtime source of truth для ScalePolicy.
> **Задачи:** CCF-13…15.
>
> **Результат:** `ScalePolicy` задаётся только самим widget/slot contract;
> resource metadata лишь подтверждает или отклоняет совместимость.

## Задачи

- [ ] **CCF-13 — Добавить regression test: `RenderMode` не меняет `ScalePolicy`**
  - Файлы:
    - `Source/GV2/Private/UI/GV2ImageWidgetBase.cpp`;
    - `Source/GV2/Public/UI/GV2ImageWidgetBase.h`;
    - `Source/GV2/Private/UI/GV2ImagePresentation.cpp`;
    - существующие image automation tests.
  - Done:
    - widget с `ScalePolicy=PreserveAspect` после load/post-load остаётся
      `PreserveAspect`, даже если initial resource имеет Tile/NineSlice mode;
    - incompatible apply возвращает failure и не меняет policy;
    - previous valid brush остаётся неизменным.
  - Evidence:
    - test сначала воспроизводит auto-inference, затем проходит после CCF-14.

  ### Пошагово

  1. Найти inference:
     ```bash
     rg -n "PostLoad|ScalePolicy|RenderMode|InitialResourceId" \
       Source/GV2/Private/UI/GV2ImageWidgetBase.cpp \
       Source/GV2/Public/UI/GV2ImageWidgetBase.h
     ```
  2. Создать test widget:
     ```text
     ScalePolicy = PreserveAspect
     InitialResource = known Tile resource
     ```
  3. Выполнить тот lifecycle path, где раньше происходил `PostLoad`.
  4. Проверить:
     ```text
     ScalePolicy == PreserveAspect
     ```
  5. Отдельно применить incompatible resource.
  6. Проверить:
     ```text
     result == false
     ScalePolicy unchanged
     AppliedResourceId unchanged
     brush unchanged
     ```

- [ ] **CCF-14 — Удалить `RenderMode → ScalePolicy` inference и поправить existing assets**
  - Зависимости: CCF-13.
  - Done:
    - `PostLoad` не выводит policy из resource metadata;
    - Tile/NineSlice widgets имеют явно сохранённую policy;
    - LocationScreen assets продолжают корректно валидироваться.
  - Negative:
    - не вводится новый fallback policy;
    - имя ресурса/файла не используется для догадки;
    - `RenderMode` не становится вторым behavior switch.
  - Evidence:
    - CCF-13 green;
    - asset audit;
    - source search не находит inference.

  ### Пошагово

  1. До удаления inference определить, какие existing assets реально зависят от
     него.
  2. Для каждого такого widget записать:
     ```text
     asset path
     current resource
     required ScalePolicy
     ```
  3. Открыть asset через существующий Unreal authoring path.
  4. Явно поставить уже существующий enum:
     - `Tile`;
     - `NineSlice`;
     - или другую уже принятую policy.
  5. Compile.
  6. Save.
  7. Только после этого удалить inference из C++.
  8. Пересобрать Editor.
  9. Запустить image + LocationScreen automation.
  10. Если asset теперь incompatible — исправить asset property, а не
      возвращать inference.

- [ ] **CCF-15 — Закрыть graphics regression matrix на resulting brush**
  - Зависимости: CCF-13, CCF-14.
  - Done:
    - PreserveAspect;
    - FreeStretch;
    - Tile;
    - NineSlice;
    - incompatible apply atomicity;
    - проверены по resulting widget/brush state.
  - Evidence:
    - dedicated Unreal automation.

  ### PreserveAspect

  Проверить compatible fixed-aspect resource и resulting brush/widget state.

  ### FreeStretch

  Если resource допускает использование:
  ```text
  DrawAs == Image
  Tiling == NoTile
  ```

  ### Tile

  Проверить реальное expected tiling:
  ```text
  DrawAs == Image
  Tiling == expected Tile mode
  ```

  ### NineSlice

  Применить реальный existing NineSlice fixture.

  Проверить:
  ```text
  DrawAs == Box
  Margin == expected parsed margin
  ```

  ### Failure atomicity

  1. Apply valid resource.
  2. Сохранить:
     ```text
     AppliedResourceId
     ResourceObject
     DrawAs
     Tiling
     Margin
     ```
  3. Apply incompatible resource.
  4. Проверить `false`.
  5. Все значения должны быть равны baseline.

## Проверка milestone

- [ ] `ScalePolicy` — единственный behavior source.
- [ ] `RenderMode` используется только для compatibility validation.
- [ ] Existing assets имеют explicit policy.
- [ ] NineSlice проверен реальным brush.
- [ ] Failed apply не портит previous brush.
- [ ] LocationScreen image tests зелёные.
