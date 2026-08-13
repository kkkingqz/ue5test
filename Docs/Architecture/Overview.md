---
title: Architecture Overview
status: normative
version: 1.3
updated: 2026-08-12
depends_on:
  - ../README.md
  - ../ADR/README.md
---

# Architecture Overview

GV2 — single-player 2D/2.5D data-driven игра на Unreal Engine 5. Gameplay реализуется в Lua, presentation — в Unreal Engine, интеграционная граница — в C++.

Authoritative gameplay runtime является portable и запускается двумя host-ами: UE application и standalone `gv2-headless`. Host не меняет gameplay semantics.

## Главный инвариант

Всё, что необходимо для однозначного продолжения игры после загрузки, хранится в canonical Lua state либо выводится из него и закреплённого snapshot `GameDataRepository`. Widgets, Actors, streaming handles и animation state не являются источником gameplay-истины.

## Слои

| Слой | Владеет | Не владеет |
|---|---|---|
| External Content | Definitions, schemas, Lua modules, localization, manifests, resource mappings | Runtime state, UObject instances |
| Lua Gameplay Runtime | Canonical state, commands, gameplay services, post-commit events, desired UI | UObject, UWorld, platform I/O |
| C++ Runtime Boundary | Bootstrap, DTO conversion, repository build, save codec/storage, typed UE adapters | Gameplay rules и параллельная domain model |
| UE Presentation | UMG, Blueprint, input capture, rendering, audio, animation, streaming, Actor projection | Canonical gameplay decisions |

## Категории состояния

| Категория | Owner | Восстановление |
|---|---|---|
| Canonical gameplay-state | Lua | Из save или new-game defaults |
| Static definitions | GameDataRepository | Из core/mod packages |
| Lua runtime state | Lua VM | Перестраивается при bootstrap/load |
| UE world/UI projection | Presentation | Из state + repository + presentation snapshot |
| Ephemeral UX state | UE/Blueprint | Не сохраняется |

Definition, runtime instance и physical UE object — разные сущности. Runtime state хранит Stable ID, а не mutable definition table или UObject reference.

## Основной runtime-поток

1. UE преобразует взаимодействие пользователя в schema-defined semantic input.
2. Runtime проверяет session/UI identity и передаёт `command_id` в Lua `Command Dispatcher`.
3. Dispatcher проверяет phase, payload и ordered validators до первой mutation.
4. Command handler изменяет canonical state через gameplay services.
5. После успешного commit EventBus публикует неизменяемые gameplay facts.
6. Lua строит новый UI/presentation snapshot и при необходимости one-shot effects.
7. UE reconciles desired presentation; поздний input отклоняется по generation/instance/revision.

Lua UI-document содержит Screen instances: `screen_id`, stable instance identity и полный набор schema-validated Screen Fields. UE разрешает `screen_id` через Screen Registry и создаёт concrete Widget Blueprint, унаследованный от общего Screen base. Lua не описывает physical Widget tree, а добавление concrete Screen не требует per-screen C++ class или branch.

Runtime-authored text, content images, repeated elements, Screen Field values и Semantic Input используют единые concern-specific presentation paths. Composite Widgets не вводят локальные resolvers/factories/ingress; различия layout и renderer capabilities остаются UE-local.

## Repository и content

- Source format: UTF-8 JSON5.
- Stable ID: `<namespace>:<kind>.<path>`.
- Core загружается первым, затем enabled mods в явном пользовательском order.
- Совпадающий ID между packages означает full replacement последним provider.
- Duplicate ID внутри одного package — ошибка.
- File enumeration order не является gameplay semantics.
- Candidate snapshot публикуется только после полной validation.
- Active session удерживает immutable read handle до controlled restart.

## Session model

- В процессе существует не более одной active session и одной Lua VM.
- Menu и Game используют один полный lifecycle; одновременно они не существуют.
- Load another save, return to menu и content reload выполняются как replacement session.
- Repository failure до создания VM показывает UE-native recovery surface.
- Session bootstrap failure уничтожает candidate и создаёт recovery menu session, если это возможно.

## Trust model

Core и включённые Lua-моды являются trusted gameplay code, но не получают filesystem, process, native library, raw UObject или Blueprint reflection API. Ограничение API обеспечивает portability и контролируемые границы, а не security sandbox.

## v1 non-goals

- Multiplayer, replication и dedicated server.
- Hostile-code sandbox и hard CPU/memory quotas.
- Event sourcing, universal rollback и replay как источник save.
- Coroutines/yield и async gameplay commands.
- Partial UI patch protocol.
- Live mutation repository внутри active session.
- Deep merge/универсальный patch language.
- Production hot reload Lua/Pak без restart.

## Vertical slice acceptance

Архитектура считается подтверждённой, когда один сценарий проходит end-to-end:

1. Core package определяет location, два screens, item, localized text и presentation resources.
2. Lua строит initial UI/presentation snapshot.
3. UI отправляет travel command; validator проверяет доступность и resource prepare.
4. Handler меняет location; EventBus публикует `leave`/`enter` facts в стабильном порядке.
5. Тестовый mod добавляет item и полностью overrides один screen.
6. Save/load восстанавливает state, PRNG, instance IDs и presentation без сериализации functions/handles.
7. Сценарий работает на Windows и Linux без доменных C++ классов Item/Quest/Location.
8. Recorded command sequence воспроизводится standalone runner-ом без Unreal Engine с тем же gameplay result.

Конкретные имена C++ классов являются рекомендацией реализации, а не частью архитектурной совместимости. Нормативны ownership, dependency direction, DTO boundary и observable behavior.
