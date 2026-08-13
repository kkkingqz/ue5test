---
title: "ADR-0014: Three-Mode Image Resources"
status: superseded
date: 2026-08-12
superseded_by: 0015-filesystem-discovered-image-resources.md
---

# ADR-0014: Three-Mode Image Resources

Решение заменено [ADR-0015](0015-filesystem-discovered-image-resources.md): три render mode сохранены, но manual Data Asset catalog заменён deterministic filesystem discovery.

## Context

Responsive Screen Templates не должны выбирать размер bitmap по каждому viewport или неявно растягивать artwork. Один и тот же `resource_id` обязан иметь предсказуемую геометрию, а Lua/headless runtime не должны знать UE brush, texture path или physical pixels layout-а.

## Decision

- Image Resource Catalog поддерживает ровно три render mode: `fixed_aspect`, `nine_slice`, `tile`.
- `fixed_aspect` объявляет immutable aspect ratio. Принимающий Screen block обязан фиксировать то же ratio; crop и non-uniform stretch запрещены.
- `nine_slice` объявляет border insets в source pixels. Углы не растягиваются, края растягиваются по одной оси, центр заполняет остаток.
- `tile` объявляет положительный repeat size в logical Slate units. Texture повторяется по обеим осям и не растягивается до размера блока.
- Lua и portable DTO передают только `resource_id`. Render mode и physical asset locator принадлежат UE catalog.
- Несовместимость render mode/aspect ratio с принимающим UI block отклоняется до Widget mutation.

## Consequences

- Aspect ratio является частью semantic presentation resource contract и не выводится из текущего viewport.
- Wide/compact layout меняет размер и положение image block, но не способ отрисовки ресурса.
- Новый image resource добавляется data-only. Добавление четвёртого render mode требует нового решения и обновления generic resolver/renderer.
- Свободное пространство адаптивного Screen заполняется layout, `nine_slice`, `tile` или цветом, но не обрезкой `fixed_aspect` artwork.

## Rejected alternatives

- Отдельные bitmap/Screen Blueprint для каждого разрешения или aspect ratio: создают дублирование и drift.
- Универсальные `cover`/`contain`/`stretch` flags из Lua: переносят physical rendering policy через portable boundary.
- Автоматически выводить ratio из загруженной texture: замена payload может незаметно изменить layout contract.
