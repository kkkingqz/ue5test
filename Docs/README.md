---
title: GV2 Documentation Index
status: normative
version: 3.1
updated: 2026-08-16
language: ru
---

# Документация GV2

Документация разделена по типу задачи читателя. Markdown предназначен одновременно для разработчиков, инструментов и AI-агентов.

| Раздел | Отвечает на вопрос | Нормативность |
|---|---|---|
| [ProjectBrief](ProjectBrief.md) | Что это за проект | нет |
| [Concepts](Concepts/README.md) | Что это и зачем | нет |
| [Architecture](Architecture/README.md), [UI](UI/README.md) | Какие правила обязательны | **да** |
| [Guides](Guides/README.md) | Как выполнить типовую задачу | нет |
| [Authoring](Authoring/README.md) | Как наполнить игру контентом (для не-программиста) | нет |
| [ADR](ADR/README.md) | Почему принято такое решение | **да** |
| [Plans](Plans/README.md) | Как конкретное изменение будет реализовано | нет |
| [Proposals](Proposals/README.md) | Что рассматривается, но не принято | нет |
| [Status](Status/ImplementationStatus.md) | Что из спецификации реализовано | нет |

## Иерархия источников

1. `ADR/*.md` со `status: accepted` фиксируют принятые архитектурные решения.
2. Contract подсистемы уточняет `Architecture/Overview.md`.
3. `Architecture/Overview.md` задаёт общие границы и инварианты.

При конфликте применяется более конкретный документ. Заменённый ADR обязан иметь `status: superseded` и ссылку на замену.

**Ненормативные тиры не спорят с нормативными.** Concepts, Guides и Authoring объясняют и инструктируют, но правил не вводят: при расхождении прав contract. Это закреплено статусом `informative`, который валидатор требует внутри `Concepts/`, `Guides/` и `Authoring/` и запрещает снаружи. Аналогично `archived` допустим только внутри `Archive/`.

Экспортированные и внешние копии нормативными не являются и рядом с canonical Markdown не хранятся.

## Маршрут загрузки контекста

Не загружайте `Docs/Architecture` целиком. Минимальный путь зависит от задачи.

**Разобраться в архитектуре или спроектировать изменение:**

1. [ProjectBrief](ProjectBrief.md)
2. Нужный документ из [Concepts](Concepts/README.md)
3. Contract затронутой подсистемы
4. Связанные accepted ADR
5. [Implementation Status](Status/ImplementationStatus.md) и ссылки на код

**Выполнить типовую задачу:** нужный [Guide](Guides/README.md) + contract, на который он ссылается + активный [план](Plans/README.md), если задача из него.

**Сориентироваться в зависимостях:** [Dependency Map](Architecture/DependencyMap.md). **Найти нормативный источник правила:** [Invariants](Architecture/Invariants.md). **Найти код и тесты по понятию:** таблица в [Concepts](Concepts/README.md).

## Правила ведения

- Сначала ADR, если меняется источник истины, идентичность, command/event semantics, save compatibility, trust model или направление зависимостей.
- Затем — все затронутые contracts в том же изменении.
- Concepts и Guides объясняют и ссылаются, но не переписывают нормативные формулировки: два описания одного правила расходятся.
- Публичный пример считается тестовым fixture и обязан соответствовать грамматике и терминологии.
- Новая абстракция вводится под конкретный сценарий или измеренную проблему.
- Изменение объёма реализованного отражается в [Implementation Status](Status/ImplementationStatus.md).

## Сборка и CI

Таргеты, хосты, коды возврата, фикстуры и обязательный integration gate — [Build and Tooling](Architecture/BuildAndTooling.md).
