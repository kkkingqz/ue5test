---
title: Image Resource Contract
status: normative
version: 1.6
updated: 2026-08-15
depends_on:
  - ../Architecture/StableIDSpecification.md
decisions:
  - ../ADR/0010-portable-runtime-and-headless-simulation.md
  - ../ADR/0016-png-suffix-image-metadata.md
  - ../ADR/0017-centralized-ui-presentation-paths.md
  - ../ADR/0035-ui-foundation-and-composition.md
---

# Image Resource Contract

> **Владеет:** тремя режимами изображений, конвенцией суффиксов файлов, правилами разрешения `resource_id` и политикой масштабирования растра.
> **Не владеет:** тем, где изображение используется, и раскладкой экрана.
> **Инварианты:** [INV-007](../Architecture/Invariants.md)
> **Реализация:** `Source/GV2/Private/UI/GV2ImageResourceCatalog.cpp`, `GV2ImagePresentation.cpp`, `GV2LayoutConstants.h`, `Resources/`.
> **Проверки:** `GV2.Runtime.Presentation.ImageCatalog*`, `GV2.Runtime.UIKit.ScalingModelAndConstants`.

## Purpose and scope

Документ задаёт canonical metadata, rendering rules bitmap-ресурсов UI и модель масштабирования визуальных примитивов. Он не определяет Screen composition, streaming lifecycle других media kinds или gameplay semantics.

## Ownership and source of truth

- Lua, Definitions и Presentation Snapshot хранят только canonical `resource_id`.
- Project `Resources` tree является authoring source; startup-built `UGV2ImageResourceCatalog` владеет опубликованным mapping `resource_id → runtime texture + render metadata`.
- UE Presentation разрешает resource после repository/catalog validation и до Widget mutation.
- Headless catalog сохраняет только ID, kind и availability metadata, не загружает texture payload и может не хранить UE-specific image geometry metadata.
- Screen Template владеет геометрией принимающего image block.

Raw `/Game/...` locator, `UTexture2D`, `FSlateBrush`, source pixels и render mode запрещены через Lua/C++ boundary.

## Dual Resolution and Scaling Model (ADR-0035)

Для интерфейса действуют два раздельных координатных базиса:

- **Разрешение авторинга растра (4K / 3840 × 2160)**: текстуры, 9-slice маркеры и фоновые тайлы рисуются в 4K для сохранения четкости и отсутствия артефактов при даунскейлинге на меньшие разрешения.
- **Единицы виртуальной раскладки (1080p / 1920 × 1080)**: координаты, отступы и размеры слотов задаются в виртуальных единицах 1080p.
- **Коэффициент пересчета**: `RasterToLayoutScale = 2.0` (`FGV2LayoutConstants::RasterToLayoutScale`).
- **Минимальная цель**: 1280 × 720 (720p).

### Primitive Scale Policy

Каждый визуальный примитив объявляет политику масштабирования (`EGV2PrimitiveScalePolicy`), которая проверяется на совместимость с объявленным `EGV2ImageRenderMode` ресурса до мутации виджета:

| Политика примитива | Совместимый режим ресурса | Поведение |
|---|---|---|
| `FreeStretch` | `tile` | Свободное растяжение текстуры без сохранения пропорций |
| `Tile` | `tile` | Повторение текстуры по X/Y с сохранением исходного размера тайла |
| `NineSlice` | `nine_slice` | Фиксированные углы, растяжение ребер по одной оси, центра по двум |
| `PreserveAspect` | `fixed_aspect` | Сохранение пропорций (aspect ratio) ресурса внутри слота |

Попытка применить ресурс с несовместимым режимом отклоняется на этапе `FGV2ImagePresentation::ResolveAndApply` без модификации свойств виджета.

## Invariants

Поддерживаются ровно три render mode:

| Canonical token | Назначение | Geometry rule |
|---|---|---|
| `fixed_aspect` | Иллюстрация, portrait, item, banner | Block и resource имеют одинаковый immutable width/height ratio; crop и non-uniform stretch запрещены |
| `nine_slice` | Рамка, panel, button surface динамического размера | Corners fixed; edges stretch по одной оси; center растягивается по двум |
| `tile` | Бесшовный pattern/background динамического размера | Texture повторяется по X/Y с фиксированным logical tile size |

Physical bitmap dimensions не определяют размер Widget. Adaptive Screen меняет rect image block, соблюдая его contract.

## Data model and UE API

Canonical file mapping:

```text
Resources/core/resource/image/character_portrait.png
→ core:resource.image.character_portrait

Resources/core/resource/ui/old_paper_tile_256.tile.png
→ core:resource.ui.old_paper_tile_256
```

Каждый directory/file segment обязан соответствовать lowercase Stable ID segment grammar. Scan рекурсивный и сортируется до build; file enumeration order не влияет на результат. Поддерживается только lowercase `.png`.

Render mode кодируется suffix имени source-файла:

| Source filename | Mode | Derived metadata |
|---|---|---|
| `<name>.png` | `fixed_aspect` | ratio = decoded `width / height` |
| `<name>.tile.png` | `tile` | logical `tile_size` = decoded `width × height` |
| `<name>.9.png` | `nine_slice` | borders из однопиксельной marker-рамки; рамка удаляется из runtime texture |

`.tile` и `.9` не входят в `resource_id`. Для `.9.png` верхняя граница обязана содержать ровно один непрерывный чёрный marker run, задающий горизонтально растягиваемую область; левая — один run для вертикальной области. Runtime border widths вычисляются относительно внутреннего bitmap после удаления рамки. JSON sidecar для image metadata запрещён.

