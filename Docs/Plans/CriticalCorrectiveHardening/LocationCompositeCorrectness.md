---
title: Critical Corrective Hardening — Location Composite Correctness
status: active
version: 1.0
updated: 2026-08-22
depends_on:
  - README.md
  - RepeaterAtomicity.md
  - ../UiFoundationHardening/LocationCompositeSemantics.md
  - ../LocationScreen/Composites.md
---

# M2 — Location Composite Correctness

> **Материализует:** корректность и атомарность LocationScreen композитов.
> **Задачи:** CCF-06…12.
>
> **Результат:** каждый LocationScreen composite сначала полностью валидирует
> candidate без mutation, затем применяет его как один commit; repeated
> characters/meters имеют только Repeater path.

## Задачи

- [ ] **CCF-06 — Сделать `CanApply*` полностью non-mutating**
  - Зависимости: CCF-01…05.
  - Файлы:
    - `Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp`;
    - `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`.
  - Done:
    - `CanApply*` не вызывает helper, создающий repeater;
    - `CanApply*` не вызывает `NewObject`, `CreateWidget`, `AddChild`,
      `SetVisibility`, `SetContainerPanel`;
    - `CanApply*` не меняет `Applied`;
    - исчезают `const_cast` только ради lazy creation.
  - Negative:
    - до и после `CanApply` internal repeater pointer, child count и visible
      state одинаковы.
  - Evidence:
    - dedicated pure-preflight test;
    - `rg` не находит relevant `const_cast.*Resolve.*Repeater`.

  ### Пошагово

  1. Найти mutation:
     ```bash
     rg -n "CanApply|const_cast|Resolve.*Repeater|NewObject|CreateWidget" \
       Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp
     ```
  2. Для SceneView/PlayerStatus/CommandPanel сохранить до `CanApply`:
     ```text
     repeater pointer
     child count
     relevant visibility
     Applied
     ```
  3. Вызвать `CanApply`.
  4. Проверить полное равенство state.
  5. Разделить helper на:
     ```text
     HasUsable...Host() const
     Ensure...Repeater()
     ```
  6. `HasUsable...Host()` может только читать state.
  7. `Ensure...Repeater()` разрешён только после successful preflight.
  8. Повторно запустить test.

- [ ] **CCF-07 — Удалить legacy single `Character` rendering path**
  - Зависимости: CCF-06.
  - Done:
    - SceneView не читает `Characters[0]`;
    - single `Character` leaf больше не является альтернативой Repeater;
    - 0, 1 и 2 character entries проходят через один Repeater path.
  - Negative:
    - если Repeater host отсутствует, candidate с characters отклоняется;
    - наличие старого single Character widget не делает такой candidate valid.
  - Evidence:
    - tests 0/1/2 characters;
    - source search не находит production `Characters[0]`.

  ### Пошагово

  1. Найти старый path:
     ```bash
     rg -n "Characters\[0\]|Character->|CharacterResource" \
       Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp \
       Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h
     ```
  2. Проверить существующий `WBP_SceneView`:
     - есть ли host для Character Repeater;
     - compile/save asset до удаления C++ fallback.
  3. Если host уже есть — asset не менять.
  4. Если host отсутствует — заменить legacy binding на уже существующий Core
     Repeater host в этом же asset.
  5. Добавить cases:
     ```text
     Characters = []
     Characters = [aria]
     Characters = [aria, merchant]
     ```
  6. Проверить entry count и keys.
  7. Удалить production `[0]` branch.
  8. Удалить obsolete binding/property только если он больше нигде не нужен.
  9. Проверить:
     ```bash
     rg -n "Characters\[0\]" Source/GV2
     ```

- [ ] **CCF-08 — Удалить legacy `StaminaMeter` / `Meters[0]` path**
  - Зависимости: CCF-06.
  - Done:
    - PlayerStatus не читает `Meters[0]`;
    - single `StaminaMeter` не является альтернативой Meter Repeater;
    - 0, 1 и 2 meters проходят через один Repeater path.
  - Negative:
    - no Repeater host + non-empty meters → validation failure.
  - Evidence:
    - tests 0/1/2;
    - source search не находит production `Meters[0]`.

  ### Пошагово

  1. Найти:
     ```bash
     rg -n "Meters\[0\]|StaminaMeter" \
       Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp \
       Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h
     ```
  2. Проверить `WBP_PlayerStatusPanel`.
  3. Убедиться, что Meter Repeater host существует.
  4. Добавить:
     ```text
     []
     [stamina]
     [health, stamina]
     ```
  5. Проверить key, percent и pointer reuse.
  6. Удалить legacy branch.
  7. Удалить obsolete binding, если он действительно больше не используется.
  8. Выполнить:
     ```bash
     rg -n "Meters\[0\]|StaminaMeter" Source/GV2
     ```

