---
title: Archived Implementation Plans
status: archived
version: 1.16
updated: 2026-08-16
depends_on:
  - ../README.md
---

# Архив выполненных планов

Здесь хранятся планы, все задачи которых выполнены. Архивный план **не является нормативным** и не источник задач: актуальное поведение описывают subsystem contracts в `Docs/Architecture` и `Docs/UI`. Документ сохраняется только как implementation record — он отвечает на вопрос «почему сделано именно так» и содержит evidence по каждой задаче.

Архивный план запрещено использовать как описание текущего API: если он расходится с contract, прав contract.

| План | Завершён | Результат |
|---|---|---|
| [PortableContentCore](PortableContentCore/README.md) | 2026-08-14 | Portable `GV2ContentCore`, JSON5 + schema validation, immutable repository snapshot, `gv2-content` CLI, интеграция UE/headless и `game.repository` в Lua |
| [HeadlessParityAndReplay](HeadlessParityAndReplay/README.md) | 2026-08-14 | 21 общий conformance entry point вместо host-локальных дублей, run manifest/digest, replay и golden-прогоны |
| [CanonicalGameplayState](CanonicalGameplayState/README.md) | 2026-08-15 | Canonical state, module lifecycle hooks, instance identity, state hash в run digest, ActorRegistry и mutation window |
| [TestArchitectureAndLuaSpecs](TestArchitectureAndLuaSpecs/README.md) | 2026-08-15 | Lua spec runner в `Tests/Lua/`, замороженный тестовый корпус, единый источник pinned-значений, миграция world и command validators в спеки |
| [ContentAuthoringTools](ContentAuthoringTools/README.md) | 2026-08-15 | Справочник и заготовки из схем, быстрая проверка Lua-модулей, обратные ссылки и переименование ID, живой цикл валидации и индекс для автодополнения |
| [GameplayEventsAndWorld](GameplayEventsAndWorld/README.md) | 2026-08-15 | Ordered command validators, конверт и шина событий, подписка по `event_id`, отложенные команды, доменный объект мира и travel-слайс целиком |
| [LocalizationPipeline](LocalizationPipeline/README.md) | 2026-08-15 | Разделение identity и содержимого текста, PO-каталоги внутри package root, резолвинг `TextSpec` в Presentation, fallback и отчёт покрытия |
| [LifecycleSpecsMigration](LifecycleSpecsMigration/README.md) | 2026-08-15 | Миграция последнего крупного унаследованного набора: 5326 строк C++ заменены декларативными спеками `Tests/Lua/lifecycle/` |
| [ContentCliModularization](ContentCliModularization/README.md) | 2026-08-15 | Разбиение `gv2-content` на модули команд и поддержки вместо монолитного `main.cpp` |
| [SaveAndLoad](SaveAndLoad/README.md) | 2026-08-16 | Обратимый канонический кодек, slot-storage примитив, конверт контейнера, загрузка на холодном старте с резолвом редиректов и версии секций с миграциями |
| [PackageSupport](PackageSupport/README.md) | 2026-08-16 | Обязательный манифест пакета, набор корней с явным порядком и lock-файлом, Lua внутри пакета, замещение модулей ядра с доступом к базе через `require_base()`, `ScriptSetHash` в run manifest и состав пакетов в сейве |
| [RhGamePackage](RhGamePackage/README.md) | 2026-08-16 | Игровой пакет `rh`: 11 конкретных сущностей со сменой namespace, переводы и ресурсы вместе с ними, демо-экран без знания об игре, гейт `core_decoupling_gate_contract` |
| [CommandHandlerRegistry](CommandHandlerRegistry/README.md) | 2026-08-16 | Реестр обработчиков по `command_id` вместо цепочки, отказ на неизвестную команду, развязка `ingress` от игровых модулей, команды из пакета без C++ |
| [TestGameplaySlice](TestGameplaySlice/README.md) | 2026-08-17 | Три локации и экрана, карта `market↔tavern↔gate`, перемещение за выносливость, покупки и заработок; первый Lua внутри игрового пакета и первое перекрытие модуля ядра |
| [CoreBoundaryMigration](CoreBoundaryMigration/README.md) | 2026-08-17 | Приведение ядра к [ADR-0026](../../ADR/0026-core-and-gameplay-ownership.md): демо в пакет `sample`, `register_type` для обёртки актора, схемы `item`/`location` в `rh`, `actor_v1` до одного поля, гейт `core_boundary_gate_contract` |
| [DesignerAuthoringLayer](DesignerAuthoringLayer/README.md) | 2026-08-18 | Слой авторинга: признак записи в окне мутации, изоляция сырого состояния, дескриптор команд с отложенной регистрацией, `Storage`/`WritePolicy`, sparse runtime-состояние, `emit`/`on`/`show_screen`, перевод геймплея `rh` |
| [SimplifiedAuthoringSurface](SimplifiedAuthoringSurface/README.md) | 2026-08-18 | Собственное окружение authoring-скриптов, автообнаружение модулей с генерируемым манифестом, `require_*`/`spend_*`, источник презентации; 791 строка Lua в `rh` сведена к 352 при том же геймплее |
| [ContentEditorPrerequisites](ContentEditorPrerequisites/README.md) | 2026-08-18 | Три блокирующих предусловия редактора: строгая классификация схемных изменений, `SetFieldValue`/`RemoveDefinitionEntry` с сохранением комментариев, `.ui.json5` вне `content_hash` с гейтом на устаревшие ключи |
| [TextSystemLayer](TextSystemLayer/README.md) | 2026-08-19 | Три слоя вместо двух: набор пакетов собирается из данных, локации и переходы принадлежат `textsystem`, стоимость перехода — игре; `Scripts/gameplay/` в ядре удалён |