`FGV2ImagePresentation.ResolveAndApply` является единственным runtime path `resource_id → resolved brush → UImage mutation`: он разрешает configured catalog, проверяет contract target block и атомарно применяет resolved brush. `UGV2ImageWidgetBase.ApplyImageResource(resource_id)` и approved native composite adapters обязаны делегировать ему. Public raw-brush mutation API отсутствует.

`UGV2ImageWidgetBase.InitialResourceId` может быть задан конкретным Screen Blueprint для статической composition. При `NativePreConstruct` компонент разрешает его тем же `ApplyImageResource` path; Blueprint не обязан дублировать event graph. Пустое значение означает, что resource будет передан динамически.

Image block Blueprint задаёт политику масштабирования `ScalePolicy` (`EGV2PrimitiveScalePolicy`). Для `PreserveAspect` он также может задавать `FixedAspectRatio` и обязан использовать layout constraint с одинаковыми minimum/maximum aspect ratio. Объявленный в ресурсе `RenderMode` является authoring capability и проверяется на совместимость со `ScalePolicy` до применения кисти. Concrete resource не может менять aspect ratio Screen layout-а.

## Processing flow

1. Startup scanner рекурсивно перечисляет `Resources/**/*.png`, определяет mode по suffix и выводит Stable ID без `.tile`/`.9`.
2. Candidate build декодирует PNG, проверяет duplicate entries и mode-specific metadata/marker border.
3. Candidate build один раз создаёт runtime texture и готовый `FSlateBrush`, затем формирует immutable lookup `resource_id → resolved resource`.
4. После полной validation catalog публикуется атомарно; старый catalog не меняется при ошибке.
5. Application bootstrap фиксирует successful build как required readiness prerequisite; только после этого может создавать session candidate.
6. Presentation готовит required resource согласно общему prepare/prefetch lifecycle.
7. Resolver строго проверяет requested Stable ID и выполняет immutable lookup без catalog-wide validation, texture loading или повторного построения brush.
8. Image Widget сверяет совместимость `ScalePolicy` с `RenderMode` и для `PreserveAspect` ratio target block.
9. Только после успешных проверок Widget заменяет brush и applied `resource_id`.

Mapping в Slate:

| Mode | `DrawAs` | Tiling | Additional value |
|---|---|---|---|
| `fixed_aspect` | `Image` | `NoTile` | Desired size сохраняет declared ratio |
| `nine_slice` | `Box` | `NoTile` | Normalized margins из border/source size |
| `tile` | `Image` | `Both` | `ImageSize = tile_size` |

## Failure and recovery semantics

Invalid ID, missing texture, duplicate ID, unknown mode, non-positive ratio/tile size, collapsed nine-slice center и target incompatibility возвращают presentation failure до Widget mutation. Required missing resource блокирует owning prepare/apply. Optional resource применяется через `FGV2ImagePresentation::ResolveOptionalAndApply`: failed requested ID записывается в UE diagnostic log, после чего тот же resolver пытается применить type-compatible placeholder. Если placeholder отсутствует или несовместим с target block, apply блокируется как configuration error.

Startup Image Catalog является required application dependency. Failed configured catalog build обязан оставить Runtime Subsystem в non-ready bootstrap state и запретить создание/публикацию session `Ready`. Атомарное сохранение ранее опубликованного catalog защищает существующего consumer-а от partial mutation, но не разрешает новой session использовать stale catalog после failed rebuild.

## Compatibility and evolution

- Новый PNG и замена PNG с сохранением filename/mode/geometry contract являются data-only change.
- Изменение mode или `fixed_aspect_ratio` опубликованного `resource_id` является несовместимой сменой смысла; требуется новый ID.
- Добавление render mode требует ADR и синхронного обновления UE/headless validators.
- Directory/file rename меняет `resource_id`; опубликованный ID запрещено молча переиспользовать.
- Resources stage-ятся как `NonUFS`; rescan active session запрещён, изменения применяются при следующем startup/session restart.

## Verification

- Scanner проверяет mapping plain, `.tile.png` и `.9.png`, включая удаление mode suffix из ID.
- Validator принимает по одному корректному fixture каждого режима и отклоняет malformed marker metadata.
- `Resources/core/resource/ui/old_paper_tile_256.tile.png` разрешается как `core:resource.ui.old_paper_tile_256`, `tile`, `256×256`, tiling по обеим осям.
- Resolver создаёт ожидаемые `DrawAs`, tiling, margins и logical tile size.
- Repeated resolve использует подготовленный immutable lookup и возвращает эквивалентный brush; runtime lookup не перечисляет entries и не перестраивает brush.
- Scaling benchmark через public `Resolve()` для 10, 1 000 и 10 000 synthetic entries не демонстрирует линейного роста относительно размера catalog.
- Widget отклоняет несовместимый mode/ratio без изменения предыдущего resource.
- Непустой `InitialResourceId` использует тот же resolver и mode validation, что и динамическое применение.
- Lua source и portable DTO не содержат texture path, brush или render mode.
- `WBP_Image` сохраняет native parent `UGV2ImageWidgetBase`; concrete fixed-aspect Screen block проверяется на matching aspect constraint при добавлении такого поля.
- Automation с invalid configured resource root подтверждает, что Lua VM/Screen не публикуются и session не достигает `Ready`; после восстановления settings catalog снова успешно строится.
