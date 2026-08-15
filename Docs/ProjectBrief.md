---
title: GV2 Project Brief
status: normative
version: 1.0
updated: 2026-08-14
depends_on:
  - Architecture/Overview.md
  - Architecture/GlossaryAndNaming.md
  - Architecture/SystemContextAndComponents.md
  - ADR/README.md
---

# Кратко о проекте GV2

Этот документ предназначен для первого знакомства с проектом. Он объясняет общую цель и принятый архитектурный подход, но не заменяет нормативные subsystem contracts и ADR из [индекса документации](README.md).

## Общая цель

GV2 — single-player 2D/2.5D data-driven игра на Unreal Engine 5. Проект строится так, чтобы игровую логику и контент можно было развивать независимо от визуальной реализации Unreal Engine, а основные игровые сценарии — проверять без запуска редактора или графического клиента.

Ключевые продуктовые свойства:

- gameplay и контент расширяются данными и Lua-модулями;
- Unreal Engine отвечает за качественную presentation: UI, графику, звук, ввод и взаимодействие с платформой;
- core и mods проходят одинаковую проверку до запуска игровой session;
- сохранение игры не зависит от состояния Widgets, Actors или других временных объектов Unreal;
- один и тот же gameplay-сценарий должен одинаково выполняться в UE и standalone Headless host.

## Архитектура в одном абзаце

Статический контент загружается из core/mod packages и превращается в проверенный immutable `GameDataRepository`. Lua создаёт canonical gameplay-state и изменяет его только через Commands. C++ организует lifecycle, безопасную передачу value-only данных и доступ к платформенным возможностям. Unreal Engine отображает полученное desired presentation и преобразует действия игрока в semantic input, не принимая gameplay-решений самостоятельно.

## Основные части системы

| Часть | Ответственность |
|---|---|
| External Content | Definitions, schemas, Lua modules, localization, resources и package manifests |
| Content Core | Проверка контента и построение immutable repository snapshot |
| Lua Gameplay Runtime | Canonical state, gameplay rules, Commands, Events и desired presentation |
| C++ Application and Bridge | Запуск session, lifecycle, DTO boundary, storage и platform operations |
| UE Presentation | Экраны, Widgets, визуализация, звук, ввод и восстановимая проекция состояния |
| Headless Host | Выполнение тех же gameplay-сценариев без Unreal presentation |

Главное правило ownership: данные должны иметь одного владельца. Gameplay-state принадлежит Lua, static definitions — repository, а физическая presentation — Unreal Engine.

## Как проходит действие игрока

1. Unreal преобразует нажатие или выбор игрока в semantic input.
2. Lua получает соответствующий Command и проверяет его до изменения состояния.
3. Gameplay service изменяет canonical state.
4. После успешного изменения публикуются Gameplay Events как уже произошедшие факты.
5. Lua формирует новое desired presentation.
6. Unreal приводит UI и world presentation к этому состоянию.

Такое разделение позволяет тестировать правила игры отдельно от интерфейса и не допускает появления второго gameplay-state в Blueprint или C++.

## Что важно запомнить сразу

Четыре правила определяют почти все остальные решения: gameplay authority принадлежит Lua, любое изменение состояния проходит через Command, граница Lua/C++ передаёт только значения, а repository публикуется целиком и атомарно.

Полный список инвариантов и non-goals первой версии — в [Architecture Overview](Architecture/Overview.md); причины каждого выбора — в [ADR Index](ADR/README.md). Дублировать эти списки здесь намеренно не будем.

## Текущее состояние

Content pipeline завершён: контент из `GameData/core` проверяется схемами, собирается в immutable repository snapshot и читается из Lua через `game.repository`. Один и тот же corpus даёт одинаковый результат в Unreal, `gv2-headless` и CLI `gv2-content`.

Следующий крупный шаг — canonical gameplay-state и Command/Event path: без них gameplay остаётся заглушкой, а vertical slice не закрывается.

Подробная разбивка по подсистемам: [Implementation Status](ImplementationStatus.md).

## Куда идти дальше

- [Architecture Overview](Architecture/Overview.md) — общая модель, инварианты и границы.
- [Glossary and Naming](Architecture/GlossaryAndNaming.md) — канонические термины и правила именования.
- [System Context and Components](Architecture/SystemContextAndComponents.md) — ownership и направления зависимостей.
- [Implementation Status](ImplementationStatus.md) — что уже реализовано.
- [Build and Tooling](Architecture/BuildAndTooling.md) — как собрать, запустить и проверить проект.
- [ADR Index](ADR/README.md) — принятые решения и причины выбора.
- [UI Index](UI/README.md) — устройство presentation/UI.

Перед изменением конкретной подсистемы следует открыть её contract через [основной индекс](README.md) и прочитать связанные accepted ADR.
