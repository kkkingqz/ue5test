---
title: Measured and Clean Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../UI/WidgetRegistry.md
---

# M3 — Measured and Clean

> **Материализует:** находки ревью OPEN-02, NEW-05, NEW-04 в части грамматики, NEW-01, OPEN-04.
> **Задачи:** BAI-08…11.
> **Результат:** проверки измеряют то, что заявляют, а объявленная поверхность используется.

## Результат этапа

Этап независим от M1 и M2 и не пересекается с ними по файлам, кроме общего файла тестов. Работа здесь однородная и не требует понимания границы Lua → C++: несогласованная грамматика, два теста, доказывающих меньше заявленного, и снятие мёртвой поверхности.

## Задачи

- [x] **BAI-08 — Одна грамматика ключа повторяемого элемента**
  - [UI Document](../../UI/UIDocumentAndReconciliation.md) требует `[a-z0-9_.-]+`, но тот же раздел приводит примеры `rh:item.weapon.iron_sword` и `actor@42`, которые этой грамматике не удовлетворяют. `IsValidRepeatedElementKey` принимает дополнительно `@` и `:`. Расходятся все три: правило, примеры и код.
  - Done: выбрана одна грамматика, покрывающая фактические источники ключа; она записана в контракте, реализована в C++, применяется валидацией на стороне Lua и проверяется conformance-тестом на положительном и отрицательном примере; примеры в контракте грамматике удовлетворяют.
  - Evidence: `Docs/UI/UIDocumentAndReconciliation.md`, `Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp`, `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

- [ ] **BAI-09 — Типографский conformance читает фактический размер**
  - `GV2WidgetSemanticFontSizeContractTests.cpp` подключает пять классов-потребителей, но не создаёт **ни одного** виджета: во всём файле ноль вызовов `CreateWidget` и `NewObject`. Для каждого «потребителя» вызывается `UGV2TextPipeline::ResolveStyleForHeight` и сравнивается результат с ним же. Регрессия в любом `ApplyCentralStyle` этот тест не сломает.
  - Done: тест создаёт виджеты каждого из пяти типов, применяет к ним стиль production-путём и читает размер шрифта из фактического renderer-контрола; проверка краснеет при внесении регрессии в один `ApplyCentralStyle`; сравнение helper-а с самим собой снято, а не дополнено.
  - Evidence: `Source/GV2/Private/Tests/GV2WidgetSemanticFontSizeContractTests.cpp`.

- [ ] **BAI-10 — Границы на 1280×720 проверяются по обеим осям**
  - `LocationScreenViewportMatrix` измеряет выделенную геометрию, но на 720p проверяет только `BtnSize.X > 0`, `BtnSize.Y > 0` и `BtnLocalPos.Y + BtnSize.Y <= 720`. Левая граница, правая граница и вхождение в панель не проверяются, поэтому горизонтальное переполнение остаётся зелёным. Текст кнопок — `Command #%d`, то есть нагрузки на раскладку нет.
  - Done: на минимальном разрешении проверяется вхождение прямоугольника кнопки в видимую область по обеим осям и в прямоугольник `CommandPanel`; fixture использует длинный текст либо псевдолокаль, дающую расширение; тест краснеет при искусственном увеличении длины подписи сверх доступной ширины.
  - Evidence: `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

- [ ] **BAI-11 — Снятие побочных эффектов и мёртвой поверхности**
  - Пять `BlueprintPure` геттеров (`GetItemRepeater`, `GetEffectRepeater`, `GetMeterRepeater`, `GetCharacterRepeater`, `GetRepeater`) вызывают `Resolve*`, которые делают `NewObject` и `SetContainerPanel`. Отдельно: `StaminaMeter` и `Character` остаются устаревшей поверхностью совместимости, а `ResourceIcon` во всех трёх путях TopBar только сворачивается и не показывается никогда.
  - Done: ни один `BlueprintPure` не создаёт объектов — геттеры либо перестают быть `pure`, либо возвращают уже созданный репитер без ленивой инициализации; `ResourceIcon` удалён либо получает путь, в котором отображается; `StaminaMeter` и `Character` удалены либо помечены `deprecated` с указанием замены; удаление привязываемых свойств согласовано с ассетами через `unreal-mcp`.
  - Evidence: `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`, `Source/GV2/Private/UI/GV2LocationCompositeWidgetBases.cpp`, `Content/TextSystem/UI/Widgets/`.

## Проверка milestone

- [ ] Грамматика ключа одна во всех четырёх местах.
- [ ] Оба переписанных теста краснеют на внесённой регрессии.
- [ ] Объявленная поверхность либо используется, либо снята.
