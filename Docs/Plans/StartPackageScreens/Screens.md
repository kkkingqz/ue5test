---
title: Start Package Screens
status: active
version: 0.1
updated: 2026-10-17
depends_on:
  - README.md
  - ../../UI/ScreenTemplates.md
  - ../../Status/ScreenRequests.md
---

# Экраны

> **Материализует:** этап M1 [плана](README.md): меню локации v2, отчёт о конце хода, магазин, диалог, сцена с участниками.

Каждый экран — Definition kind `screen` в `GameData/textsystem/definitions/`, Widget Blueprint на `WBP_ScreenBase` в `Content/TextSystem/UI/Screens/`, запись в `DA_ScreenRegistry` со слоем `location_content`, schemas полей в `GameData/textsystem/schemas/`, field contract в [Screen Templates](../../UI/ScreenTemplates.md). Поля — по заявке с уточнениями SPS-01.

## SPS-04 — Меню локации с неактивными пунктами (SR-001)

- [ ] SPS-04 — Меню локации с неактивными пунктами

**Файлы:** `GameData/textsystem/schemas/ui_field_location_commands_v2.schema.json5`; элементы `WBP_CommandPanel` → элемент действия SPS-02; `GameData/textsystem/scripts/presentation/location_presenter.lua` публикует v2; `Docs/UI/ScreenTemplates.md` § Location Commands Field Contract.

**Зависит от:** SPS-02; GAP-006 — тем же change set или раньше.

**Решение:** элемент `items[]` — элемент действия. v1 удаляется, если у неё нет других потребителей; изменение классифицируется по [Compatibility Policy](../../Architecture/CompatibilityPolicy.md).

