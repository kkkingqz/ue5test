---
title: DocumentationRework Archive Summary
status: archived
version: 1.0
updated: 2026-08-20
---

# DocumentationRework: итог выполнения

> **Материализует:** исторический итог выполненного плана; документ не является источником правил или задач.

## Цель и результат

**Цель:** сократить документацию без потери правил, решений, диагностики и проверяемых примеров, сохранив повышенный приоритет понятности в `Docs/Authoring/`.

**Результат:** уточнена authority тиров, зафиксированы правила совместимости и границы C++, удалены legacy-инструкции, разделён Lua-контракт, создана документация авторского слоя, разведены аудитории `Authoring/` и `Guides/`, статус реализации инвертирован, а архив планов свёрнут в проверяемые summaries.

## Этапы и задачи

### M1 — Rules and Visibility

Зафиксирован воспроизводимый baseline, статусы документов приведены к их authority, политика совместимости и критерий принадлежности C++ сделаны заметными, а процесс ADR и синхронизации документации закреплён правилами репозитория.

- `DOC-00` — Зафиксировать baseline и выровнять authority статусов
- `DOC-01` — Политика совместимости
- `DOC-02` — Заметность правила о минимуме C++
- `DOC-03` — Правило выделения номера ADR
- `DOC-04` — Правила переработки документации при изменении кода

### M2 — Legacy Cleanup

Валидатор получил контекстный гейт на удалённые идентификаторы, а актуальные contracts, concepts и инструкции очищены от устаревших API и форматов без переписывания исторических records.

- `DOC-05` — Гейт на удалённые идентификаторы
- `DOC-06` — Чистка legacy-текста

### M3 — Contract Split

Ответственности монолитного `LuaRuntimeContract` разделены между VM/runtime, фасадом и реестрами, а также новым контрактом авторского слоя; ссылки и dependency graph приведены к единственным владельцам правил.

- `DOC-07` — Выделить контракт авторского слоя
- `DOC-08` — Выделить контракт фасада и реестров
- `DOC-09` — Целостность после разделения

### M4 — Authoring Docs

Создан отдельный informative-тир `Docs/Authoring/` с маршрутизатором и понятным человеку справочником фактической authoring surface, подкреплённым актуальными примерами.

- `DOC-10` — Каталог `Docs/Authoring/` и его тир
- `DOC-11` — Справочник инструментов авторского слоя

### M5 — Guides and Archive

Задачи дизайнера перенесены в `Authoring/`, programmer guides сведены к реальным точкам расширения, `ImplementationStatus` оставляет только подтверждённые разрывы contract ↔ code, а 117 архивных task-файлов заменены 24 плоскими summaries с проверяемой полной историей.

- `DOC-12` — Разделить `Guides/` по аудитории
- `DOC-13` — Недостающие гайды программиста
- `DOC-14` — Инвертировать `ImplementationStatus`
- `DOC-15` — Перевести архив планов в плоские summary-файлы
- `DOC-16` — Сквозная верификация

## Актуальные нормативные источники

- [CompatibilityPolicy](../../Architecture/CompatibilityPolicy.md)
- [LuaRuntimeContract](../../Architecture/LuaRuntimeContract.md)
- [RuntimeFacadeAndRegistries](../../Architecture/RuntimeFacadeAndRegistries.md)
- [AuthoringSurfaceContract](../../Architecture/AuthoringSurfaceContract.md)

## Полная история

`source_commit`: [49fc22aaa0cda2fa205012e875efe6e0df082fcf](https://github.com/kkkingqz/ue5test/commit/49fc22aaa0cda2fa205012e875efe6e0df082fcf)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/49fc22aaa0cda2fa205012e875efe6e0df082fcf/Docs/Plans/DocumentationRework) содержит исходные task-файлы, acceptance criteria и evidence.
