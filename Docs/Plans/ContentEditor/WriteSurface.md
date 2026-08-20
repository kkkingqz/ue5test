---
title: Write Surface Tasks
status: active
version: 1.0
updated: 2026-08-20
depends_on:
  - ReadSurface.md
  - ../../Architecture/GameDataRepositoryContract.md
---

# M4 — Write Surface

> **Материализует:** разделы 20—28 [предложения](../../Proposals/ContentEditorPluginProposal.md).
> **Задачи:** CED-13…16.
> **Результат:** правка через редактор даёт тот же файл, что та же правка через CLI.

## Результат этапа

Замыкается основной цикл: открыть, изменить, сохранить, проверить, увидеть результат.

## Задачи

- [x] **CED-13 — Модель изменённых полей и сохранение**
  - Зависимости: CED-11, CED-03.
  - Done: редактор отслеживает исходное значение, текущее значение и признак изменения; сохранение собирает набор изменённых полей и применяет его **одной** операцией библиотеки; несколько независимых записей не выполняются ни при каких условиях; после успеха выполняется authoritative-валидация, каноническое представление перечитывается, ссылки обновляются, признаки изменения снимаются; автосохранение не вводится, доступны явные сохранение и перечитывание.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/GV2EditorAdapter.h`, `Source/GV2ContentEditor/Private/GV2EditorAdapter.cpp`, `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2DefinitionProperties.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2DefinitionProperties.cpp`, `Source/GV2ContentEditor/Private/Testing/WriteSurfaceConformance.cpp`.

- [x] **CED-14 — Создание, дублирование, удаление, переименование**
  - Зависимости: CED-13.
  - Done: создание использует операцию библиотеки и значения по умолчанию из действующего contract, а не собственную логику редактора; дублирование вызывает одну операцию библиотеки; удаление сначала показывает входящие ссылки и не допускает висящих ссылок; переименование выполняется операцией библиотеки и не содержит собственного распространения ссылок; после каждой операции выполняется валидация и обновление представлений.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/GV2EditorAdapter.h`, `Source/GV2ContentEditor/Private/GV2EditorAdapter.cpp`, `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2DefinitionBrowser.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2DefinitionBrowser.cpp`, `Source/GV2ContentEditor/Private/Testing/WriteSurfaceConformance.cpp`.

- [x] **CED-15 — Валидация и панель проблем**
  - Зависимости: CED-13.
  - Done: адаптер поля может отклонить очевидно неверный ввод немедленно, но это не считается достаточным; authoritative-валидация выполняется всегда и не воспроизводится редактором; диагностика отображается отдельной панелью первого класса; двойной щелчок выбирает определение и наводит на соответствующее поле, когда сопоставление возможно.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2DiagnosticsPanel.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2DiagnosticsPanel.cpp`, `Source/GV2ContentEditor/Public/GV2ContentEditor/Widgets/SGV2ContentEditorTab.h`, `Source/GV2ContentEditor/Private/Widgets/SGV2ContentEditorTab.cpp`, `Source/GV2ContentEditor/Private/Testing/WriteSurfaceConformance.cpp`.

- [x] **CED-16 — Совпадение с CLI**
  - Зависимости: CED-14, CED-15.
  - Done: тест применяет одну и ту же правку через редакторский адаптер и через CLI и сравнивает файлы побайтово, включая комментарии и форматирование; расхождение является дефектом библиотеки, а не редактора; проверяется также, что отказ в середине набора оставляет файл исходным, и что запись по устаревшему представлению отклоняется.
  - Evidence: отчёт CTest, тесты редакторского модуля (`RunWriteSurfaceConformance()`).

## Проверка milestone

- [x] Сохранение набора полей атомарно.
- [x] Правка через редактор и через CLI дают одинаковый файл.
- [x] Удаление не оставляет висящих ссылок.
- [x] Диагностика переводит к полю.
- [x] Автосохранение отсутствует.
