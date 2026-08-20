# GV2 Repository Instructions for Gemini

Эти инструкции действуют для всего репозитория. Более близкий `GEMINI.md` или `GEMINI.override.md` может уточнить правила только для своего поддерева, но не должен молча ослаблять архитектурные инварианты из `Docs/`.

## Documentation authority

- Нормативная документация находится только в `Docs/**/*.md`.
- Начальная точка для любой задачи: `Docs/README.md`.
- Архивные, экспортированные или внешние копии документации не являются источником истины.
- Accepted ADR фиксирует решение и причины; subsystem contract содержит его актуальное полное правило.
- При конфликте использовать порядок приоритета из `Docs/README.md` и исправить конфликт в рамках текущей задачи, если он относится к изменяемой области.

## How to read documentation

Перед анализом или изменением кода Gemini должен:

1. Прочитать `Docs/README.md`.
2. Прочитать `Docs/Architecture/Overview.md` и `Docs/Architecture/GlossaryAndNaming.md`, если они ещё не прочитаны в текущей задаче.
3. По карте документов выбрать только контракты затронутых подсистем.
4. Прочитать связанные `accepted` ADR, перечисленные в front matter или `Docs/ADR/README.md`.
5. Проверить соседние контракты, если изменение пересекает ownership, Stable ID, command/event, save, repository, Lua/UE boundary, lifecycle, UI или modding.

Не загружать все документы без необходимости. Не полагаться на память, если соответствующий контракт можно прочитать из `Docs/`.

## Architecture rules that must not drift

- Stable ID имеет вид `<namespace>:<kind>.<path>` и использует strict lowercase ASCII.
- Lua владеет canonical gameplay-state.
- Gameplay mutation проходит через Command Dispatcher и Gameplay Services.
- EventBus публикует только post-commit gameplay facts.
- C++/Lua boundary value-only; C++ не хранит Lua callbacks.
- Active session использует pinned immutable repository snapshot; reload применяется через controlled session restart.
- UI является reconstructable desired presentation и отправляет bound `command_id`, а не Lua function/callback name.
- Runtime text, content images, repeated elements, Screen Fields и Semantic Input используют только централизованные presentation paths; composite Widgets не создают parallel mechanisms.
- Definitions используют full override by ID; implicit deep merge отсутствует.
- Опубликованный Stable ID не переиспользуется для другого смысла.

Изменение любого из этих правил требует нового ADR и синхронного обновления затронутых контрактов.

## Documentation is part of every code change

Добавление или изменение кода считается завершённым только после проверки и синхронизации документации.

Gemini обязан обновить документацию в том же change set, если код:

- добавляет subsystem, service, module, registry, lifecycle phase или dependency direction;
- меняет observable behavior, public API, DTO, schema, Stable ID category или error semantics;
- добавляет/меняет command, validator, event, technical input или gameplay-state invariant;
- меняет bootstrap, session lifecycle, async operation, save/load, migration или recovery;
- меняет repository build/publication, content envelope, schema или override behavior;
- добавляет UI document field, widget contract, semantic input, snapshot/effect или resource rule;
- меняет mod package/API, trust boundary, extension point или compatibility policy.

Для нового кода:

1. Найти owner contract в `Docs/README.md`.
2. Обновить существующий contract либо создать новый, если ответственность действительно новая.
3. Добавить новый документ в соответствующий index/router.
4. Создать ADR, если появилось архитектурное решение или изменился один из устойчивых инвариантов.
5. Добавить/обновить examples, error codes, lifecycle sequence и acceptance criteria, затронутые кодом.

Pure refactor без изменения behavior не требует переписывания документации, если ownership, dependency direction и public names остаются прежними. В итоговом отчёте Gemini должен явно указать либо обновлённые документы, либо причину, почему документация не изменилась.

Нельзя откладывать обязательное обновление документации как отдельный будущий task, если изменение уже реализовано.

## Creating new documentation

Создавать новый документ только когда информация не помещается естественно в существующий contract. Предпочитать обновление одного authoritative файла созданию дублирующего overview/specification.

### Placement

- `Docs/Architecture/` — runtime, data, lifecycle, state, repository, Lua, modding и cross-cutting contracts.
- `Docs/UI/` — UI document, semantic input, widgets, presentation snapshots/effects и UX-facing contracts.
- `Docs/ADR/` — принятые или предложенные архитектурные решения.
- `Docs/README.md` — repository-wide router; не превращать его в полный contract.
- `README.md` внутри тематического каталога — только локальный index/router.

Новый тематический каталог создаётся только при наличии минимум двух самостоятельных документов и понятного owner/scope.

### Standard filenames

