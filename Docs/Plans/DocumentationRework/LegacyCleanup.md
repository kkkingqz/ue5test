---
title: Legacy Cleanup Tasks
status: active
version: 1.4
updated: 2026-08-20
depends_on:
  - RulesAndVisibility.md
  - ../../Architecture/GlossaryAndNaming.md
---

# M2 — Legacy Cleanup

> **Материализует:** раздел «Принятые решения» [плана](README.md) в части гейта на удалённый API.
> **Задачи:** DOC-05…06.
> **Результат:** инструкция не может ссылаться на механизм, которого нет, — это отклоняется проверкой.

## Результат этапа

Класс дефекта «документация учит удалённому API» перестаёт быть невидимым. Сегодня его не ловит ничто: `Concepts/` и `Guides/` проверяются только на front matter и ссылки.

## Задачи

- [x] **DOC-05 — Гейт на удалённые идентификаторы**
  - Зависимости: DOC-04.
  - Ключевое ограничение области: **в планах и предложениях упоминание удалённого API законно** — они его удаляют и обязаны называть. Наивный grep по всему `Docs/` дал бы ложные срабатывания там, где документ прав: план RHActorsSimplification обязан называть `validate_amount` и `RESOURCES`, UI-контракты обязаны называть `/Game/...`, потому что запрещают его.
  - Done: проверка встроена в существующий `validate_docs.py` и использует его единую классификацию инструктирующих тиров; после DOC-10 новый `Authoring/` попадает под тот же гейт без отдельного списка каталогов; правила удалённых идентификаторов ведутся в одной machine-readable таблице и для каждого pattern содержат scope по каталогам/файлам, причину удаления, canonical replacement и явно обоснованные исключения; `/Game/...` запрещён в gameplay/Lua data examples, но не блокирует Editor-only пример asset path; self-test содержит отрицательные cases и положительные cases для нормативного запрета, исторического упоминания и разрешённого Editor-only контекста; проверка подключена к CTest; добавление удалённого механизма расширяет таблицу в том же change set, которым он удаляется.
  - Evidence: `Tools/Documentation/validate_docs.py`, таблица legacy rules, `CMakeLists.txt`.

- [x] **DOC-06 — Чистка legacy-текста**
  - Зависимости: DOC-05.
  - Точный набор упоминаний берётся из baseline DOC-00, а не из чисел на момент составления плана.
  - Done: каждое упоминание классифицировано как нормативный запрет, historical record, разрешённый scoped пример либо остаток; остатки убраны; исключение требует line-local marker с причиной; гейт DOC-05 проходит. Примеры legacy-форм в `AGENTS.md` сверены и остаются актуальными; exhaustive owner — machine-readable таблица валидатора.
  - Evidence: `Docs/Guides/`, `Docs/Concepts/`, `Docs/Architecture/`, `Docs/UI/`.

## Проверка milestone

- [x] Гейт отклоняет инструктивное упоминание удалённого API в `Guides/`, `Concepts/` и после создания в `Authoring/`.
- [x] Гейт не срабатывает на plans/proposals, нормативные запреты и явно разрешённые Editor-only примеры.
- [x] Отрицательные и положительные cases воспроизводятся self-test-ом.
- [x] В инструктирующих тирах не осталось описаний удалённых механизмов как рабочих.

## Audit 2026-08-20

| Pattern | Результат |
|---|---|
| `register_type`, `actor_decorator` | Удалены из Guides, Concepts и актуального runtime contract; остались в ADR и plan/proposal records |
| `game.commands.handlers.register`, `game.commands.validators.register` | Удалены из Guides; низкоуровневый registry упоминается только в его нормативном programmer contract |
| `validate_amount`, `RESOURCES` | Встречаются только как причина принятого ADR и в plan baseline/history |
| `/Game/...` | Остался в UI/Modding и UI plans/proposals как запрет либо Editor/automation asset scope; gameplay/Lua examples отсутствуют |

Guide по методам сущностей переведён на `Actor` + `field.*`, Guide по командам — на `commands`, `validate`, `fail` и `services`. Устаревший раздел Guides о якобы отсутствующих package discovery и load удалён.
