---
title: Archived Implementation Plans
status: archived
version: 1.6
updated: 2026-08-15
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
