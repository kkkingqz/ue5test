---
title: GV2 Implementation Plans Index
status: normative
version: 4.2
updated: 2026-08-19
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

- [DocumentationRework](DocumentationRework/README.md) — Политика совместимости и заметность правил, гейт на удалённый API в инструкциях, разделение `LuaRuntimeContract`, документация авторского слоя для дизайнера, переработка `Guides/` и сжатие архива. Выполняется после закрытия остальных активных планов.
- [GameplayServices](GameplayServices/README.md) — Авторский синтаксис `services.<name> = { … }` для процессов, координирующих несколько сущностей, вместе с первым потребителем: торговцем в `rh`, который получает золото и теряет товар.
- [UiComposition](UiComposition/README.md) — Слои, оверлеи, модалки и вкладки вместо одного активного экрана, переиспользование виджетов при реконсиляции, масштабирование раскладки по целевому разрешению.

Фактическое состояние реализации по подсистемам: [Implementation Status](../Status/ImplementationStatus.md).

## Архив

| План | Завершён | Результат |
|---|---|---|
| [CommandValidators](Archive/CommandValidators/README.md) | 2026-08-19 | Авторский `validate()` для независимых policy поверх чужих команд, единое декодирование аргументов, охранники побочных эффектов и явная заменяемость обработчика |
| [EntityExtensionsHardening](Archive/EntityExtensionsHardening/README.md) | 2026-08-19 | Заморозка реестра расширений стала необходимой, скомпонованная таблица методов — единственным путём поиска, повторное объявление внутри модуля — ошибкой, приём метода в `textsystem` — строго `self` |
| [EntityAuthoringExtensions](Archive/EntityAuthoringExtensions/README.md) | 2026-08-19 | Декларативное расширение сущностей (`Actor`, `Location`, `Quest`, `Item`) через прототипы в `_ENV`, централизованный реестр расширений и скомпонованные effective method tables |
| [TextSystemLayer](Archive/TextSystemLayer/README.md) | 2026-08-19 | Трёхуровневая архитектура (`core` ← `textsystem` ← `rh`), набор пакетов из данных, перенос владения локациями и переходами, декларативные экраны и трёхуровневые спеки |
| [RHActorsSimplification](Archive/RHActorsSimplification/README.md) | 2026-08-19 | Декларативные контракты полей `field.*` с композицией схем, запрет повторного объявления, обобщённое создание экземпляров с реестром видов, чистый `rh/actors.lua` |
| [PortableContentCore](Archive/PortableContentCore/README.md) | 2026-08-14 | Общий pipeline `Packages → Definitions → Immutable Repository Snapshot` для CLI, Headless и UE плюс `game.repository` в Lua |
| [HeadlessParityAndReplay](Archive/HeadlessParityAndReplay/README.md) | 2026-08-14 | Одна реализация на portable-проверку и воспроизводимый прогон с общим digest для UE и headless |
| [CanonicalGameplayState](Archive/CanonicalGameplayState/README.md) | 2026-08-15 | `game.state`, instance identity, хэш состояния в digest, ActorRegistry и mutation window |
| [TestArchitectureAndLuaSpecs](Archive/TestArchitectureAndLuaSpecs/README.md) | 2026-08-15 | Lua spec runner, независимость тестов от контента игры, миграция 2375 строк C++ в спеки |
| [ContentAuthoringTools](Archive/ContentAuthoringTools/README.md) | 2026-08-15 | Справочник и заготовки из схем, быстрая проверка Lua-модулей, обратные ссылки и переименование ID, живой цикл валидации и индекс для автодополнения |
| [SaveAndLoad](Archive/SaveAndLoad/README.md) | 2026-08-16 | Обратимый канонический кодек, slot-storage примитив, конверт контейнера, загрузка на холодном старте с резолвом редиректов и версии секций с миграциями |
| [PackageSupport](Archive/PackageSupport/README.md) | 2026-08-16 | Обязательный манифест пакета, набор корней с явным порядком и lock-файлом, Lua внутри пакета, замещение модулей ядра с доступом к базе, состав пакетов в digest и сейве |
| [RhGamePackage](Archive/RhGamePackage/README.md) | 2026-08-16 | Игровой пакет `rh`, перенос конкретных сущностей со сменой namespace, развязка `core` от игровых идентификаторов и гейт на обратную ссылку |
| [CommandHandlerRegistry](Archive/CommandHandlerRegistry/README.md) | 2026-08-16 | Реестр `game.commands.handlers` по ключу `command_id`, типизированный отказ на неизвестную команду, регистрация команд пакетом без правки ядра |
| [TestGameplaySlice](Archive/TestGameplaySlice/README.md) | 2026-08-17 | Первый играбельный цикл целиком в пакете `rh`: три локации и экрана, карта с расходом выносливости, покупки и заработок |
| [CoreBoundaryMigration](Archive/CoreBoundaryMigration/README.md) | 2026-08-17 | Демо в пакете `sample`, точка расширения обёртки актора, схемы `item`/`location` в `rh`, `actor_v1` до `discriminator`, гейт границы |
| [DesignerAuthoringLayer](Archive/DesignerAuthoringLayer/README.md) | 2026-08-18 | Designer-facing Lua: `write_revision` и правило `fail()`, дескриптор команд, модель property, универсальная секция runtime-состояния, события и экраны; геймплей `rh` переписан |
| [SimplifiedAuthoringSurface](Archive/SimplifiedAuthoringSurface/README.md) | 2026-08-18 | Окружение authoring-скрипта без `M.`, автообнаружение модулей и генерируемый манифест, предусловия и единый API актора, источник презентации; геймплей `rh` сведён к одному файлу правил |
| [ContentEditorPrerequisites](Archive/ContentEditorPrerequisites/README.md) | 2026-08-18 | Классификация схемных изменений с сосуществованием версий, точечная правка поля и удаление записи в JSON5, метаданные представления в отдельном файле; редактор и декларативные экраны разблокированы |
| [TextSystemLayer](Archive/TextSystemLayer/README.md) | 2026-08-19 | Набор пакетов из данных, слой `textsystem` с локациями и переходами, семантические действия и декларативные экраны, трёхуровневое разбиение спеков |
