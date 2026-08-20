---
title: Read Surface Tasks
status: active
version: 1.0
updated: 2026-08-20
depends_on:
  - EditorAdapter.md
  - ../../Architecture/DefinitionEnvelopeAndSchemaRules.md
---

# M3 — Read Surface

> **Материализует:** разделы 6—16 [предложения](../../Proposals/ContentEditorPluginProposal.md).
> **Задачи:** CED-09…12.
> **Результат:** определение открывается формой, построенной из схемы, со ссылками в обе стороны.

## Результат этапа

Редактор становится полезным на чтение раньше, чем научится писать. Это намеренно: форма по схеме и панель ссылок проверяются без риска испортить контент.

## Задачи

- [x] **CED-09 — Браузер определений**
  - Зависимости: CED-06.
  - Done: левая панель перечисляет определения по kind из индекса библиотеки; собственной логики наложения пакетов браузер не содержит; поддерживаются поиск, группировка по kind, указание пакета, копирование Stable ID и признак несохранённых правок; выбор определения ведёт к загрузке его канонического представления.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2DefinitionBrowser.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2DefinitionBrowser.cpp`, `Source/GV2ContentEditor/Private/Testing/ReadSurfaceConformance.cpp`.

- [x] **CED-10 — Реестр адаптеров полей**
  - Зависимости: CED-09.
  - Done: заведён внутренний реестр адаптеров, сопоставляющий вид поля схемы с descriptor-ом представления; единый Slate renderer реализует scalar, enum, slider, typed definition/resource/text pickers, reference arrays и recursively flattened simple objects; каноничной editor model не вводится, реестр не является публичной точкой расширения.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/FieldAdapterRegistry.h`, `Source/GV2ContentEditor/Private/FieldAdapterRegistry.cpp`, `Source/GV2ContentEditor/Private/Testing/ReadSurfaceConformance.cpp`.

- [x] **CED-11 — Форма по схеме и метаданные представления**
  - Зависимости: CED-10.
  - Done: форма строится из описания схемы, а не из захардкоженного списка полей — добавление поля в схему не требует правки C++; `schemas/<name>.ui.json5` используется для подписи, описания, категории, порядка и подсказки виджета; подсказка выбирает адаптер, но не меняет семантику схемы; метаданные представления не влияют на `content_hash`, что подтверждается существующим гейтом.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/SchemaFormModel.h`, `Source/GV2ContentEditor/Private/SchemaFormModel.cpp`, `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2DefinitionProperties.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2DefinitionProperties.cpp`, `Source/GV2ContentEditor/Private/Testing/ReadSurfaceConformance.cpp`.

- [x] **CED-12 — Панель ссылок**
  - Зависимости: CED-11.
  - Done: панель показывает исходящие и входящие ссылки из существующего механизма ссылок; активация ссылки переводит к определению, если оно доступно; ссылочные поля отображаются типизированным выбором совместимых целей, а не строкой; в канонические данные записывается Stable ID в форме, установленной действующим contract.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/ReferenceScanner.h`, `Source/GV2ContentEditor/Private/ReferenceScanner.cpp`, `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2ReferencePanel.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2ReferencePanel.cpp`, `Source/GV2ContentEditor/Private/Testing/ReadSurfaceConformance.cpp`.

## Проверка milestone

- [x] Определение открывается формой, построенной из схемы.
- [x] Добавление поля в схему не требует правки C++ редактора.
- [x] Ссылочное поле выбирается из совместимых целей.
- [x] Ссылки отображаются в обе стороны и переводят к определению.
