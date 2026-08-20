---
title: Four Kinds Tasks
status: active
version: 1.0
updated: 2026-08-20
depends_on:
  - WriteSurface.md
  - ../../Architecture/StableIDSpecification.md
decisions:
  - ../../ADR/0026-core-and-gameplay-ownership.md
---

# M5 — Four Kinds

> **Материализует:** раздел 38 [предложения](../../Proposals/ContentEditorPluginProposal.md).
> **Задачи:** CED-17…20.
> **Результат:** редактируются `item`, `location`, `actor` и `world`.

## Результат этапа

Цикл, подтверждённый на простейшем kind-е, расширяется на остальные. Порядок задач идёт от простого к сложному не ради удобства: каждый следующий kind добавляет ровно один новый механизм, поэтому отказ однозначно относится к нему.

| Kind | Что добавляет |
|---|---|
| `item` | Плоская схема одного пакета; единственный kind с готовым файлом метаданных представления |
| `location` | Массив ссылок на определения и ссылка на экран |
| `actor` | Базовая схема `core` плюс два extension site из `textsystem` и `rh` |
| `world` | Kind-а не существует; требует предварительного контентного решения |

## Задачи

- [x] **CED-17 — `item`**
  - Зависимости: CED-16.
  - Done: определения `item` открываются, редактируются и сохраняются полностью; `item_v1.ui.json5` управляет подписями, категориями и порядком полей; числовые, строковые и ссылочные поля работают; правка совпадает с CLI побайтово.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/Testing/FourKindsConformance.h`, `Source/GV2ContentEditor/Private/Testing/FourKindsConformance.cpp`, `GameData/rh/definitions/items.json5`.

- [x] **CED-18 — `location`**
  - Зависимости: CED-17.
  - Done: массив ссылок на определения редактируется как коллекция типизированных выборов, а не как список строк; добавление, удаление и переупорядочивание элементов проходят одной атомарной операцией; ссылка на экран разрешается тем же механизмом; входящие ссылки локации отображаются корректно.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/Testing/FourKindsConformance.h`, `Source/GV2ContentEditor/Private/Testing/FourKindsConformance.cpp`, `GameData/rh/definitions/locations.json5`.

- [x] **CED-19 — `actor` и extension sites**
  - Зависимости: CED-18.
  - Схема актора собрана из трёх файлов трёх пакетов: базовая в `core`, расширения в `textsystem` и `rh`.
  - Done: форма показывает базовые поля и поля обоих расширений с явным указанием, какому пакету принадлежит каждая группа; правка поля расширения записывается в свой пакет, а не в базовое определение; отсутствие расширения не ломает форму; попытка записать поле в чужой пакет отклоняется.
  - Evidence: `Source/GV2ContentEditor/Public/GV2ContentEditor/Testing/FourKindsConformance.h`, `Source/GV2ContentEditor/Private/Testing/FourKindsConformance.cpp`, `GameData/*/schemas/actor*.json5`.

- [x] **CED-20 — `world`**
  - Зависимости: CED-19.
  - **Предварительное решение.** Сегодня мир существует только как рантайм-состояние (`game.instances.world()`, `state.world`) и определением не является — редактировать нечего. Задача начинается с контентного решения: становится ли `world` kind-ом, какому слою он принадлежит и какие поля содержит. Решение принимается отдельно и фиксируется ADR либо контрактом, а не выводится из потребностей редактора.
  - Done: зафиксировано решение: согласно `StableIDSpecification.md` и `SessionBootstrapContract.md`, `world` является динамическим рантайм-состоянием игровой сессии, а не статическим определением в репозитории (`GameData`). В реестре Stable ID kind `world` отсутствует; срез определений закрыт тремя каноническими kind-ами (`item`, `location`, `actor`).
  - Evidence: `Docs/Architecture/StableIDSpecification.md`, `GameData/`, `Source/GV2ContentEditor/Private/Testing/FourKindsConformance.cpp`.

## Проверка milestone

- [x] `item`, `location` и `actor` редактируются полностью.
- [x] Поле расширения записывается в свой пакет.
- [x] Массив ссылок редактируется типизированно и сохраняется одной операцией.
- [x] Судьба `world` решена явно и записана.
