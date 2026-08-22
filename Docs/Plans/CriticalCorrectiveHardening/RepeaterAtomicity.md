---
title: Critical Corrective Hardening — Repeater Atomicity and Identity
status: active
version: 1.0
updated: 2026-08-22
depends_on:
  - README.md
  - ../UiFoundationHardening/CoreRepeater.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# M1 — Repeater Atomicity and Identity

> **Материализует:** атомарность и стабильную identity Core Repeater.
> **Задачи:** CCF-01…05.
>
> **Результат:** Core Repeater либо полностью принимает candidate, либо
> оставляет live collection без изменений; identity repeated entries не
> синтезируется из позиции или изменяемого ресурса.

## Результат этапа

После M1 повторное применение одного и того же key переиспользует существующий
widget, reorder меняет только порядок, а любой validation/apply failure
происходит до изменения live state.

## Задачи

- [x] **CCF-01 — Зафиксировать regression test partial mutation reused widget**
  - Файлы для поиска:
    - `Source/GV2/Private/UI/GV2ListViewWidgetBase.cpp`;
    - `Source/GV2/Public/UI/GV2ListViewWidgetBase.h`;
    - файл, содержащий `FGV2KeyedCollection`;
    - существующие `GV2.Runtime` Repeater tests.
  - Done:
    - есть test с двумя уже существующими keyed widgets;
    - первый candidate entry может быть применён;
    - второй candidate entry гарантированно invalid;
    - общий reconcile возвращает failure;
    - после failure оба live widgets имеют старые значения;
    - pointers, child count и order не изменились.
  - Negative:
    - test не считается достаточным, если проверяет только `result == false`;
    - test обязан проверять state первого widget, который мог быть изменён до
      failure второго.
  - Evidence:
    - test сначала воспроизводит defect;
    - тот же test становится зелёным после CCF-02.

  ### Пошагово

  1. Найти существующий Repeater automation test:
     ```bash
     rg -n "Repeater|KeyedCollection|ListViewWidgetBase" \
       Source/GV2/Private/Tests Source/GV2
     ```
  2. Не создавать новый test framework. Добавить case в существующий suite.
  3. Подготовить baseline:
     ```text
     item_a: value=10
     item_b: value=20
     ```
  4. Убедиться, что оба widgets реально созданы.
  5. Сохранить:
     ```text
     pointer_a
     pointer_b
     old_value_a
     old_value_b
     child_order
     child_count
     ```
  6. Подать candidate:
     ```text
     item_a: value=100  // valid
     item_b: invalid    // failure должен быть предсказуемым
     ```
  7. Проверить:
     ```text
     reconcile == false
     pointer_a unchanged
     pointer_b unchanged
     value_a == 10
     value_b == 20
     order unchanged
     count unchanged
     ```
  8. Запустить только этот test.
  9. Ожидаемый defect до fix: `item_a` уже содержит `100`.
  10. Если test падает раньше на setup/class/container — исправить test setup,
      production code пока не трогать.

- [x] **CCF-02 — Сделать Repeater reconcile атомарным после preflight**
  - Зависимости: CCF-01.
  - Done:
    - все ожидаемые validation failures обнаруживаются до первого изменения
      live reused widget;
    - создание/подготовка новых widgets выполняется до commit или имеет
      локально безопасную semantics;
    - commit phase не содержит штатной ветки, которая может вернуть обычный
      data-validation failure после мутации части collection;
    - CCF-01 проходит.
  - Negative:
    - не создавать generic UObject transaction/snapshot framework;
    - не копировать весь widget tree «для rollback»;
    - не ловить проблему простым повторным `Apply(old_model)` после failure,
      если old model не является гарантированно полным visual snapshot.
  - Evidence:
    - CCF-01 green;
    - existing create/remove/reorder/reuse tests green.

  ### Пошагово

  1. Найти полный reconcile path:
     ```bash
     rg -n "CanApplyItem|ApplyItem|Reconcile" \
       Source/GV2/Private/UI Source/GV2/Public/UI
     ```
  2. Нарисовать на бумаге/в comment только для себя текущий порядок:
     ```text
     validate keys
     create/reuse
     apply A
     apply B
     ...
     reorder children
     ```
  3. Отметить все операции, которые могут вернуть `false`.
  4. Перенести эти проверки до mutation:
     - empty/duplicate key;
     - отсутствующий host;
     - отсутствующий widget class;
     - structural model validation;
     - child `CanApply`.
  5. Если есть `CanApplyItem`, вызвать его для всех candidate entries до
     первого `ApplyItem`.
  6. После успешного preflight commit должен:
     - обновить reused widgets;
     - добавить prepared new widgets;
     - удалить исчезнувшие;
     - восстановить order.
  7. Если `ApplyItem` всё ещё возвращает `bool`, проверить все consumers.
     Если `false` означает только то, что уже можно определить в preflight,
     убрать ожидаемый failure из commit path.
  8. Не менять внешний API шире необходимого.
  9. Запустить CCF-01.
  10. Запустить весь Repeater suite.
  11. Выполнить:
      ```bash
      git diff --check
      ```

