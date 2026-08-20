---
title: GameplayServices Archive Summary
status: archived
version: 1.0
updated: 2026-08-19
---

# GameplayServices: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Дать авторскому слою stateless-процессы для операций, координирующих несколько сущностей, — и одновременно сделать в `rh` первую такую операцию

**Результат:** авторский синтаксис `services.<name> = { … }` и первый настоящий потребитель — торговец в `rh`, у которого покупка отнимает товар и приносит золото

## Этапы и задачи

### M1 — Service Authoring

пакет объявляет сервис одной таблицей и не видит устройства реестра

- `GSA-01` — Создать ADR по авторским сервисам
- `GSA-02` — Kind `service` и проверка формы ID
- `GSA-03` — Прокси `services` и контракт таблицы
- `GSA-04` — Неизменяемость, дубликаты и разрешение ссылок
- `GSA-05` — Наследование execution scope

### M2 — Merchant Trade

покупка перестаёт создавать предмет из воздуха: у товара есть владелец, который получает золото и теряет товар

- `GSA-06` — Начальное состояние: игрок и торговец
- `GSA-07` — Инвентарь как поведение одной сущности
- `GSA-08` — Сервис торговли и перевод покупки
- `GSA-09` — Спеки и сквозная верификация

## Актуальные нормативные источники

- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)
- [CommandsAndEvents](../../Architecture/CommandsAndEvents.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/GameplayServices) содержит исходные task-файлы, acceptance criteria и evidence.
