---
title: When to Write Cpp
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/Overview.md
decisions:
  - ../ADR/0020-cpp-scope-criterion.md
---

# Решить, нужен ли C++

> **Задача:** выбрать слой реализации до создания нового native module/service/API.
> **Предмет:** критерий C++ scope и минимальное представление Lua/UE boundary.
> **Нормативно:** [Overview § Границы C++](../Architecture/Overview.md#границы-c), [ADR-0020](../ADR/0020-cpp-scope-criterion.md).

Повторяемость видна на трёх типах изменений: pre-VM content pipeline в `Source/GV2ContentCore/`, UObject/UMG adapters в `Source/GV2/Private/UI/` и Lua-owned gameplay/authoring в `Scripts/authoring/` и `GameData/rh/scripts/`.

## Процедура

1. Одним предложением назовите capability, которой требует задача.
2. Ответьте на два вопроса:
   - Нужны filesystem/process/threads/native library/UObject/UMG/Slate/platform API, недоступные Lua по trust model?
   - Код обязан работать до создания Lua VM или без неё?
3. Если оба ответа «нет», реализация принадлежит Lua. Удобство типизации, привычка команды и потенциальная производительность не являются третьим критерием.
4. Если ответ «да», оставьте в C++ только capability adapter/orchestration. Gameplay rules и canonical state остаются Lua-owned.
5. Спроектируйте минимальный boundary: scalar вместо struct, Stable ID вместо object, opaque bytes вместо parsed tree. Не передавайте canonical state.
6. Проверьте dependency direction по [Dependency Map](../Architecture/DependencyMap.md) и повторное использование существующего module/API.
7. В change set укажите, какой критерий выполнен. Новый mechanism/dependency direction требует ADR и contract update.

## Примеры решения

- Repository/parser до VM — C++ по второму критерию.
- UMG Widget adapter — C++ по первому критерию; Screen content и bindings остаются value-only Lua data.
- Command, Validator, Event, Service, migration и state hash — Lua, если не требуют native capability до VM.
- Save host хранит opaque bytes; Lua владеет container semantics и migration.
