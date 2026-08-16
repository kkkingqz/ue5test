---
title: GV2 Implementation Plans Index
status: normative
version: 2.7
updated: 2026-08-16
depends_on:
  - ../README.md
---

# Индекс планов реализации

`Plans` содержит исполняемые декомпозиции уже принятых направлений. План не меняет архитектурный контракт и не заменяет Proposal или ADR: он связывает ограниченные задачи, зависимости и evidence завершения.

## Правила ведения

- Checkbox задачи является единственным источником её статуса завершения.
- `[ ]` означает, что Definition of Done ещё не подтверждён; `[x]` — подтверждён полностью.
- Для текущей работы после названия можно временно добавить `— in progress`; для блокировки — `— blocked: <причина>`.
- Нельзя отмечать задачу выполненной только по наличию кода: должны пройти перечисленные tests и быть добавлены ссылки в поле `Evidence` либо в итоговый отчёт change set.
- При завершении всех задач этапа синхронно отмечается milestone в его локальном `README.md`.
- Изменение архитектурного инварианта по ходу задачи требует ADR и обновления contracts до отметки `[x]`.
- Полностью выполненный план переносится в [Archive](Archive/README.md) и перестаёт быть источником задач; его нормативный результат к этому моменту обязан быть перенесён в contracts.
- При переносе все файлы плана получают `status: archived`. Валидатор требует этот статус для всего внутри `Archive/` и запрещает его снаружи.

## Активные планы

Активных планов нет. Следующее направление выбирается из [Proposals](../Proposals/README.md).

После `TestArchitectureAndLuaSpecs` добавление контента в `GameData/core` не меняет ни одного pinned-значения. Pinned-значения принадлежат замороженному корпусу и golden-прогонам; задачи, меняющие их (`LOC-01`), остаются отдельными change set-ами.

Фактическое состояние реализации по подсистемам: [Implementation Status](../Status/ImplementationStatus.md).

## Архив

| План | Завершён | Результат |
|---|---|---|
| [PortableContentCore](Archive/PortableContentCore/README.md) | 2026-08-14 | Общий pipeline `Packages → Definitions → Immutable Repository Snapshot` для CLI, Headless и UE плюс `game.repository` в Lua |
| [HeadlessParityAndReplay](Archive/HeadlessParityAndReplay/README.md) | 2026-08-14 | Одна реализация на portable-проверку и воспроизводимый прогон с общим digest для UE и headless |
| [CanonicalGameplayState](Archive/CanonicalGameplayState/README.md) | 2026-08-15 | `game.state`, instance identity, хэш состояния в digest, ActorRegistry и mutation window |
| [TestArchitectureAndLuaSpecs](Archive/TestArchitectureAndLuaSpecs/README.md) | 2026-08-15 | Lua spec runner, независимость тестов от контента игры, миграция 2375 строк C++ в спеки |
| [ContentAuthoringTools](Archive/ContentAuthoringTools/README.md) | 2026-08-15 | Справочник и заготовки из схем, быстрая проверка Lua-модулей, обратные ссылки и переименование ID, живой цикл валидации и индекс для автодополнения |
| [SaveAndLoad](Archive/SaveAndLoad/README.md) | 2026-08-16 | Обратимый канонический кодек, slot-storage примитив, конверт контейнера, загрузка на холодном старте с резолвом редиректов и версии секций с миграциями |
| [PackageSupport](Archive/PackageSupport/README.md) | 2026-08-16 | Обязательный манифест пакета, набор корней с явным порядком и lock-файлом, Lua внутри пакета, замещение модулей ядра с доступом к базе, состав пакетов в digest и сейве |
| [RhGamePackage](Archive/RhGamePackage/README.md) | 2026-08-16 | Игровой пакет `rh`, перенос конкретных сущностей со сменой namespace, развязка `core` от игровых идентификаторов и гейт на обратную ссылку |
