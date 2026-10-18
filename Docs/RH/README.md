---
title: RH Game Documentation Index
status: informative
version: 1.1
updated: 2026-10-18
---

# Документация игры RH

Каталог содержит правила игры пакета `rh`: архитектуру геймплея, механики и их данные. Документы обязательны для кода и данных `GameData/rh/`, но не для движка: при расхождении с contracts из [Architecture](../Architecture/README.md) и [UI](../UI/README.md) прав contract движка, а расхождение исправляется в документе игры.

Механизмы, которых не хватает `rh` в `textsystem` и `core`, записываются в [Engine Gaps](../Status/EngineGaps.md), недостающие экраны — в [Screen Requests](../Status/ScreenRequests.md).

| Документ | Статус | Содержание |
|---|---|---|
| [Gameplay Architecture](GameplayArchitecture.md) | draft | Устройство пакета: слои, модель данных, общие механики, модули диалога, боя и торговли, итог команды, сборка экрана, расширяемость и стратегия проверки |
| [Hunter Gameplay](HunterGameplay.md) | draft | Правила игры «Охотник»: классы, формулы, предметы, локации, охота, добыча, встречи, торговля, танец, старт и конец игры, ресурсы |