**Инвариант:** [SR-001](../../Status/ScreenRequests.md#sr-001-неактивный-пункт-меню-с-подсказкой-о-причине): неактивность — только presentation; команда перепроверяет условия. Нарушение — пункт, скрытый вместо неактивного.

**Не считается закрытием:**
- v2, которую публикует только тест;
- неактивный пункт, которого не публикует ни один production-источник.

**Done:**
- Потребители `location_commands.v1` перечислены поиском schema ID по `GameData/`, `Scripts/`, `Source/`, `Tests/`; каждый переведён на v2 или назван причиной оставить v1.
- Экран локации, опубликованный production presenter-ом, показывает неактивный пункт с подсказкой.
- Пункт с тем же `key` при смене доступности сохраняет widget.
- Панель команд с неактивными пунктами сохраняет layout на шести разрешениях.
- Presentation specs экрана локации проходят.

**Evidence:** content smoke через production reconciler; поиск потребителей v1; матрица; specs экрана локации.

## SPS-05 — Экран отчёта о конце хода (SR-002)

- [ ] SPS-05 — Экран отчёта о конце хода

**Файлы:** Definition `textsystem:screen.turn_report`; schema записей отчёта; Widget Blueprint экрана на элементе SPS-03; запись `DA_ScreenRegistry`; текст «Далее» `textsystem`; `Docs/UI/ScreenTemplates.md`.

**Зависит от:** SPS-03; production-путь — механизм режимов.

**Решение:** поля `title`, `entries` (`key`, необязательный `resource_id`, `text`), `buttons` по [SR-002](../../Status/ScreenRequests.md#sr-002-отчёт-о-конце-хода).

**Инвариант:** запись без `resource_id` показывается без картинки, а не с заглушкой. Нарушение — fallback-ресурс в записи без картинки.

**Не считается закрытием:**
- модальное окно вместо экрана слоя `location_content`;
- экран, показанный только тестовым presenter-ом.

**Done:**
- `textsystem:screen.turn_report` разрешается через `DA_ScreenRegistry` в слое `location_content`.
- Пустой `entries` отклоняется схемой.
- Запись без `resource_id` занимает layout без картинки.
- Записи листаются по одной; на последней показаны `buttons`.
- Длинный текст записи прокручивается внутри блока на шести разрешениях.
- Экран показан route активного режима через production-путь.

**Evidence:** content smoke; отказы схемы; матрица; `run_ue_tests.py`.

## SPS-06 — Экран магазина (SR-003)

- [ ] SPS-06 — Экран магазина

**Файлы:** Definition `textsystem:screen.shop`; schemas полей; Widget Blueprint экрана и composite товара по решению SPS-01; запись `DA_ScreenRegistry`; `Docs/UI/ScreenTemplates.md`.

**Зависит от:** SPS-02; production-путь — механизм режимов.

**Решение:** поля по [SR-003](../../Status/ScreenRequests.md#sr-003-экран-магазина) с уточнением SPS-01; кнопка покупки — элемент действия.

**Инвариант:** выбор и прокрутка — UE-local state и сохраняются между revisions с теми же `key`. Нарушение — сброс выбора после покупки.

**Не считается закрытием:**
- подтверждение покупки модальным окном;
- `key` товара из позиции в списке.

**Done:**
- `textsystem:screen.shop` разрешается в слое `location_content`.
- Товар без денег показан неактивным с причиной.
- После новой revision с теми же `key` товаров выбор и прокрутка сохранены, баланс и доступность обновлены.
- Длинный ассортимент прокручивается на шести разрешениях.
- «Уйти» отправляет свой binding.
- Экран показан route активного режима через production-путь.

**Evidence:** content smoke с двумя последовательными revisions; матрица; `run_ue_tests.py`.

## SPS-07 — Экран диалога (SR-004)

- [ ] SPS-07 — Экран диалога

**Файлы:** Definition `textsystem:screen.dialogue`; schemas полей; Widget Blueprint экрана; запись `DA_ScreenRegistry`; `Docs/UI/ScreenTemplates.md`.

**Зависит от:** SPS-02; production-путь — механизм режимов.

**Решение:** поля по [SR-004](../../Status/ScreenRequests.md#sr-004-экран-диалога); варианты ответа — элементы действия; сброс прокрутки при смене шага — по решению SPS-01.

**Инвариант:** «Далее» — обычный вариант из `choices`, а не кнопка шаблона. Нарушение — отдельная кнопка «Далее» в template.

**Не считается закрытием:**
- новый Screen Instance на каждый шаг;
- прокрутка, сохранённая при переходе к другому шагу.

**Done:**
- `textsystem:screen.dialogue` разрешается в слое `location_content`.
- Пустой `choices` отклоняется схемой.
- Шаг без `speaker_name` и портрета показан как реплика рассказчика.
- Новый шаг — новая revision того же Screen Instance; прокрутка текста сбрасывается при смене шага.
- Неактивный вариант показан с причиной.
- Длинный текст прокручивается на шести разрешениях.
- Экран показан route активного режима через production-путь.

**Evidence:** content smoke со сменой шага; отказы схемы; матрица; `run_ue_tests.py`.

## SPS-08 — Экран сцены с участниками (SR-005)

- [ ] SPS-08 — Экран сцены с участниками

**Файлы:** Definition `textsystem:screen.participant_scene`; schemas участников и полосок; Widget Blueprint экрана и composite участника; запись `DA_ScreenRegistry`; `Docs/UI/ScreenTemplates.md`.

**Зависит от:** SPS-02; production-путь — механизм режимов.

**Решение:** поля по [SR-005](../../Status/ScreenRequests.md#sr-005-экран-танца); полоски участника — коллекция по форме `meters`; действия — элементы действия.

**Инвариант:** участники и полоски сохраняют widgets по `key` между revisions. Нарушение — пересоздание участников после каждого действия.

**Не считается закрытием:**
- фиксированное число слотов участников вместо коллекции;
- `percent` вне `[0..1]`, обрезанный виджетом молча, без правила схемы.

**Done:**
- `textsystem:screen.participant_scene` разрешается в слое `location_content`.
- Меньше двух участников отклоняется схемой.
- `percent` принимается в диапазоне `[0..1]` по правилу `meters` экрана локации.
- Между revisions участники и полоски с теми же `key` сохраняют widgets.
- Layout держит наибольшее число участников из SPS-01 на шести разрешениях.
- Экран показан route активного режима через production-путь.

**Evidence:** content smoke с двумя revisions; отказы схемы; матрица; `run_ue_tests.py`.
