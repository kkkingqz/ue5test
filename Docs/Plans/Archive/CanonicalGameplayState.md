---
title: CanonicalGameplayState Archive Summary
status: archived
version: 1.0
updated: 2026-08-15
---

# CanonicalGameplayState: итог выполнения

> **Состояние:** план выполнен; документ является историческим summary, а не источником правил или задач.

## Цель и результат

**Цель:** Canonical gameplay-state существует, создаётся при bootstrap session, проверяется на инварианты, наблюдается одним скаляром в run digest и меняется единственным разрешённым путём

**Результат:** Canonical gameplay-state существует, создаётся при bootstrap session, проверяется на инварианты, наблюдается одним скаляром в run digest и меняется единственным разрешённым путём

## Этапы и задачи

### M1 — State Root and Module Lifecycle

Module loader вызывает lifecycle hooks в порядке, заданном контрактом. Каждый module объявляет свой вклад в canonical state, координатор собирает `game.state` до перехода session в `Ready`, а невалидный state блокирует создание session вместо того, чтобы проявиться позже

- `CGS-01` — Ввести module lifecycle hooks
- `CGS-02` — Создать canonical state root
- `CGS-03` — Проверять допустимые значения state
- `CGS-04` — Зафиксировать изоляцию секций
- `CGS-05` — Синхронизировать документацию этапа

### M2 — Instance Identity

`meta` содержит persistent счётчики, аллокатор выдаёт `instance_id` по grammar Stable ID, а принадлежность инстансов проверяется инвариантами. После этого форма state стабилизируется, и её можно фиксировать хэшем

- `CGS-06` — Реализовать аллокатор `instance_id`
- `CGS-07` — Зафиксировать структуру `meta`
- `CGS-08` — Проверять инварианты принадлежности
- `CGS-09` — Синхронизировать документацию этапа

### M3 — State Observability

Lua вычисляет канонический хэш своего state и публикует его host-у одним скаляром. Хэш входит в run digest, поэтому golden-прогон начинает ломаться при изменении наблюдаемого состояния. Само дерево boundary не пересекает

- `CGS-10` — Реализовать канонический хэш state в Lua
- `CGS-11` — Опубликовать хэш host-у
- `CGS-12` — Включить хэш в run digest
- `CGS-13` — Обновить golden-прогоны
- `CGS-14` — Синхронизировать документацию этапа

### M4 — Runtime Validator Consolidation

Правила валидации canonical state существуют в одном экземпляре, в `Scripts/`, и не дублируются внутри C++. Список канонических секций объявлен один раз. Возврат дублирующей реализации обнаруживается автоматически

- `CGS-15` — Свести валидацию state к одной реализации
- `CGS-16` — Объявить список канонических секций один раз
- `CGS-17` — Расширить parity gate на встроенный Lua
- `CGS-18` — Синхронизировать документацию этапа

### M5 — Actor Object Model and Mutation Slice

Gameplay-код работает с акторами через disposable runtime-объекты поверх canonical state, а не напрямую с таблицами. Registry отвечает за identity и получение объекта, Actor — за локальные доменные операции, Gameplay Services — за широкие workflow. Изменение state возможно только внутри mutation window, и это проверяется автоматически

- `CGS-19` — Ввести mutation window
- `CGS-20` — Перевести игрока в общую модель акторов
- `CGS-21` — Реализовать ActorRegistry
- `CGS-22` — Реализовать Actor wrapper и дискриминатор
- `CGS-23` — Определить удаление и висячие ссылки
- `CGS-24` — Ввести реестр Gameplay Services
- `CGS-25` — Реализовать первый command handler
- `CGS-26` — Закрыть модель conformance-тестами
- `CGS-27` — Синхронизировать документацию этапа

## Актуальные нормативные источники

- [CanonicalStateAndSave](../../Architecture/CanonicalStateAndSave.md)
- [CommandsAndEvents](../../Architecture/CommandsAndEvents.md)
- [RuntimeFacadeAndRegistries](../../Architecture/RuntimeFacadeAndRegistries.md)

## Полная история

`source_commit`: [2ad10751577e04bd21ceb0d500e6bc2e5515dd29](https://github.com/kkkingqz/ue5test/commit/2ad10751577e04bd21ceb0d500e6bc2e5515dd29)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/2ad10751577e04bd21ceb0d500e6bc2e5515dd29/Docs/Plans/Archive/CanonicalGameplayState) содержит исходные task-файлы, acceptance criteria и evidence.
