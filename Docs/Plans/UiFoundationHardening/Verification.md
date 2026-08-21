---
title: UI Foundation Hardening Verification
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - CoreRepeater.md
  - PresentationPipelines.md
  - LocationCompositeSemantics.md
  - ../LocationScreen/Verification.md
---

# M4 — Verification

> **Материализует:** раздел 13 [предложения](../../Proposals/CoreUIBaselineAndScalingProposal.md) и раздел 18 [предложения](../../Proposals/GameplayLocationScreenProposal.md).
> **Задачи:** UIH-13…16.
> **Результат:** acceptance основан на фактическом UMG behavior; после этапа
> можно честно закрыть LocationScreen GLS-14…16.

## Задачи

- [x] **UIH-13 — Реальная viewport/layout matrix**
  - Зависимости: UIH-01…12.
  - Done: automation создаёт настоящий зарегистрированный
    `WBP_LocationScreen`, применяет полноценный candidate document,
    выполняет layout/prepass на каждом viewport и проверяет resulting widget
    geometry:
      - `3840×2160`;
      - `2560×1440`;
      - `1920×1080`;
      - `1280×720`;
      - `3440×1440`;
      - `2560×1080`.
  - Проверяются:
      - TopBar внутри viewport;
      - PlayerStatusPanel внутри своих constraints;
      - SceneView имеет ненулевую доступную область;
      - background не crop/stretch;
      - character aspect сохраняется;
      - CommandPanel wrap/reflow сохраняет все entries;
      - все обязательные controls имеют достижимую geometry;
      - на 21:9 основную дополнительную ширину получает SceneView.
  - Fixture: минимум один worst-case с длинной pseudolocale и количеством
    команд, достаточным для переноса.
  - Запрещено считать проверкой простой цикл по числовой таблице размеров.
  - Evidence: dedicated Unreal automation test.

- [x] **UIH-14 — Rendering conformance на реальных widgets**
  - Зависимости: UIH-05…08, UIH-13.
  - Done: instantiated Text, RichText, Button, InputField и Dropdown с одним
    semantic style на одном viewport дают один effective font size;
    minimum readable threshold реально наблюдаем; images проверяют
    PreserveAspect/Tile/NineSlice на resulting widget/brush state.
  - Negative: incompatible graphics resource сохраняет предыдущий valid state.
  - Evidence: Unreal automation.

- [x] **UIH-15 — Сквозной LocationScreen transition**
  - Зависимости: UIH-02…12.
  - Done: Tavern → Market выполняется через semantic action/Command Dispatcher;
    `(layer, screen_id, instance_key)` сохраняют identity Screen;
    сам `WBP_LocationScreen` переиспользован;
    неизменившиеся repeated entries переиспользованы по key;
    location/background/characters/commands обновились;
    gameplay mutation происходит только в Lua.
  - Hosts: scenario сохраняет одинаковую gameplay семантику UE и Headless;
    UI-specific identity assertions выполняются UE automation.
  - Evidence: Lua spec + runtime automation.

- [x] **UIH-16 — Закрыть старые acceptance claims новым evidence**
  - Зависимости: UIH-13, UIH-14, UIH-15.
  - Done:
      - `GLS-14` отмечен выполненным только по реальному layout matrix;
      - `GLS-15` — по transition scenario;
      - `GLS-16` — по подтверждённому inventory pipelines/components;
      - `Docs/Status/ImplementationStatus.md` синхронизирован;
      - активный `LocationScreen` plan завершён только если его полный DoD
        теперь доказан;
      - старый `UiFoundation` archive не переписывается задним числом;
      - новый plan после полного завершения архивируется обычной
        двухкоммитной процедурой.
  - Full gate:
      - clean GV2Editor build;
      - полный `ctest`;
      - `gv2-headless --self-test`;
      - `--check-scripts`;
      - golden deterministic run;
      - `validate_docs.py`;
      - полный релевантный Unreal automation suite.
  - Evidence: test reports, updated active plan/status docs, source commit.

## Проверка milestone

- [x] Реальный LocationScreen проверен на всех шести viewport sizes.
- [x] 1280×720 пригоден к использованию.
- [x] Pseudolocale не делает controls недостижимыми.
- [x] DPI scaling подтверждён на rendered widgets.
- [x] Graphics policy подтверждена на rendered widgets.
- [x] Tavern → Market переиспользует Screen Instance.
- [x] Повторяемые children переиспользуются по key.
- [x] GLS-14…16 имеют новое фактическое evidence.
- [x] Полный project gate зелёный.
