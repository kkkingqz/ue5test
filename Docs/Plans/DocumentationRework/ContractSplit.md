---
title: Contract Split Tasks
status: active
version: 1.2
updated: 2026-08-20
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

На документ ссылаются десятки файлов, поэтому переразбиение выполняется один раз и целиком; точный набор входящих ссылок фиксирует DOC-00. Разделение начинается не с переноса текста, а с ownership map:

| Документ после разделения | Владеет | Не владеет |
|---|---|---|
| `LuaRuntimeContract.md` | VM, загрузка модулей, sandbox, protected execution, determinism, GC, runtime diagnostics | составом authoring `_ENV`, семантикой конкретных registries и подсистем |
| `AuthoringSurfaceContract.md` | authoring syntax, declarations, adapters к runtime, execution scope, package attribution и freeze authoring descriptors | Command/Event semantics, canonical state, Screen Document и Presentation semantics |
| `RuntimeFacadeAndRegistries.md` | картой фасада `game`, общим registry protocol и host-side freeze sequence | значением entries, локальным ordering/refusal/override contract конкретного subsystem registry |
| Subsystem contracts | семантикой Commands, Events, instances, services, state и UI | устройством VM и общим authoring syntax |

Эта таблица является правилом миграции M3: новый документ не может присвоить ответственность, уже принадлежащую более конкретному subsystem contract.

## Задачи

- [ ] **DOC-07 — Выделить контракт авторского слоя**
  - Зависимости: ожидание UiFoundation снято решением из [execution baseline](README.md#execution-baseline).
  - Поверхность дизайнера — `commands`, `actions`, `Actor.*`, `field.*`, `validate`, `services`, `fail`, `emit`, `on`, `text`, `button`, `show_screen` — не имеет владельца и размазана по трём документам и пяти ADR.
  - Done: создан `Docs/Architecture/AuthoringSurfaceContract.md`, владеющий составом авторского окружения, правилами объявления и отложенной регистрации, адаптацией в runtime registries, заморозкой, атрибуцией `fail()` и `emit()` пакету объявления, execution scope и его наследованием; authoring-specific разделы удалены из `LuaRuntimeContract.md`, а из `ScreenTemplates.md` переносится только syntax/adaptation — Screen Document и Presentation semantics остаются у UI contract; ссылки в затронутых документах перенаправлены в том же change set; `DependencyMap.md` и `Docs/README.md` знают о новом документе.
  - Evidence: `Docs/Architecture/AuthoringSurfaceContract.md`, `Docs/Architecture/LuaRuntimeContract.md`, `Docs/README.md`.

- [ ] **DOC-08 — Выделить контракт фасада и реестров**
  - Зависимости: DOC-07.
  - Done: создан `Docs/Architecture/RuntimeFacadeAndRegistries.md`, владеющий только составом `game`, общей формой `register → freeze → is_frozen`, правилами доступности реестров и host-side порядком заморозки на фазе `register`; для экземпляров, сервисов, семантических действий, расширений сущностей, обработчиков, валидаторов и подписчиков документ содержит маршрут к owner contract, но не повторяет их entry semantics, local ordering, refusal или override rules; в `LuaRuntimeContract.md` остаются VM, стандартные библиотеки, загрузчик модулей, окружение, защищённое исполнение, детерминизм, GC и диагностика; раздел `Canonical state`, бывший указателем на соседний контракт, заменён ссылкой.
  - Evidence: `Docs/Architecture/RuntimeFacadeAndRegistries.md`, `Docs/Architecture/LuaRuntimeContract.md`.

- [ ] **DOC-09 — Целостность после разделения**
  - Зависимости: DOC-08.
  - Done: ни одна ответственность не потеряна и не удвоена — ownership map дополнена соответствием каждого исходного раздела его итоговому владельцу; все файлы из baseline DOC-00, ссылавшиеся на `LuaRuntimeContract.md`, указывают на документ, который действительно владеет упомянутым правилом, а не на прежний путь по инерции; file links и anchors проходят существующую проверку validator-а; `DependencyMap.md`, `Invariants.md` и `Docs/README.md` синхронизированы; циклов зависимостей не появилось.
  - Evidence: `Docs/Architecture/DependencyMap.md`, `Docs/Architecture/Invariants.md`, отчёт валидатора.

## Проверка milestone

- [ ] `LuaRuntimeContract.md` не содержит разделов про фасад, реестры и авторский слой.
- [ ] У каждого правила ровно один владелец.
- [ ] Общий facade contract маршрутизирует к subsystem contracts и не повторяет их семантику.
- [ ] UI contract сохранил ownership Screen Document и Presentation semantics.
- [ ] Ссылки ведут к владельцу правила, а не к прежнему пути.
- [ ] Карта зависимостей и реестр инвариантов обновлены.
