---
title: Editor Adapter Tasks
status: active
version: 1.0
updated: 2026-08-20
depends_on:
  - AuthoringLibrary.md
  - ../../Architecture/SystemContextAndComponents.md
---

# M2 — Editor Adapter

> **Материализует:** разделы 4, 5 и 34 [предложения](../../Proposals/ContentEditorPluginProposal.md).
> **Задачи:** CED-05…08.
> **Результат:** редактор получает единственную точку обращения к контенту и не заводит второй модели.

## Результат этапа

Появляется редакторский модуль и адаптер над библиотекой. Окон ещё нет; есть проверяемый слой, поверх которого они строятся.

## Задачи

- [x] **CED-05 — Редакторский модуль**
  - Зависимости: CED-02.
  - Done: заведён модуль, существующий только в редакторской сборке; он зависит от библиотеки авторинга, `GV2ContentCore` и `GV2ContentHostSupport`; runtime и headless зависимости на него не получают, что проверяется сборкой shipping-конфигурации; модуль не содержит игровых правил и не создаёт `UObject`-модели геймплея.
  - Evidence: `Source/GV2ContentEditor/`, `Source/GV2ContentEditor/GV2ContentEditor.Build.cs`, `GV2.uproject`, `Source/GV2Editor.Target.cs`, отчёт сборки.

- [x] **CED-06 — Адаптер и модель состояния**
  - Зависимости: CED-05.
  - Done: адаптер является единственной точкой обращения редактора к контенту; в памяти хранятся только загруженное каноническое представление, текущие значения полей, набор изменённых полей, выбор и временное состояние виджетов; второй изменяемой модели контента и собственной синхронизации не заводится; транспорт скрыт за адаптером.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/GV2EditorAdapter.h`, `Source/GV2ContentEditor/Private/GV2EditorAdapter.cpp`, `Source/GV2ContentEditor/Private/Testing/EditorAdapterConformance.cpp`, `Source/GV2/Private/Tests/GV2ContentEditorTests.cpp`.

- [x] **CED-07 — Устаревшее представление в интерфейсе**
  - Зависимости: CED-06, CED-04.
  - Done: адаптер хранит признак состояния файла вместе с загруженным представлением и передаёт его в операцию записи; отказ по устаревшему представлению доходит до интерфейса как отдельный случай, а не как ошибка валидации; предлагается перечитывание с явной судьбой несохранённых правок; изменение обнаруживается и без попытки записи.
  - Evidence: `Source/GV2ContentEditor/Private/GV2EditorAdapter.cpp`, `Source/GV2ContentEditor/Private/Testing/EditorAdapterConformance.cpp`.

- [x] **CED-08 — Структурная диагностика**
  - Зависимости: CED-06.
  - Done: диагностика проходит от библиотеки до интерфейса в структурном виде — уровень, типизированный код, сообщение, файл, строка, колонка, Stable ID, путь к полю; текст сообщения не разбирается регулярными выражениями ни на одном шаге; отсутствие части полей допустимо и не ломает отображение.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/EditorAdapterTypes.h`, `Source/GV2ContentEditor/Private/Testing/EditorAdapterConformance.cpp`.

## Проверка milestone

- [x] Редакторский модуль отсутствует в shipping-сборке.
- [x] Обращение к контенту идёт только через адаптер.
- [x] Второй изменяемой модели контента нет.
- [x] Отказ по устаревшему файлу отличим от ошибки валидации.
- [x] Диагностика структурна на всём пути.
