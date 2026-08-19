---
title: Receiver Contract Tasks
status: normative
version: 1.0
updated: 2026-08-19
depends_on:
  - RegistryIntegrity.md
  - ../../Architecture/LuaRuntimeContract.md
decisions:
  - ../../ADR/0030-textsystem-layer-and-data-driven-package-set.md
  - ../../ADR/0031-entity-authoring-extensions.md
---

# M2 — Receiver Contract

> **Материализует:** [ADR-0031 § 2.3](../../ADR/0031-entity-authoring-extensions.md) в части контракта `self`.
> **Задачи:** EEH-06…08.
> **Результат:** метод сущности отвечает о получателе, а не о том, кого он нашёл в глобальном состоянии.

## Результат этапа

`textsystem` перестаёт содержать неявный переход к игроку. Внутри одного файла остаётся одна конвенция приёма метода.

## Задачи

- [ ] **EEH-06 — Убрать переход к игроку из `textsystem`**
  - Зависимости: EEH-03.
  - `Actor:is_player` и `Actor:is_npc` в `GameData/textsystem/scripts/gameplay/actors.lua` объявлены через двоеточие, то есть уже имеют неявный `self`, и при этом принимают дополнительный явный первый параметр: `self_or_nil or self or …player()`. Отсюда два следствия. Вызов `actor:is_player(other)` отвечает про `other`, а не про `actor`. Вызов без получателя молча отвечает про игрока — то есть `is_player()` всегда истинно. Соседний `Actor:require_location` в том же файле работает строго с `self`.
  - Done: методы принимают только получателя; дополнительный первый параметр удалён; переход к `game.instances.actors.player()` из `textsystem` удалён; вызов без получателя даёт типизированную ошибку, а не молчаливый ответ про игрока; спека TextSystem tier покрывает оба отрицательных случая.
  - Evidence: `GameData/textsystem/scripts/gameplay/actors.lua`, `Tests/Lua/world/`.

- [ ] **EEH-07 — Одна конвенция приёма на файл**
  - Зависимости: EEH-06.
  - Done: все методы `Actor` и `Location` в `textsystem` объявлены единообразно и обращаются к полям через `self`; расхождений конвенции внутри файла не осталось; правило приёма метода записано в [Lua Runtime Contract § Entity Extensions](../../Architecture/LuaRuntimeContract.md) как нормативное, а не как стиль.
  - Evidence: `GameData/textsystem/scripts/gameplay/actors.lua`, `Docs/Architecture/LuaRuntimeContract.md`.

- [ ] **EEH-08 — Сквозная верификация**
  - Зависимости: EEH-07.
  - Done: полный `ctest`, `gv2-headless --self-test`, `--check-scripts`, `validate_docs.py`, `validate_core_boundary.py` и Unreal automation зелёные; в golden изменились только `script_set_hash` и производный `digest_hash`, воспроизведённые манифестом; `final_screen_id`, `final_screen_fields`, хэш состояния и `repository_content_hash` не изменились; спеки уровней Core и TextSystem проходят без `rh`.
  - Evidence: отчёт CTest, golden-прогон.

## Проверка milestone

- [ ] `Actor:is_player()` отвечает о получателе при любых аргументах.
- [ ] Вызов метода сущности без получателя отклоняется.
- [ ] В `textsystem` нет обращений к глобальному игроку из методов сущностей.
- [ ] Спеки Core и TextSystem проходят без `rh`.
