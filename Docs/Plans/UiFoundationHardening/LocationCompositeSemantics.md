---
title: Location Composite Semantics Tasks
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - CoreRepeater.md
  - PresentationPipelines.md
  - ../LocationScreen/Composites.md
---

# M3 — Location Composite Semantics

> **Материализует:** разделы 8—11 [предложения](../../Proposals/GameplayLocationScreenProposal.md).
> **Задачи:** UIH-09…12.
> **Результат:** четыре LocationScreen composites честно реализуют свои
> Screen Field contracts без stale visual state и скрытой потери данных.

## Задачи

- [x] **UIH-09 — Унифицировать `ResetScreenField`**
  - Done: после reset visual tree соответствует default/empty captured state.
  - `TopBar`: старые day/location/resource больше не видны.
  - `PlayerStatus`: очищены name, portrait, meters, item/effect collections.
  - `SceneView`: optional layers возвращены в default state.
  - `CommandPanel`: collection пуста.
  - Test: Apply A → Reset → Capture/visual assertions не содержат A.
  - Evidence: `GV2LocationCompositeWidgetBases.*`, automation.

- [x] **UIH-10 — Исправить placeholder semantics**
  - Done:
    - пустая background illustration → остаётся только tile;
    - missing/invalid character image → silhouette placeholder;
    - missing/invalid portrait → portrait placeholder;
    - missing/invalid item/effect icon → icon placeholder с сохранением entry;
    - отсутствие optional resource не прерывает применение всего Screen.
  - Diagnostic: fallback остаётся наблюдаемым.
  - Evidence: resource fixtures, composite code, negative tests.

- [x] **UIH-11 — Унифицировать Screen Field validation**
  - Done: все четыре composites проверяют ожидаемые `field_id`,
    `schema_id` и структурную применимость до mutation; ошибочный field с
    правильной schema не принимается; `CanApply` не изменяет widget.
  - Evidence: `GV2LocationCompositeWidgetBases.*`,
    adapter/composite negative tests.

- [x] **UIH-12 — Устранить молчаливое усечение массивов presentation**
  - Зависимости: UIH-01, UIH-04.
  - Done: renderer не использует `[0]` как скрытую реализацию массива.
    Character entries отображаются repeated collection.
    Для `Meters` выбирается один честный contract:
      1. либо поле становится явно singular stamina в соответствующей schema;
      2. либо все объявленные meters рендерятся repeated collection.
    Предпочтительный вариант — второй, поскольку ViewModel уже выражает
    ordered collection.
  - Constraint: identity meter entry стабильна и не выводится из localized text.
  - Evidence: schemas/view models, PlayerStatus renderer, tests with 0/1/N entries.

## Проверка milestone

- [x] Reset не оставляет stale visual state.
- [x] Empty character показывает принятую заглушку.
- [x] Optional resource failure не ломает весь Screen.
- [x] FieldId проверяется всеми composites.
- [x] Ни character, ни meter arrays не обрезаются молча.
