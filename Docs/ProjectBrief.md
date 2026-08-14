---
title: GV2 Project Brief
status: normative
version: 0.1
updated: 2026-08-13
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

## Ключевые принятые решения

- **Lua является gameplay authority.** C++ и Blueprint не содержат параллельную доменную модель и не меняют canonical state напрямую.
- **Все gameplay-изменения проходят через Commands.** Validators проверяют намерение до mutation; Events сообщают только о состоявшемся результате.
- **Граница Lua/C++ передаёт только значения.** UObject, raw pointers, Lua callbacks и физические asset paths через неё не проходят.
- **Контент отделён от runtime-state.** Definition является immutable описанием; сохраняемый Runtime Instance ссылается на него по Stable ID.
- **Stable ID имеет единый формат** `<namespace>:<kind>.<path>`. Опубликованный ID нельзя повторно использовать для нового смысла.
- **Repository публикуется атомарно.** Ошибочный candidate не становится видимым, а active session продолжает использовать закреплённый snapshot.
- **Reload выполняется через controlled session restart.** Repository и Lua-код не подменяются внутри работающей session.
- **Overrides являются полными.** Последний provider целиком заменяет definition с тем же ID; implicit deep merge отсутствует.
- **Gameplay runtime portable.** UE и Headless используют один Lua runtime, один Content Core и одинаковые contracts.
- **UI является presentation, а не источником истины.** Lua описывает желаемое состояние экранов, а Unreal создаёт конкретные Blueprint Screen Templates и Widgets.
- **Presentation paths централизованы.** Text, images, repeated elements, Screen Fields и input не получают локальные альтернативные механизмы внутри отдельных Widgets.
- **Modding расширяет те же публичные точки.** Mods используют namespaces, definitions, commands, validators, events и документированные lifecycle hooks без прямого доступа к Unreal API.

## Осознанные ограничения первой версии

В ближайшую архитектурную цель не входят multiplayer/replication, hostile-code sandbox для Lua-модов, live repository mutation, universal rollback, event sourcing и универсальный patch/deep-merge язык. Новая абстракция добавляется только под конкретный игровой сценарий или измеренную проблему.

## Текущий фокус

Проект укрепляет end-to-end vertical slice: проверенный контент должен пройти через repository, Lua gameplay, Commands/Events, UI projection, save/load и одинаково воспроизводиться в Unreal и Headless.

Текущий крупный implementation track — [PortableContentCore](Plans/PortableContentCore/README.md): общий pipeline от package descriptors и JSON5 definitions до immutable repository snapshot и CLI validation.

## Куда идти дальше

- [Architecture Overview](Architecture/Overview.md) — общая модель и архитектурные границы.
- [Glossary and Naming](Architecture/GlossaryAndNaming.md) — канонические термины и правила именования.
- [System Context and Components](Architecture/SystemContextAndComponents.md) — ownership и направления зависимостей.
- [ADR Index](ADR/README.md) — принятые решения и причины выбора.
- [UI Index](UI/README.md) — устройство presentation/UI.
- [Implementation Plans](Plans/README.md) — текущие планы и отмечаемый прогресс.

Перед изменением конкретной подсистемы следует открыть её contract через [основной индекс](README.md) и прочитать связанные accepted ADR.
