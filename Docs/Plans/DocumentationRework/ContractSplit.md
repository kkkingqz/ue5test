---
title: Contract Split Tasks
status: normative
version: 1.0
updated: 2026-08-19
depends_on:
  - README.md
  - ../../Architecture/LuaRuntimeContract.md
  - ../../Architecture/DependencyMap.md
decisions:
  - ../../ADR/0027-designer-lua-authoring-layer.md
  - ../../ADR/0031-entity-authoring-extensions.md
---

# M3 — Contract Split

> **Материализует:** решение о разделении `LuaRuntimeContract.md` по владению.
> **Задачи:** DOC-07…09.
> **Результат:** три документа с разными владельцами и разными читателями вместо одного склада на семнадцать разделов.

## Результат этапа

`LuaRuntimeContract.md` — 4872 слова и 17 разделов: от стандартных библиотек Lua до авторского слоя дизайнера. Разделение проводится по владению, а не по объёму: «как загружается и защищается Lua» и «что существует в рантайме и когда замерзает» — разные ответственности с разными потребителями.

На документ ссылаются 76 файлов, поэтому переразбиение выполняется один раз и целиком.

## Задачи

- [ ] **DOC-07 — Выделить контракт авторского слоя**
  - Зависимости: закрытие активных планов (см. [предусловие](README.md#предусловие-выполнения)).
  - Поверхность дизайнера — `commands`, `actions`, `Actor.*`, `field.*`, `validate`, `services`, `fail`, `emit`, `on`, `text`, `button`, `show_screen` — не имеет владельца и размазана по трём документам и пяти ADR.
  - Done: создан `Docs/Architecture/AuthoringSurfaceContract.md`, владеющий составом авторского окружения, правилами объявления и отложенной регистрации, заморозкой, атрибуцией `fail()` и `emit()` пакету объявления, execution scope и его наследованием; соответствующие разделы удалены из `LuaRuntimeContract.md` и `ScreenTemplates.md`, а не продублированы; ссылки в затронутых документах перенаправлены в том же change set; `DependencyMap.md` и `Docs/README.md` знают о новом документе.
  - Evidence: `Docs/Architecture/AuthoringSurfaceContract.md`, `Docs/Architecture/LuaRuntimeContract.md`, `Docs/README.md`.

- [ ] **DOC-08 — Выделить контракт фасада и реестров**
  - Зависимости: DOC-07.
  - Done: создан `Docs/Architecture/RuntimeFacadeAndRegistries.md`, владеющий составом `game`, реестрами (экземпляры, сервисы, семантические действия, расширения сущностей, обработчики, валидаторы, подписчики), общей формой `register → freeze → is_frozen` и порядком заморозки на фазе `register`; в `LuaRuntimeContract.md` остаются VM, стандартные библиотеки, загрузчик модулей, окружение, защищённое исполнение, детерминизм, GC и диагностика; раздел `Canonical state`, бывший указателем на соседний контракт, заменён ссылкой.
  - Evidence: `Docs/Architecture/RuntimeFacadeAndRegistries.md`, `Docs/Architecture/LuaRuntimeContract.md`.

- [ ] **DOC-09 — Целостность после разделения**
  - Зависимости: DOC-08.
  - Done: ни одна ответственность не потеряна и не удвоена — сверка разделов «до» и «после» приложена к change set; все 76 ссылавшихся файлов указывают на документ, который действительно владеет упомянутым правилом, а не на прежний путь по инерции; `DependencyMap.md`, `Invariants.md` и `Docs/README.md` синхронизированы; валидатор зелёный; циклов зависимостей не появилось.
  - Evidence: `Docs/Architecture/DependencyMap.md`, `Docs/Architecture/Invariants.md`, отчёт валидатора.

## Проверка milestone

- [ ] `LuaRuntimeContract.md` не содержит разделов про фасад, реестры и авторский слой.
- [ ] У каждого правила ровно один владелец.
- [ ] Ссылки ведут к владельцу правила, а не к прежнему пути.
- [ ] Карта зависимостей и реестр инвариантов обновлены.
