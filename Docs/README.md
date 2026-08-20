---
title: GV2 Documentation Index
status: informative
version: 4.0
updated: 2026-08-20
language: ru
---

# Документация GV2

Этот файл только направляет читателя. Нормативные правила находятся в `Architecture/`, `UI/` и accepted ADR.

| Нужно | Читать |
|---|---|
| Понять цель проекта | [Project Brief](ProjectBrief.md), [Concepts](Concepts/README.md) |
| Найти обязательное правило | [Architecture](Architecture/README.md), [UI](UI/README.md), [Invariants](Architecture/Invariants.md) |
| Понять причину решения | [ADR](ADR/README.md) |
| Выполнить типовую задачу | [Guides](Guides/README.md) и owner contract |
| Выполнить запланированную работу | [Plans](Plans/README.md) |
| Изучить открытые идеи | [Proposals](Proposals/README.md) |
| Сверить contract и реализацию | [Implementation Status](Status/ImplementationStatus.md) |

## Authority и lifecycle

Accepted ADR фиксирует решение и причины, subsystem contract — актуальное полное правило. Более конкретный contract сильнее `Architecture/Overview.md`. При конфликте ненормативный документ уступает contract.

| Документы | `status` | Роль |
|---|---|---|
| `Architecture/`, `UI/` | `draft`, `normative`, `deprecated` | Contracts |
| `ADR/NNNN-*.md` | decision status | Решения |
| Активные Plans | `active` | Выполняемая работа, не architecture authority |
| Активные Proposals | `draft` + `proposal_state` | Открытый вопрос |
| Concepts, Guides, Status, Project Brief и routers | `informative` | Объяснение, инструкция, навигация |
| `Archive/`, `Rejected/` | `archived` | История, не источник правил или задач |

Экспортированные и внешние копии не являются источником истины. Соответствие статуса и расположения проверяет `Tools/Documentation/validate_docs.py`.

## Ключевые архитектурные гейты

- [INV-013](Architecture/Invariants.md): код принадлежит C++ только если требует недоступной Lua возможности либо обязан работать до создания VM; полный критерий — [Overview § Границы C++](Architecture/Overview.md#границы-c).
- [INV-017](Architecture/Invariants.md): до project version `1.0.0` обратная совместимость между релизами не гарантируется, но breaking change обязан быть явным; полный контракт — [Compatibility Policy](Architecture/CompatibilityPolicy.md).

## Минимальный маршрут

Для проектирования: релевантный Concept → owner contract → связанные accepted ADR. Для типового изменения: Guide → owner contract → активный Plan, если он есть. При пересечении ownership, Stable ID, command/event, save, repository, Lua/UE boundary, lifecycle, UI или modding проверить соседние contracts. Не загружать `Architecture/` целиком.

Карта допустимых зависимостей — [Dependency Map](Architecture/DependencyMap.md), сборка и CI — [Build and Tooling](Architecture/BuildAndTooling.md).

## Ведение

- Архитектурное решение сначала фиксируется ADR, затем синхронно отражается в owner contracts.
- Concepts и Guides ссылаются на правило, не копируют его.
- Публичные примеры являются fixtures и обязаны соответствовать grammar и schemas.
- Новая абстракция требует конкретного сценария или измеренной проблемы.
