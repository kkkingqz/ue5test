---
title: Add Actor Wrapper
status: informative
version: 1.5
updated: 2026-08-20
depends_on:
  - README.md
---

# Добавить доменные методы сущности

> **Задача:** дать категории сущностей доменные методы вместо ручной правки таблиц состояния.
> **Предмет:** authoring API доменных методов Runtime Instance.
> **Нужно:** понимание разницы definition / экземпляр / обёртка — [RuntimeInstances](../Concepts/RuntimeInstances.md).
> **Нормативно:** [Authoring Surface](../Architecture/AuthoringSurfaceContract.md), [Canonical State and Save](../Architecture/CanonicalStateAndSave.md), [Commands and Events](../Architecture/CommandsAndEvents.md).

## Шаги

**1. Объявить поля состояния.** Структурные ограничения принадлежат `field.*`, а не повторяются в каждом методе.

```lua
Actor.gold = field.non_negative_integer()
```

**2. Объявить методы на управляемом прототипе.** Authoring-скрипт находится в `<package>/scripts/authoring/`; `_ENV` уже содержит `Actor`, `field` и `fail`.

```lua
function Actor:require_gold(amount)
    if self.gold < amount then
        fail("economy.insufficient_gold", {
            available = self.gold,
            required = amount,
        })
    end
end

function Actor:add_gold(amount)
    assert(type(amount) == "number" and amount > 0)
    self.gold = self.gold + amount
end
```

Метод получает текущую disposable wrapper как `self`. Возвращать result envelope не нужно; отсутствие `return` означает успех команды.

**3. Разделить ответственность.** Метод принадлежит одной сущности. Процесс над несколькими равноправными сущностями оформляется через `services.<name> = { ... }`.

**4. Добавить спеку.** Проверить успешный вызов, типизированный отказ до мутации, field invariant и конфликт повторного объявления метода.

## Типичные ошибки

**Мутирующие методы в реестре.** `registry.add_gold(id, 20)` превращает реестр в manager. Правильно — `registry.get(id):add_gold(20)`.

**Изменение без команды.** Field и метод не открывают mutation window; запись допустима только из Command path.

**Молчаливое переопределение метода.** Дубликаты отклоняются; порядок пакетов не является override-механизмом.
