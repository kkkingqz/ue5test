---
title: Architecture Invariants Index
status: normative
version: 1.3
updated: 2026-08-20
depends_on:
  - Overview.md
---

# Индекс архитектурных инвариантов

> **Владеет:** только идентификаторами `INV-NNN`.
> **Не владеет:** формулировками инвариантов — нормативным остаётся contract, а не индекс.
> **Инварианты:** перечислены ниже; собственных не вводит.
> **Реализация:** идентификаторы используются в спеках, планах, ADR и review.
> **Проверки:** каждый инвариант проверяется тестами своей подсистемы.

Индекс намеренно не повторяет нормативный текст. Если формулировка нужна целиком — она в указанном источнике.

| ID | Инвариант | Нормативный источник |
|---|---|---|
| INV-001 | Lua владеет canonical gameplay-state | [Overview](Overview.md), [Canonical State and Save](CanonicalStateAndSave.md) |
| INV-002 | Repository публикуется целиком; снимок неизменяем, сессия закрепляет его до перезапуска | [GameDataRepository Contract](GameDataRepositoryContract.md), [ADR-0006](../ADR/0006-repository-reload-and-session-pinning.md) |
| INV-003 | Состояние меняется только пока исполняется command handler | [Commands and Events § Mutation authority](CommandsAndEvents.md) |
| INV-004 | Событие публикуется только после успешного commit и не может быть отменено | [Commands and Events](CommandsAndEvents.md) |
| INV-005 | Не более одной активной сессии и одной Lua VM в процессе | [Bootstrap and Session Lifecycle](BootstrapAndSessionLifecycle.md) |
| INV-006 | Реестры и таблицы экспорта модулей замораживаются; поздняя регистрация и мутация экспортов запрещены | [Runtime Facade and Registries](RuntimeFacadeAndRegistries.md), [Lua Runtime Contract](LuaRuntimeContract.md), [ADR-0025](../ADR/0025-lua-module-replacement-and-export-freezing.md) |
| INV-007 | Через boundary проходят только значения; UObject, указатели и функции — нет | [Lua Runtime Contract](LuaRuntimeContract.md), [ADR-0005](../ADR/0005-value-only-async-boundary.md) |
| INV-008 | Canonical state не пересекает boundary; host получает байты и скаляры | [ADR-0021](../ADR/0021-opaque-save-container.md), [Lua Runtime Contract](LuaRuntimeContract.md) |
| INV-009 | Stable ID имеет единый формат и не переиспользуется после публикации | [Stable ID Specification](StableIDSpecification.md), [ADR-0023](../ADR/0023-stable-id-publication-freeze.md) |
| INV-010 | Override заменяет definition целиком; deep merge отсутствует | [GameDataRepository Contract](GameDataRepositoryContract.md) |
| INV-011 | Одинаковый вход даёт одинаковый снимок и хэш либо одинаковый упорядоченный набор диагностик | [GameDataRepository Contract](GameDataRepositoryContract.md), [Headless Simulation Contract](HeadlessSimulationContract.md) |
| INV-012 | Portable-проверка существует в одном экземпляре и исполняется обоими host-ами | [Headless Simulation Contract](HeadlessSimulationContract.md), [ADR-0024](../ADR/0024-lua-spec-runner.md) |
| INV-013 | Код принадлежит C++ только если требует недоступной Lua возможности либо работает до создания VM | [Overview § Границы C++](Overview.md), [ADR-0020](../ADR/0020-cpp-scope-criterion.md) |
| INV-014 | Presentation восстановима и не является источником gameplay-истины | [Overview](Overview.md), [UI Index](../UI/README.md) |
| INV-015 | Runtime instance ссылается на definition по Stable ID и не хранит его копию | [Canonical State and Save](CanonicalStateAndSave.md) |
| INV-016 | `core` владеет механизмами, пакет — семантикой; наличие kind в ядре не означает владения его схемой | [Modding](Modding.md), [Stable ID Specification](StableIDSpecification.md), [ADR-0026](../ADR/0026-core-and-gameplay-ownership.md) |
| INV-017 | До project version 1.0 обратная совместимость между релизами не гарантируется; breaking changes остаются явными и проверяемыми | [Compatibility Policy](CompatibilityPolicy.md), [ADR-0036](../ADR/0036-pre-1-0-compatibility-policy.md) |

## Как использовать

В тесте или спеке: `-- covers INV-003`. В плане или ADR: ссылка на ID вместо пересказа правила. В review: указание нарушенного ID вместо цитаты.

Добавление инварианта в индекс не делает его нормативным — сначала он должен существовать в contract.