- [ ] **CCF-09 — Добавить regression tests на partial composite apply**
  - Зависимости: CCF-07, CCF-08.
  - Done:
    - существует SceneView failure test;
    - существует PlayerStatus failure test;
    - failure позднего child не меняет уже валидированные ранние visuals.
  - Evidence:
    - tests воспроизводят partial mutation до CCF-10, если defect всё ещё
      существует.

  ### PlayerStatus case

  Baseline:

  ```text
  name = Old Name
  portrait = valid_old
  stamina = 0.70
  ```

  Candidate:

  ```text
  name = New Name
  portrait = valid_new
  late repeated part = invalid
  ```

  После failure:

  ```text
  name == Old Name
  portrait == valid_old
  stamina == 0.70
  Applied == old model
  repeater state unchanged
  ```

  ### SceneView case

  Baseline:

  ```text
  context = Old Context
  background = valid_old
  character aria = valid_old
  ```

  Candidate меняет ранние значения, но содержит заведомо invalid позднюю часть.

  После failure всё остаётся baseline.

- [ ] **CCF-10 — Разделить composite apply на pure preflight и commit**
  - Зависимости: CCF-09.
  - Done:
    - все ожидаемые failures возникают до visual mutation;
    - `Applied` присваивается после успешного commit;
    - commit path не содержит обычного data-validation failure;
    - CCF-09 становится green.
  - Negative:
    - wrong `field_id`;
    - wrong `schema_id`;
    - duplicate/missing repeated key;
    - missing required host;
    - incompatible resource;
    - все случаи оставляют old state.
  - Evidence:
    - SceneView/PlayerStatus/CommandPanel failure tests green.

  ### Пошагово

  1. В каждом `ApplyScreenField` выписать все `return false`.
  2. Для каждого определить: можно ли проверить условие до mutation?
  3. Перенести все такие проверки в начало.
  4. До первого `SetText`, `ApplyImage`, Repeater commit и т.п. должны быть
     проверены:
     - field/schema identity;
     - structural shape;
     - required hosts;
     - repeated keys;
     - child `CanApply`;
     - resource compatibility.
  5. `Applied = Candidate` переместить в конец.
  6. Если child apply всё ещё может неожиданно вернуть false после полного
     preflight, не игнорировать это.
  7. Сначала выяснить, почему preflight не может предсказать failure.
  8. Не строить общий transaction framework.
  9. Запустить CCF-09.
  10. Запустить весь Location composite suite.

- [ ] **CCF-11 — Зафиксировать reset semantics для всех четырёх composites**
  - Зависимости: CCF-10.
  - Done:
    - `ResetScreenField()` очищает captured model;
    - visible text/images/repeated entries не содержат stale state;
    - повторный apply после reset не восстанавливает старые значения.
  - Evidence:
    - TopBar, SceneView, PlayerStatus, CommandPanel reset tests.

  ### Test pattern

  Для каждого composite:

  ```text
  apply model A
  verify A visible
  reset
  verify captured empty/default
  verify visible cleared/fallback according to contract
  apply model B
  verify only B visible
  ```

  Если test уже проходит, production code не менять.

- [ ] **CCF-12 — Зафиксировать существующую placeholder semantics**
  - Зависимости: CCF-07…11.
  - Done:
    - missing/invalid character resource использует существующий silhouette
      fallback;
    - portrait использует существующий portrait fallback;
    - item/effect image использует существующий missing-icon fallback;
    - optional background absence остаётся допустимым согласно текущему
      contract;
    - invalid resource не оставляет случайно предыдущую картинку.
  - Negative:
    - не добавляются новые placeholder resource IDs;
    - не меняется existing placeholder policy.
  - Evidence:
    - positive/negative resource tests.

## Проверка milestone

- [ ] `CanApply*` pure.
- [ ] Нет production `Characters[0]`.
- [ ] Нет production `Meters[0]`.
- [ ] Repeated characters/meters используют только Core Repeater.
- [ ] Failed composite apply сохраняет old state.
- [ ] `Applied` обновляется последним.
- [ ] Reset очищает captured и visible state.
- [ ] Placeholder behavior подтверждён tests.
- [ ] Полный LocationScreen-related automation зелёный.