- Architecture/UI contract: English PascalCase без пробелов, например `SaveContainerContract.md`, `AssetLoadingPolicy.md`, `LocationScreenContract.md`.
- Общий обзор каталога: `README.md`.
- ADR: `NNNN-short-kebab-case-title.md`, например `0010-save-container-format.md`.
- Не использовать `final`, `new`, `copy`, даты, языковые суффиксы, версии в filename или скобки вроде `(1)`.
- Один concept — один canonical filename. При rename обновить все relative links в том же change set.

### Required front matter

Каждый contract начинается с:

```yaml
---
title: Human Readable Title
status: draft | normative | deprecated
version: 0.1
updated: YYYY-MM-DD
depends_on:
  - RelativeContract.md
decisions:
  - ../ADR/NNNN-decision.md
---
```

Не добавлять пустые `depends_on`/`decisions`. Dependencies должны существовать и не образовывать cycles.

ADR использует:

```yaml
---
title: "ADR-NNNN: Human Readable Title"
status: proposed | accepted | superseded | rejected
date: YYYY-MM-DD
---
```

### Required contract structure

Выбирать только применимые разделы, сохраняя следующий порядок:

1. Purpose/scope.
2. Ownership and source of truth.
3. Invariants.
4. Data model/API/envelopes.
5. Lifecycle or processing flow.
6. Failure and recovery semantics.
7. Compatibility/evolution rules.
8. Examples.
9. Verification/acceptance criteria.

Документ должен отвечать на вопросы: кто владеет данными, когда они создаются/уничтожаются, что пересекает boundary, что происходит при ошибке и как проверить реализацию.

## Documentation style

- Основной язык объяснений — русский; canonical identifiers и технические термины — английские согласно `GlossaryAndNaming.md`.
- Использовать короткие нормативные формулировки: «обязан», «запрещено», «может».
- Не дублировать целые разделы другого contract; давать relative link и фиксировать только локальную семантику.
- Примеры являются contract fixtures и обязаны соответствовать текущим grammar/schemas.
- Stable ID examples используют `<namespace>:<kind>.<path>`.
- Lua/JSON5 fields — `snake_case`; C++ identifiers — Unreal style.
- Не использовать raw UE asset paths в gameplay/Lua data; использовать `resource_id`.
- Не вводить синонимы для Command, Event, Definition, Runtime Instance, UI Component и Widget.
- Mermaid/table использовать только когда они упрощают flow, state machine, ownership или repeated mappings.
- Все внутренние links — relative Markdown links.

## Standardization and validation

После создания или изменения документации Gemini должен:

1. Проверить links и front matter.
2. Проверить отсутствие dependency cycles.
3. Найти legacy forms и конфликтующие terms, включая `type.namespace.path`, `game.data`, gameplay `action`, cancellable `before_*` events и raw `/Game/...` paths.
4. Проверить Stable ID examples strict parser-ом или эквивалентной validation.
5. Проверить, что новый файл добавлен в `Docs/README.md` или локальный router.
6. Сопоставить examples и acceptance criteria с фактическим кодом/tests.

Если обнаружено противоречие, не добавлять compatibility alias или вторую grammar без ADR. Выбрать один canonical rule и обновить все затронутые документы.

## Task integrity and reporting

- Запрещено отмечать задачу или пункт плана выполненным (`[x]`), если реально работа не выполнена или не проверена.
- При обнаружении недостоверных отметок, нереализованных требований или незакрытых пробелов Gemini обязан остановиться, не ставить ложную отметку о готовности и дать пользователю честное сообщение о проблеме с точным описанием того, что осталось сделать.

## Code review rules

При review считать defect-ом:

- новый public behavior без обновления соответствующего contract;
- code/documentation disagreement;
- ложная отметка о выполнении задачи или пункта плана при отсутствии реальной реализации/проверки;
- новый Stable ID или DTO, нарушающий grammar/naming;
- gameplay mutation вне command path;
- C++/Blueprint gameplay authority;
- raw UObject/callback/asset path через Lua boundary;
- новый архитектурный механизм без ADR или concrete measured need;
- новый документ, дублирующий существующий source of truth.

## Unreal Editor API

- Editor-authored assets (`.uasset`), включая Widget Blueprint, Data Asset и CommonUI style assets, Gemini обязан создавать и изменять через настроенный Unreal Editor API (`unreal-mcp`), а не прямой записью бинарных файлов.
- Если API недоступен, Gemini обязан проверить, запущен ли Unreal Editor и активен ли `ModelContextProtocol`; отсутствие запущенного Editor не является основанием подменять API генерацией `.uasset` сторонними средствами.
- После изменения asset через Editor API Gemini обязан выполнить compile затронутых Blueprint, сохранить assets и проверить их загрузку/контракт automation-тестом либо эквивалентной Editor validation.
