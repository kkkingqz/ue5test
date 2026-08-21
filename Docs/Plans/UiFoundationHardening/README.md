---
title: UI Foundation Hardening Plan
status: active
version: 1.0
updated: 2026-08-21
depends_on:
  - ../../Proposals/CoreUIBaselineAndScalingProposal.md
  - ../../Proposals/GameplayLocationScreenProposal.md
  - ../LocationScreen/README.md
  - ../../UI/ScreenTemplates.md
  - ../../UI/WidgetRegistry.md
  - ../../UI/ImageResources.md
decisions:
  - ../../ADR/0013-unified-text-pipeline.md
  - ../../ADR/0017-centralized-ui-presentation-paths.md
  - ../../ADR/0035-ui-foundation-and-composition.md
---

# План усиления UI Foundation

> **Исправляет:** расхождения между принятым Core UI contract и фактической
> реализацией, обнаруженные при первом реальном потребителе — `LocationScreen`.
>
> **Задачи:** UIH-01…16.
>
> **Результат:** Core Repeater действительно является единым механизмом
> повторяемого контента, DPI scaling работает в реальном text pipeline,
> graphics scaling имеет один источник поведения, LocationScreen не имеет
> собственных обходных путей, а матрица разрешений проверяет настоящий UMG
> layout, а не только числовые константы.

## Цель

`UiFoundation` был реализован и архивирован до появления первого полноценного
игрового экрана. `LocationScreen` стал первым интеграционным потребителем и
обнаружил несколько случаев, где API и тесты формально существуют, но
заявленный contract материализован не полностью.

План не вводит новую UI-архитектуру. Его задача — привести существующую
реализацию к уже принятым решениям:

```text
Core primitives
      ↓
единые text / graphics / repeated-content pipelines
      ↓
textsystem composites
      ↓
LocationScreen
```

## Состояние на входе

Проверено по текущему коду.

| Область | Фактическое состояние |
|---|---|
| Core `ListView` / Repeater | Тип существует, но не выполняет reconciliation элементов |
| Повторяемые команды | `CommandPanel` самостоятельно использует `UWrapBox + FGV2KeyedCollection` |
| Item/effect icons | Имеют отдельный локальный reconciliation path |
| Character layer | ViewModel содержит массив, renderer использует только `[0]` |
| Text DPI model | Кривая и minimum существуют, но основной `TextPipeline::Apply` их не применяет |
| Graphics scaling | Одновременно существуют `ScalePolicy` и `AcceptedRenderMode`; второй может менять первый |
| Resolution matrix | Тестирует размеры/aspect/text-scale математически, но не geometry реального экрана |
| Reset semantics | Часть composite widgets сбрасывает модель, но оставляет старое визуальное состояние |
| Placeholder semantics | Пустой character resource скрывает слой вместо silhouette placeholder |
| Field validation | Composite widgets неодинаково проверяют `field_id` и `schema_id` |

## Принятые решения

- Архивный `UiFoundation` не переписывается: это исторический результат.
- Новый план исправляет фактическую реализацию и актуальные normative contracts.
- `FGV2KeyedCollection` остаётся низкоуровневым механизмом reconciliation,
  но gameplay/textsystem composites не владеют собственными альтернативными
  механизмами повторяемого контента.
- `UGV2ListViewWidgetBase` становится реальным Core Repeater, а не только
  контейнером с `ClearEntries`.
- Layout повторяемого контейнера и identity/reconciliation разделены:
  вертикальный, горизонтальный и wrap-host используют один механизм identity.
- `ScalePolicy` является единственным объявлением поведения визуального
  примитива. Render mode приходит из ресурса и только проверяет допустимость.
- DPI scaling применяется в реальном общем text rendering path.
- Тест разрешения считается пройденным только если построен реальный widget
  tree и проверена его resulting geometry.
- `ResetScreenField()` обязан сбрасывать и captured state, и видимое состояние.
- Нельзя молча принимать массив и отображать только первый элемент.

## Границы

Входят:

