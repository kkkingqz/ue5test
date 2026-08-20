---
title: Guides Index
status: informative
version: 2.0
updated: 2026-08-20
depends_on:
  - ../README.md
---

# Руководства

Раздел отвечает на вопрос «как выполнить типовую задачу в рамках уже принятой архитектуры». Руководство не определяет архитектуру, не меняет contract и использует только публичные точки расширения.

**Раздел не является нормативным.** При расхождении с contract прав contract. Руководства не копируют нормативные определения, а ссылаются на них.

## Доступные руководства

| Руководство | Задача |
|---|---|
| [Add Lua Module](AddLuaModule.md) | Добавить programmer Lua module в общий graph |
| [Add Lua Spec](AddLuaSpec.md) | Проверить Lua-правило обоими host-ами на правильном tier |
| [Add Authoring Surface](AddAuthoringSurface.md) | Расширить designer-facing `_ENV` и adapter |
| [Add Screen Field](AddScreenField.md) | Добавить reusable Screen Field schema и Dynamic Screen Element |
| [Regenerate Golden](RegenerateGolden.md) | Воспроизвести и обновить golden manifest/digest |
| [When to Write C++](WhenToWriteCpp.md) | Применить C++ scope criterion к конкретной задаче |

Задачи автора игры — Definitions, Commands, Events и Screens — находятся в [Authoring](../Authoring/README.md), а не здесь.

## Структура руководства

Каждое руководство по возможности содержит: цель, предусловия, ссылки на нормативные contracts, список файлов, минимальный пример, способ проверить результат и типичные ошибки.
