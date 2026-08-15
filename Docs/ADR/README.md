---
title: Architecture Decision Records
status: normative
version: 2.2
updated: 2026-08-15
---

# Architecture Decision Records

Accepted ADR фиксирует решение и причины. Контракты содержат актуальное полное правило; ADR объясняет выбор. Изменение accepted решения создаёт новый ADR и помечает старый `superseded`.

| ADR | Status | Decision |
|---|---|---|
| [0000](0000-markdown-documentation-authority.md) | accepted | Markdown в `Docs` — единственный нормативный набор |
| [0001](0001-authority-boundaries.md) | accepted | Lua state / C++ boundary / UE presentation ownership |
| [0002](0002-stable-id-format.md) | accepted | `<namespace>:<kind>.<path>` и no reuse |
| [0003](0003-command-and-event-model.md) | accepted | Commands + validators; EventBus post-commit only |
| [0004](0004-lua-state-mutation.md) | accepted | Mutable table без proxies; mutation только command path |
| [0005](0005-value-only-async-boundary.md) | accepted | DTO ingress; C++ не хранит Lua callbacks |
| [0006](0006-repository-reload-and-session-pinning.md) | accepted | Pinned snapshot; reload через restart |
| [0007](0007-lua-module-environment.md) | accepted | Одна VM/_G; no globals; `game.mods[mod_id]` |
| [0008](0008-minimal-repository-indexes.md) | accepted | Только необходимые indexes v1 |
| [0009](0009-explicit-schema-defaults.md) | accepted | Optional absent остаётся absent без explicit default |
| [0010](0010-portable-runtime-and-headless-simulation.md) | accepted | Portable gameplay runtime, UE/headless hosts и общий Command Dispatcher path |
| [0011](0011-blueprint-screen-templates.md) | accepted | UE-authored Screen Templates, Lua-authored fields и generic C++ screen layer |
| [0012](0012-centralized-ui-theme.md) | accepted | Один UE-side theme asset и обязательное central styling reusable UI components |
| [0013](0013-unified-text-pipeline.md) | accepted | Единый TextSpec pipeline и data-driven typography/markup tokens |
| [0014](0014-three-mode-image-resources.md) | superseded | Три image render mode; manual catalog authoring заменён ADR-0015 |
| [0015](0015-filesystem-discovered-image-resources.md) | superseded | Recursive discovery сохранён; JSON sidecar заменён ADR-0016 |
| [0016](0016-png-suffix-image-metadata.md) | accepted | Image mode в `.png` / `.tile.png` / `.9.png`, без sidecar |
| [0017](0017-centralized-ui-presentation-paths.md) | accepted | Единые paths для text, images, collections, Screen Fields и Semantic Input |
| [0018](0018-portable-content-core-module.md) | accepted | Нижний portable Content module и единый `BuildRepository()` path |
| [0019](0019-content-host-support-module.md) | accepted | `GV2ContentHostSupport` — filesystem package discovery отдельно от filesystem-free `GV2ContentCore` |
| [0020](0020-cpp-scope-criterion.md) | accepted | Критерий принадлежности кода C++ и минимальное представление на boundary |
| [0021](0021-opaque-save-container.md) | accepted | State не пересекает boundary; save — непрозрачные байты через slot storage |
| [0022](0022-external-translation-catalog.md) | accepted | Репозиторий владеет identity текста; переводы — внешние PO-каталоги, резолвит host |
| [0023](0023-stable-id-publication-freeze.md) | accepted | Жизненный цикл Stable ID: авторский rename до релиза и безусловный freeze после публикации |
| [0024](0024-lua-spec-runner.md) | accepted | Lua-правила проверяются Lua-спеками из `Tests/Lua/` через один generic runner |

## Template

```markdown
---
title: ADR-NNNN: Title
status: proposed | accepted | superseded | rejected
date: YYYY-MM-DD
---

# ADR-NNNN: Title
## Context
## Decision
## Consequences
## Rejected alternatives
```