- Core Repeater;
- миграция повторяемых частей `LocationScreen`;
- фактическое подключение DPI-aware text rendering;
- устранение двойного graphics policy;
- исправление semantics composite widgets;
- реальная viewport/layout automation;
- завершение `LocationScreen` GLS-14…16.

Не входят:

- новые типы экранов;
- dialogue/inventory/character sheet;
- arbitrary scene graph;
- визуальный редактор экранов;
- новые gameplay mechanics;
- animation system;
- breakpoint framework;
- изменение Lua ownership/gameplay authority.

## Milestones

- [x] M1 — [Core Repeater](CoreRepeater.md): один механизм identity и reconciliation для повторяемого UI. UIH-01…04.
- [x] M2 — [Presentation Pipelines](PresentationPipelines.md): реальный DPI-aware text path и один graphics scale contract. UIH-05…08.
- [x] M3 — [Location Composite Semantics](LocationCompositeSemantics.md): repeated characters/meters, placeholders, reset и field validation. UIH-09…12.
- [ ] M4 — [Verification](Verification.md): реальные layout-тесты, transition scenario и закрытие GLS-14…16. UIH-13…16.

## Критический путь

```text
M1 ──────┐
         ├──► M3 ───► M4
M2 ──────┘
```

M1 и M2 независимы.

M3 требует обоих: после появления настоящего Repeater переводятся коллекции
LocationScreen, а text/graphics leaves уже обязаны использовать исправленные
центральные pipelines.

M4 не начинается как acceptance gate до завершения M1–M3.

## Связь с LocationScreen

`Docs/Plans/LocationScreen/Verification.md` остаётся владельцем GLS-14…16.

До завершения UIH:

```text
GLS-14 — blocked by UIH-13/UIH-14
GLS-15 — blocked by UIH-15
GLS-16 — blocked by UIH-16
```

Задачи не дублируются и не отмечаются выполненными по старым synthetic tests.

## Общие правила выполнения

1. Исправление переиспользует уже принятые contracts; новый ADR нужен только
   если приходится менять архитектурный инвариант.
2. Нельзя переносить gameplay-specific сущность в `core`.
3. Ни один composite не создаёт собственного text, image или repeated-content
   rendering path.
4. `CanApply*` не мутирует widget.
5. Ошибка validation/apply не оставляет частично изменённый визуальный state.
6. Все identity rules имеют negative tests.
7. Все изменения `.uasset` выполняются через принятый Unreal authoring path,
   с compile и save.
8. Проверка разрешения обязана строить реальный widget tree.
9. Checkbox отмечается только после прохождения указанного Evidence.
10. Изменение observable behavior синхронно отражается в normative contract.

## Итоговый Definition of Done

- [ ] `UGV2ListViewWidgetBase` реально выполняет keyed reconciliation.
- [ ] CommandPanel не владеет собственным `ButtonsByKey` reconciliation path.
- [ ] Item/effect/character/meter collections используют Core Repeater.
- [ ] Reorder с тем же key переиспользует существующий widget.
- [ ] DPI curve реально влияет на plain и rich text rendering.
- [ ] Один semantic text style имеет одинаковый effective size во всех consumers.
- [ ] `ScalePolicy` является единственным источником graphics behavior.
- [ ] Render mode ресурса только разрешает или запрещает выбранную policy.
- [ ] Location composites имеют единый field validation и reset contract.
- [ ] Empty/missing optional resources следуют принятой placeholder policy.
- [ ] Ни один массив presentation entries не обрезается молча до первого элемента.
- [ ] Реальный `WBP_LocationScreen` проходит шесть целевых viewport sizes.
- [ ] На 1280×720 все обязательные команды достижимы.
- [ ] На 21:9 дополнительное пространство получает SceneView.
- [ ] Tavern → Market переиспользует экземпляр LocationScreen.
- [ ] GLS-14…16 закрыты новым evidence.
- [ ] Полные CTest, headless, script checks, docs validation и Unreal automation зелёные.
