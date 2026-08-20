---
title: Add Authoring Surface
status: informative
version: 1.0
updated: 2026-08-20
depends_on:
  - README.md
  - ../Architecture/AuthoringSurfaceContract.md
---

# Расширить authoring surface

> **Задача:** добавить designer-facing helper/proxy в authoring `_ENV` без второго runtime path.
> **Предмет:** declaration descriptor, adapter, package attribution, freeze и human-facing reference.
> **Нормативно:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Runtime Facade](../Architecture/RuntimeFacadeAndRegistries.md).

Повторяемость подтверждают `commands` (`Scripts/authoring/commands.lua` + `Tests/Lua/authoring/commands.lua`), `services` (`Scripts/authoring/context.lua` + `Tests/Lua/authoring/gameplay_services.lua`) и presentation helpers (`Scripts/authoring/presentation.lua` + `Tests/Lua/authoring/events_and_presentation.lua`).

## Процедура

1. Назовите конкретную designer task. Если существующий helper выражает её без infrastructure details, новый API не нужен.
2. Зафиксируйте public name, value-only вход/выход, owner registry/contract, execution scopes и typed failures в `AuthoringSurfaceContract.md`. Новый устойчивый механизм требует ADR.
3. Реализуйте helper/proxy в `Scripts/authoring/`. Proxy с `__newindex` накапливает declarations, но не создаёт альтернативный registry.
4. Подключите surface в `authoring.gameplay(...)` и public `_ENV`. Adapter канонизирует short IDs, сохраняет declaring package/module и регистрирует declarations только в `register`.
5. На общем freeze разрешите cross-package targets, проверьте duplicates/conflicts и заморозьте descriptor/proxy. Частичная регистрация не становится доступной session.
6. Оберните исполняемые functions authoring context-ом: `fail()`/`emit()` должны атрибутироваться объявившему package, scope восстанавливаться после success/refusal/fault.
7. Добавьте conformance на success, invalid input, duplicate, late declaration, missing target, immutability, attribution и validator side-effect guard.
8. В том же change set обновите inventory и пример в [Authoring docs](../Authoring/README.md).

## Гейт review

- Surface не раскрывает raw `game` infrastructure, UObject, callback или canonical tree.
- Short key даёт `<declaring_package>:<kind>.<path>`; cross-package ID является reference.
- Declaration order не становится скрытым override rule.
- Public helper отсутствует в `_ENV` до появления contract, tests и Authoring reference.
