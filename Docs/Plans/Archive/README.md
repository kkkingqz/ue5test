---
title: Archived Implementation Plans
status: archived
version: 2.1
updated: 2026-08-20
depends_on:
  - ../README.md
---

# Архив выполненных планов

Архив содержит по одному плоскому summary на выполненный план. Summary не является источником правил или задач: актуальное поведение задают contracts, а полные исторические task-файлы доступны через `source_commit`.

| План | Завершён | Результат |
|---|---|---|
| [CanonicalGameplayState](CanonicalGameplayState.md) | 2026-08-15 | Canonical state, module lifecycle hooks, instance identity, state hash в run digest, ActorRegistry и mutation window |
| [CommandHandlerRegistry](CommandHandlerRegistry.md) | 2026-08-16 | Реестр обработчиков по `command_id` вместо цепочки, отказ на неизвестную команду, развязка `ingress` от игровых модулей, команды из пакета без C++ |
| [CommandValidators](CommandValidators.md) | 2026-08-19 | Авторский `validate()` для независимых policy поверх чужих команд, единое декодирование аргументов, охранники побочных эффектов и явная заменяемость обработчика |
| [ContentAuthoringTools](ContentAuthoringTools.md) | 2026-08-15 | Справочник и заготовки из схем, быстрая проверка Lua-модулей, обратные ссылки и переименование ID, живой цикл валидации и индекс для автодополнения |
| [ContentCliModularization](ContentCliModularization.md) | 2026-08-15 | Разбиение `gv2-content` на модули команд и поддержки вместо монолитного `main.cpp` |
| [ContentEditorPrerequisites](ContentEditorPrerequisites.md) | 2026-08-18 | Три блокирующих предусловия редактора: строгая классификация схемных изменений, `SetFieldValue`/`RemoveDefinitionEntry` с сохранением комментариев, `.ui.json5` вне `content_hash` с гейтом на устаревшие ключи |
| [CoreBoundaryMigration](CoreBoundaryMigration.md) | 2026-08-17 | Приведение ядра к [ADR-0026](../../ADR/0026-core-and-gameplay-ownership.md): демо в пакет `sample`, `register_type` для обёртки актора, схемы `item`/`location` в `rh`, `actor_v1` до одного поля, гейт `core_boundary_gate_contract` |
| [DesignerAuthoringLayer](DesignerAuthoringLayer.md) | 2026-08-18 | Слой авторинга: признак записи в окне мутации, изоляция сырого состояния, дескриптор команд с отложенной регистрацией, `Storage`/`WritePolicy`, sparse runtime-состояние, `emit`/`on`/`show_screen`, перевод геймплея `rh` |
| [DocumentationRework](DocumentationRework.md) | 2026-08-20 | Authority и совместимость сделаны явными, Lua-контракты разделены, создан `Authoring/`, инструкции очищены, а архив планов свёрнут в проверяемые summaries |
| [EntityAuthoringExtensions](EntityAuthoringExtensions.md) | 2026-08-19 | Декларативное расширение сущностей (`Actor`, `Location`, `Quest`, `Item`) через прототипы в `_ENV`, централизованный реестр расширений и скомпонованные effective method tables |
| [EntityExtensionsHardening](EntityExtensionsHardening.md) | 2026-08-19 | Заморозка реестра расширений стала необходимой, скомпонованная таблица методов — единственным путём поиска, повторное объявление внутри модуля — ошибкой, приём метода в `textsystem` — строго `self` |
| [GameplayEventsAndWorld](GameplayEventsAndWorld.md) | 2026-08-15 | Ordered command validators, конверт и шина событий, подписка по `event_id`, отложенные команды, доменный объект мира и travel-слайс целиком |
| [GameplayServices](GameplayServices.md) | 2026-08-19 | Авторский синтаксис `services.<name> = { … }` для stateless-процессов, координация передачи предмета и начисления золота торговцу в `rh` |
| [HeadlessParityAndReplay](HeadlessParityAndReplay.md) | 2026-08-14 | 21 общий conformance entry point вместо host-локальных дублей, run manifest/digest, replay и golden-прогоны |
| [LifecycleSpecsMigration](LifecycleSpecsMigration.md) | 2026-08-15 | Миграция последнего крупного унаследованного набора: 5326 строк C++ заменены декларативными спеками `Tests/Lua/lifecycle/` |
| [LocalizationPipeline](LocalizationPipeline.md) | 2026-08-15 | Разделение identity и содержимого текста, PO-каталоги внутри package root, резолвинг `TextSpec` в Presentation, fallback и отчёт покрытия |
| [PackageSupport](PackageSupport.md) | 2026-08-16 | Обязательный манифест пакета, набор корней с явным порядком и lock-файлом, Lua внутри пакета, замещение модулей ядра с доступом к базе через `require_base()`, `ScriptSetHash` в run manifest и состав пакетов в сейве |
| [PortableContentCore](PortableContentCore.md) | 2026-08-14 | Portable `GV2ContentCore`, JSON5 + schema validation, immutable repository snapshot, `gv2-content` CLI, интеграция UE/headless и `game.repository` в Lua |
| [RHActorsSimplification](RHActorsSimplification.md) | 2026-08-19 | декларативные контракты полей сущностей (`field.*`), разделение структурных инвариантов и геймплейных предусловий, обобщённое создание экземпляров (`instances.create`), чистое доменное описание `rh/scripts/gameplay/actors.lua` без низкоуровневых утечек runtime |
| [RhGamePackage](RhGamePackage.md) | 2026-08-16 | Игровой пакет `rh`: 11 конкретных сущностей со сменой namespace, переводы и ресурсы вместе с ними, демо-экран без знания об игре, гейт `core_decoupling_gate_contract` |
| [SaveAndLoad](SaveAndLoad.md) | 2026-08-16 | Обратимый канонический кодек, slot-storage примитив, конверт контейнера, загрузка на холодном старте с резолвом редиректов и версии секций с миграциями |
| [SimplifiedAuthoringSurface](SimplifiedAuthoringSurface.md) | 2026-08-18 | Собственное окружение authoring-скриптов, автообнаружение модулей с генерируемым манифестом, `require_*`/`spend_*`, источник презентации; 791 строка Lua в `rh` сведена к 352 при том же геймплее |
| [TestArchitectureAndLuaSpecs](TestArchitectureAndLuaSpecs.md) | 2026-08-15 | Lua spec runner в `Tests/Lua/`, замороженный тестовый корпус, единый источник pinned-значений, миграция world и command validators в спеки |
| [TestGameplaySlice](TestGameplaySlice.md) | 2026-08-17 | Три локации и экрана, карта `market↔tavern↔gate`, перемещение за выносливость, покупки и заработок; первый Lua внутри игрового пакета и первое перекрытие модуля ядра |
| [TextSystemLayer](TextSystemLayer.md) | 2026-08-19 | Три слоя вместо двух: набор пакетов собирается из данных, локации и переходы принадлежат `textsystem`, стоимость перехода — игре; `Scripts/gameplay/` в ядре удалён |

## Как архивировать следующий план

1. Отметить все tasks/milestones выполненными и закоммитить полный каталог плана отдельным commit.
2. Создать `<PlanName>.md`: краткий результат каждого этапа, каждый task ID и исходное название ровно один раз, ссылки на owner contracts и полный `source_commit`.
3. Проверить `git cat-file -e <source_commit>:Docs/Plans/<PlanName>/<path>` для всех исходных файлов и восстановить representative файл через `git show`.
4. Удалить исходный каталог, обновить этот index и активный router вторым archive commit. Zip и второй формат архива не создавать.
