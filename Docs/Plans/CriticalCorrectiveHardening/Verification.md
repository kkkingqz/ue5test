---
title: Critical Corrective Hardening — Verification
status: active
version: 1.0
updated: 2026-08-22
depends_on:
  - RepeaterAtomicity.md
  - LocationCompositeCorrectness.md
  - GraphicsContract.md
  - ../UiFoundationHardening/Verification.md
  - ../LocationScreen/Verification.md
---

# M4 — Verification

> **Материализует:** проверку фактического UMG/Slate поведения виджетов.
> **Задачи:** CCF-16…21.
>
> **Результат:** acceptance основан на actual geometry, actual consumer state,
> actual brush и реальном transition scenario.

## Задачи

- [ ] **CCF-16 — Проверять actual arranged geometry на шести viewport sizes**
  - Зависимости: CCF-01…15.
  - Done:
    - test создаёт реальный зарегистрированный `WBP_LocationScreen`;
    - выполняется Slate/UMG prepass/layout;
    - assertions читают cached/arranged geometry, а не только DesiredSize;
    - проверяются все шесть accepted viewport sizes.
  - Evidence:
    - dedicated Unreal automation.

  ### Viewport matrix

  ```text
  3840 × 2160
  2560 × 1440
  1920 × 1080
  1280 × 720
  3440 × 1440
  2560 × 1080
  ```

  ### Пошагово

  1. Найти существующий UIH viewport test.
  2. Не создавать второй независимый matrix test, если можно усилить текущий.
  3. Для каждого размера:
     - resize test window;
     - добавить actual `WBP_LocationScreen`;
     - выполнить требуемый Slate prepass/layout step;
     - получить root geometry;
     - получить geometry TopBar;
     - SceneView;
     - PlayerStatusPanel;
     - CommandPanel.
  4. Проверить:
     ```text
     width > 0
     height > 0
     left/top внутри viewport
     right/bottom внутри viewport с допустимым rounding tolerance
     ```
  5. Не фиксировать exact pixels, если contract задаёт только constraints.
  6. Failure message должен содержать viewport и actual rectangle.

- [ ] **CCF-17 — Доказать фактическую пригодность 1280×720**
  - Зависимости: CCF-16.
  - Done:
    - обязательные command widgets реально имеют geometry;
    - controls не исчезли за границей allocated CommandPanel;
    - long/pseudolocale fixture не делает обязательные controls недостижимыми.
  - Negative:
    - `EntryCount == N` сам по себе не является доказательством reachability.
  - Evidence:
    - actual geometry assertions для command widgets.

  ### Пошагово

  1. Использовать existing worst-case command fixture.
  2. На 1280×720 получить geometry каждого command child.
  3. Проверить ненулевой размер.
  4. Проверить расположение относительно CommandPanel/scroll/wrap host согласно
     уже существующему layout contract.
  5. Если используется WrapBox без scroll — все обязательные widgets должны
     оказаться в allocated panel.
  6. Не добавлять новый scroll functionality только ради test.

- [ ] **CCF-18 — Доказать фактическое ultrawide allocation**
  - Зависимости: CCF-16.
  - Done:
    - дополнительная ширина 21:9 действительно увеличивает SceneView;
    - assertion основан на actual geometry.
  - Evidence:
    - comparison 16:9 vs 21:9.

  ### Пошагово

  1. Выбрать два существующих viewport cases с сопоставимой логикой layout.
  2. Считать:
     ```text
     SceneView width
     PlayerStatus width
     CommandPanel width
     ```
  3. Проверить:
     ```text
     SceneViewWidthUltrawide > SceneViewWidthStandard
     ```
  4. Не вводить новую процентную норму, если её нет в existing contract.

- [ ] **CCF-19 — Проверить actual font size во всех существующих consumers**
  - Зависимости: CCF-16.
  - Done:
    - Text;
    - RichText;
    - Button;
    - InputField;
    - Dropdown;
    - с одним semantic style получают одинаковый effective size.
  - Evidence:
    - instantiated-widget automation на нескольких viewport heights.

  ### Высоты

  ```text
  720
  1080
  1440
  2160
  ```

  ### Пошагово

  1. Инстанцировать реальные существующие consumer classes.
  2. Применить одинаковый semantic style, например существующий `body`.
  3. Применить текущий viewport/DPI context.
  4. Считать **реально установленный** font size из каждого consumer.
  5. Сравнить с expected central pipeline result.
  6. Проверить minimum readable threshold на низком viewport.
  7. Failure message:
     ```text
     consumer=<name>, viewport=<height>, expected=<n>, actual=<m>
     ```
  8. Если один consumer расходится, исправлять только его обход общего
     pipeline; не создавать второй scaling helper.

- [ ] **CCF-20 — Проверить NineSlice через реальный resulting brush**
  - Зависимости: CCF-15.
  - Done:
    - используется реальный existing NineSlice resource;
    - assertion читает resulting brush;
    - проверяется `DrawAs` и `Margin`.
  - Evidence:
    - rendering conformance automation.

  ### Важно

  Недостаточно:

  ```text
  IsCompatible(resource, NineSlice) == true
  ```

  Нужен actual apply и resulting brush state.

- [ ] **CCF-21 — Усилить Tavern → Market transition verification**
  - Зависимости: CCF-01…20.
  - Done:
    - используется существующий semantic command перехода;
    - `WBP_LocationScreen` pointer сохраняется;
    - exact location/background/commands/characters становятся Market state;
    - removed entries действительно удаляются;
    - new entries появляются;
    - same-key repeated entry reuse проверяется pointer assertion, если такой
      entry реально есть в текущем fixture.
  - Negative:
    - не менять gameplay data специально, чтобы искусственно создать same-key
      entry;
    - не добавлять новый travel command.
  - Evidence:
    - UE automation;
    - existing headless/parity scenario для той же gameplay semantics.

  ### Пошагово

  1. Найти существующий transition test и semantic binding.
  2. До transition сохранить:
     ```text
     LocationScreen pointer
     current location id/title
     background resource
     command keys
     character keys
     relevant repeated widget pointers
     ```
  3. Отправить существующее semantic action.
  4. После transition проверить exact Market values.
  5. Проверить тот же Screen pointer.
  6. Проверить old removed keys отсутствуют.
  7. Проверить new keys присутствуют.
  8. Если есть key, одинаковый до/после, сравнить pointer.
  9. Если такого key нет — reuse остаётся покрытым M1 integration test; game
     data не менять.

## Проверка milestone

- [ ] Все 6 viewports проверяют actual geometry.
- [ ] 720p reachability доказана geometry assertions.
- [ ] 21:9 SceneView expansion доказан geometry assertions.
- [ ] Text pipeline проверен на actual consumers.
- [ ] NineSlice проверен actual brush.
- [ ] Tavern→Market проверяет exact state и Screen reuse.
- [ ] Полный relevant Unreal automation зелёный.