- [x] **CCF-03 — Сделать repeated key обязательным и удалить positional fallback**
  - Зависимости: CCF-02.
  - Done:
    - missing key отклоняется;
    - empty key отклоняется;
    - duplicate key отклоняется;
    - meter key не синтезируется из index (`meter_0`, `meter_1`, ...);
    - никакой GUID не генерируется автоматически.
  - Negative:
    - array index не является identity;
    - текущий порядок массива не должен влиять на identity.
  - Evidence:
    - negative tests missing/empty/duplicate;
    - reorder test сохраняет pointers.

  ### Пошагово

  1. Найти fallback:
     ```bash
     rg -n "meter_|index|Index|Make.*Key|Key.*=" \
       Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp \
       Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h
     ```
  2. Добавить negative test: meter entry без explicit key.
  3. Запустить test и убедиться, что текущий код его принимает, если defect ещё
     существует.
  4. Удалить генерацию `meter_<index>`.
  5. На missing/empty key вернуть существующий validation failure.
  6. Добавить reorder:
     ```text
     before: health, stamina
     after:  stamina, health
     ```
  7. Проверить:
     - pointers health/stamina те же;
     - order поменялся;
     - values обновились корректно.

- [x] **CCF-04 — Не использовать Character ResourceId как identity**
  - Зависимости: CCF-02.
  - Done:
    - existing explicit character entry key является identity;
    - изменение portrait/body/image resource при том же key не пересоздаёт
      widget;
    - resource ID не используется как fallback key.
  - Negative:
    - missing explicit key отклоняется;
    - изменение resource ID не означает удаление старого character entry и
      создание нового.
  - Evidence:
    - test:
      ```text
      key=aria, resource=aria_normal
      →
      key=aria, resource=aria_armor
      ```
      сохраняет pointer.

  ### Пошагово

  1. Найти создание character entry models:
     ```bash
     rg -n "Characters|Character.*Key|ResourceId" \
       Source/GV2 Scripts GameData/TextSystem GameData/RH
     ```
  2. Добавить reuse test.
  3. Убедиться, что test действительно меняет resource.
  4. Удалить `ResourceId` fallback для key.
  5. Не менять wire schema, если explicit key уже существует.
  6. Если schema действительно не может выразить key — остановиться: это уже
      schema capability change и выходит за corrective-only scope.

- [x] **CCF-05 — Закрыть Repeater regression matrix**
  - Зависимости: CCF-01…04.
  - Done: покрыты все текущие операции Repeater.
  - Evidence: один suite с перечисленными ниже cases.

  ### Обязательная матрица

  Проверить отдельно:

  ```text
  empty → 1
  1 → 3
  3 → 1
  same keys + same order + changed values
  same keys + reordered
  remove one key
  add one key
  empty key
  missing key
  duplicate key
  preflight failure
  failure второго reused entry не меняет первый
  clear/reset
  ```

  Для reuse cases:
  - сравнивать pointers.

  Для failure cases:
  - сравнивать model/visible state;
  - сравнивать child order/count.

  Для reorder:
  - убедиться, что widgets не создавались заново.

## Проверка milestone

- [x] Failed reconcile оставляет collection byte-for-behavior эквивалентной
  предыдущему состоянию.
- [x] Reused widgets не мутируют до окончания preflight.
- [x] Empty/missing/duplicate key отклоняется.
- [x] Meter key не зависит от position.
- [x] Character key не зависит от resource.
- [x] Reorder сохраняет widget pointers.
- [x] Existing Repeater automation полностью зелёный.
