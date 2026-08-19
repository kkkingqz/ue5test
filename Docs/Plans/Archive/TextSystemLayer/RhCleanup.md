---
title: RH Cleanup Tasks
status: archived
version: 1.0
updated: 2026-08-19
depends_on:
  - LocationOwnership.md
---

# M5 — RH Cleanup

> **Материализует:** трёхуровневую границу в коде игры.
> **Задачи:** TSL-18…20.
> **Результат:** в `rh` остаются только правила и сущности игры.

## Результат этапа

`rh/actors.lua` перестаёт быть местом, где живёт половина текстового движка. Методы управления ресурсами (золото, выносливость) генерируются таблично без дублирования. Документация репозитория полностью синхронизирована с трёхуровневой архитектурой.

## Задачи

- [x] **TSL-18 — Убрать из `actors.lua` ответственность `textsystem`**
  - Зависимости: TSL-10.
  - Done: код локации, перехода, связности и регистрации ссылочного поля локации удалён — он живёт в `textsystem`; остаются золото, выносливость, `add_item` и регистрации игры; ни одна спека не обращается к удалённым методам через `rh`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`, `GameData/textsystem/scripts/gameplay/actors.lua`, спеки `Tests/Lua/economy/actor_rh_economy.lua`, `Tests/Lua/economy/location_actions.lua`.

- [x] **TSL-19 — Свести дублирование ресурсов**
  - Зависимости: TSL-18.
  - Золото и выносливость сегодня реализованы двумя почти одинаковыми наборами методов.
  - Done: проверка суммы существует в одном экземпляре (`validate_amount`); описание ресурса — поле состояния, код отказа, имена параметров — задаётся таблицей `RESOURCES`, а методы (`get_*`, `require_*`, `spend_*`, `add_*`) порождаются из неё генератором `attach_resource_methods`; поведение и коды отказов не изменились, существующие спеки проходят с прежними ожиданиями; добавление третьего ресурса требует лишь добавления записи в `RESOURCES`.
  - Evidence: `GameData/rh/scripts/gameplay/actors.lua`, `Tests/Lua/economy/actor_rh_economy.lua`, `Tests/Lua/economy/location_actions.lua`.

- [x] **TSL-20 — Синхронизировать документацию**
  - Зависимости: TSL-14, TSL-17, TSL-19.
  - Решение по геттерам/хелперам (`get_gold`, `get_stamina`, `is_player`, `is_npc`): методы сохранены на декораторе как удобные явные хелперы предметной модели, покрыты спеками `Tests/Lua/economy/actor_rh_economy.lua`.
  - Done: [Modding](../../../Architecture/Modding.md) описывает три уровня и критерий выбора; [Stable ID Specification](../../../Architecture/StableIDSpecification.md) — kind `action` и namespace `textsystem`; [Build and Tooling](../../../Architecture/BuildAndTooling.md) — набор пакетов из данных и контейнерный режим; [Concepts](../../../Concepts/README.md) и [ContentModel](../../../Concepts/ContentModel.md) объясняют три слоя читателю; [Implementation Status](../../../Status/ImplementationStatus.md) обновлён; в предложении [TextSystemLayerProposal](../../../Proposals/Archive/TextSystemLayerProposal.md) проставлено `proposal_state: implemented`.
  - Evidence: `Docs/Architecture/Modding.md`, `Docs/Architecture/StableIDSpecification.md`, `Docs/Architecture/BuildAndTooling.md`, `Docs/Concepts/ContentModel.md`, `Docs/Status/ImplementationStatus.md`, `Docs/Proposals/Archive/TextSystemLayerProposal.md`.

## Проверка milestone

- [x] `rh/actors.lua` содержит только золото, выносливость, предметы и регистрации.
- [x] Добавление третьего ресурса не требует копирования блока кода.
- [x] Судьба неиспользуемых геттеров решена явно, а не оставлена как есть.
- [x] Документация описывает три слоя одинаково в contracts и Concepts.
