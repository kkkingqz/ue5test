---
title: Glossary and Naming
status: normative
version: 1.7
updated: 2026-08-12
depends_on:
  - StableIDSpecification.md
---

# Glossary and Naming

Один архитектурный смысл имеет одно каноническое имя. Английские термины и identifiers канонические; пояснения документа — на русском.

## Данные и состояние

| Термин | Значение |
|---|---|
| Definition | Статическое schema-validated описание из repository |
| Resolved definition | Победившая immutable definition после override и validation |
| GameDataRepository | Published immutable snapshot definitions, schemas и provenance |
| Canonical gameplay-state | Единственное сохраняемое mutable состояние прохождения, owner — Lua |
| Runtime instance | Сохраняемый экземпляр с `type_id`, `definition_id`, `instance_id` и instance state |
| Runtime object | Любой transient объект Lua runtime; может не сохраняться |
| Presentation-state | Локальное состояние UE/UX, не определяющее gameplay |
| Presentation projection | Widgets, Actors, streams и audio/visual representation desired state |

Термин `prototype` не используется как синоним class, definition или instance.

## Commands, input и events

| Термин | Значение |
|---|---|
| Semantic input | Value-only UE → Lua сообщение о gameplay-значимом пользовательском input |
| Command | Проверяемое намерение изменить canonical gameplay-state |
| CommandRequest | Value-only envelope `command_id + args + sequence`, подаваемый в Command Dispatcher |
| Command Dispatcher | Единственная публичная точка запуска gameplay commands |
| Command validator | Ordered, side-effect-free проверка до mutation; может отклонить command |
| Gameplay event | Неотменяемый post-commit факт внутри Lua runtime |
| Technical input | Результат UE/platform operation, доставленный через runtime ingress queue |
| Handler | Реализация command/event/lifecycle contract |
| Hook | Документированная lifecycle или extension point |
| Callback | Внутренняя функция реализации; не пересекает C++/Lua boundary |
| Observation | Read-only value-only snapshot, который simulation agent может использовать для выбора следующей команды |
| Simulation Driver | Внешний к gameplay VM headless orchestrator: Observation → CommandRequest |

`before event` не является частью v1. Предусловия и mod veto реализуются command validators. EventBus никогда не используется для запроса разрешения.

## UI и presentation

| Термин | Значение |
|---|---|
| Location | Gameplay-сущность текущего местоположения |
| Screen | Визуальный режим location или другого UI route |
| Screen Template | UE-authored Widget Blueprint layout конкретного Screen, унаследованный от общего base |
| Screen Registry | UE presentation mapping `screen_id` в trusted Screen Template class и layer policy |
| Screen Field | Schema-identified value-only dynamic input одного Screen Template |
| Dynamic Screen Element | Reusable Widget adapter, объявляющий и применяющий один Screen Field |
| Leaf Adapter | Единственный approved владелец mutation конкретного runtime content primitive |
| Composite Widget | Widget, который составляет UI из adapters и не вводит собственный presentation path |
| Route | Основной навигационный узел с lifecycle identity |
| UI-document | Полная declarative desired model Screen instances по UI layers |
| UI component | Reusable semantic presentation element type; не physical Widget и не root Screen Template |
| Widget | Физический UMG runtime object |
| Game Shell | Постоянная корневая оболочка UI layers |
| Overlay | Временный неблокирующий слой |
| Modal | Слой, блокирующий нижележащий interactive route |
| Presentation snapshot | Полное восстанавливаемое desired presentation |
| Presentation effect | One-shot визуальное/звуковое намерение, не используемое для restore |
| UI binding handle | Opaque transient handle current UI revision; связывает physical Widget с validated command binding |
| Resource ID | Stable logical presentation resource; не UE asset path |
| TextSpec | Value-only `text_id + args + optional style` до localization/render resolution |
| Text style token | Theme-local semantic typography name; не Stable ID и не UE asset locator |
| Image Resource Catalog | Startup-built UE-side mapping filesystem-derived image `resource_id` в runtime texture и canonical render metadata |

## Stable ID categories

| Поле | Пример |
|---|---|
| `definition_id` | `core:item.weapon.iron_sword` |
| `command_id` | `core:command.location.travel` |
| `event_id` | `core:event.location.enter` |
| `module_id` | `core:module.location_service` |
| `schema_id` | `core:schema.definition.item.v1` |
| `widget_id` | `core:widget.button_list` |
| `screen_id` | `core:screen.main` |
| `text_id` | `core:text.location.market.title` |
| `resource_id` | `core:resource.character.aria.casual` |
| `operation_id` (kind) | `core:operation.resource.prepare` |
| `validator_id` | `core:validator.item.semantics` |
| `error_id` | `core:error.location.locked` |
| `diagnostic_id` | `core:diagnostic.repository.unknown_id` |
| `slot_id` | `core:slot.location.main` |

## Lua и JSON5

| Элемент | Стиль | Пример |
|---|---|---|
| Class/type | PascalCase | `Companion` |
| Function/method | snake_case | `add_gold` |
| Variable/field | snake_case | `definition_id` |
| Constant | UPPER_SNAKE_CASE | `MAX_PARTY_SIZE` |
| Boolean | `is_`, `has_`, `can_`, `should_` | `can_travel` |
| Filename/directory | snake_case | `location_service.lua` |

Reference fields оканчиваются на `_id`, collections — на `_ids`. Публичные schemas не используют неоднозначные `data`, `info`, `obj`, `tmp` без конкретного контекста; стандартное поле definition envelope `data` является осознанным исключением.

## C++ и Unreal

C++ следует Unreal naming: `U`/`A`/`F`/`E` prefixes, PascalCase functions/fields, `b` для bool. Public wire fields остаются snake_case и преобразуются adapter-ом.

Assets используют Unreal prefixes (`WBP_`, `BP_`, `T_`, `M_`, `SFX_`, `MUS_`), но asset name/path не является Stable ID. Связь задаётся explicit resource mapping.

## Запрещённые неоднозначности

| Не использовать | Использовать |
|---|---|
| gameplay action | command |
| before event | command validator |
| event как запрос | command или technical input |
| callback как public API | hook, event subscription или operation result |
| screen как синоним location | location / screen |
| widget как синоним component | UI component / Widget |
| Blueprint path как screen identity | screen_id / Screen Registry |
| asset path в Lua gameplay data | resource_id |
| localization_key / text_key | text_id |
| game data repository | GameDataRepository |
