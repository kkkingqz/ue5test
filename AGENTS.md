# GV2 Repository Instructions

Эти инструкции действуют для всего репозитория. Более близкий `AGENTS.md` или `AGENTS.override.md` может уточнить правила только для своего поддерева, но не должен молча ослаблять архитектурные инварианты из `Docs/`.

## Documentation authority

- Нормативная документация находится только в `Docs/Architecture/`, `Docs/UI/` и `Docs/ADR/`.
- `Docs/Concepts/`, `Docs/Guides/` и `Docs/Authoring/` объясняют и инструктируют, но правил не вводят: при расхождении прав contract. Это закреплено статусом `informative`, обязательным внутри этих каталогов и запрещённым снаружи.
- `Docs/Authoring/` написан для не-программиста, наполняющего игру контентом. Термины из C++, Lua-рантайма и сборки в нём не используются без объяснения; ссылки на contracts не ставятся — вместо них указывается команда проверки.
- Начальная точка для любой задачи: `Docs/README.md`.
- Архивные, экспортированные или внешние копии документации не являются источником истины.
- Accepted ADR фиксирует решение и причины; subsystem contract содержит его актуальное полное правило.
- При конфликте использовать порядок приоритета из `Docs/README.md` и исправить конфликт в рамках текущей задачи, если он относится к изменяемой области.

## How to read documentation

Перед анализом или изменением кода AI должен:

1. Прочитать `Docs/README.md`.
2. Для задачи «понять или спроектировать»: релевантный документ из `Docs/Concepts/`, затем contract затронутой подсистемы, затем связанные `accepted` ADR.
3. Для задачи «выполнить типовое изменение»: релевантный `Docs/Guides/` плюс contract, на который он ссылается, плюс активный план, если задача из него.
4. `Docs/Architecture/DependencyMap.md` — если вопрос про допустимость зависимости. `Docs/Architecture/Invariants.md` — если нужно найти нормативный источник правила по его ID.
5. Проверить соседние контракты, если изменение пересекает ownership, Stable ID, command/event, save, repository, Lua/UE boundary, lifecycle, UI или modding.

Не загружать `Docs/Architecture` целиком. Не полагаться на память, если соответствующий контракт можно прочитать из `Docs/`.

## Architecture rules that must not drift

- Stable ID имеет вид `<namespace>:<kind>.<path>` и использует strict lowercase ASCII.
- Lua владеет canonical gameplay-state.
- Код принадлежит C++ только если требует возможности, недоступной Lua по trust model, либо обязан работать до создания VM. Иначе он принадлежит Lua. Новый C++ module или сервис без ответа на этот вопрос — дефект проектирования.
- Данные пересекают boundary минимальным представлением: скаляр вместо структуры, ID вместо объекта, непрозрачные байты вместо разобранного дерева. Canonical state boundary не пересекает.
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

AI обязан обновить документацию в том же change set, если код:

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

Pure refactor без изменения behavior не требует переписывания документации, если ownership, dependency direction и public names остаются прежними. В итоговом отчёте AI должен явно указать либо обновлённые документы, либо причину, почему документация не изменилась.

Нельзя откладывать обязательное обновление документации как отдельный будущий task, если изменение уже реализовано.

## Creating new documentation

Создавать новый документ только когда информация не помещается естественно в существующий contract. Предпочитать обновление одного authoritative файла созданию дублирующего overview/specification.

### Placement

- `Docs/Architecture/` — runtime, data, lifecycle, state, repository, Lua, modding и cross-cutting contracts.
- `Docs/UI/` — UI document, semantic input, widgets, presentation snapshots/effects и UX-facing contracts.
- `Docs/Concepts/` — объяснение понятия обычным языком со ссылками на нормативный источник; новых правил не вводит.
- `Docs/Guides/` — инструкция по типовой задаче через уже существующие точки расширения; contract не меняет и его формулировки не копирует.
- `Docs/Authoring/` — инструкция для не-программиста, наполняющего игру контентом: объекты, тексты, персонажи, события. Ориентируется на файлы `GameData/` и команды `gv2-content`, а не на код.
- `Docs/Status/` — состояние реализации относительно contracts.
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
status: draft | normative | deprecated | archived | informative
version: 0.1
updated: YYYY-MM-DD
depends_on:
  - RelativeContract.md
decisions:
  - ../ADR/NNNN-decision.md
---
```

Не добавлять пустые `depends_on`/`decisions`. Dependencies должны существовать и не образовывать cycles.

Сразу после заголовка документ открывается блоком-цитатой с обязательным первым полем: `Владеет` для contracts, `Объясняет` для Concepts, `Задача` для Guides и Authoring, `Решение` для ADR, `Предлагает` для Proposals, `Материализует` для Plans, `Показывает` для Status. Header не повторяет front matter. Полная схема — `Docs/Architecture/README.md`; наличие проверяется валидатором.

`archived` означает исторический record: документ не нормативен и не является источником задач. Он допустим только внутри каталога `Archive/`, и наоборот — любой документ внутри `Archive/` обязан иметь `status: archived`.

`informative` означает объясняющий или инструктирующий документ: он не вводит архитектурных правил и при расхождении уступает contract. Допустим только внутри `Concepts/` и `Guides/`, и обязателен там.

Оба правила проверяет `Tools/Documentation/validate_docs.py` в обе стороны.

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

После создания или изменения документации AI должен:

1. Проверить links и front matter.
2. Проверить отсутствие dependency cycles.
3. Найти legacy forms и конфликтующие terms, включая `type.namespace.path`, `game.data`, gameplay `action`, cancellable `before_*` events и raw `/Game/...` paths.
4. Проверить Stable ID examples strict parser-ом или эквивалентной validation.
5. Проверить, что новый файл добавлен в `Docs/README.md` или локальный router.
6. Сопоставить examples и acceptance criteria с фактическим кодом/tests.

Если обнаружено противоречие, не добавлять compatibility alias или вторую grammar без ADR. Выбрать один canonical rule и обновить все затронутые документы.

## Code review rules

При review считать defect-ом:

- новый public behavior без обновления соответствующего contract;
- code/documentation disagreement;
- новый Stable ID или DTO, нарушающий grammar/naming;
- gameplay mutation вне command path;
- C++/Blueprint gameplay authority;
- raw UObject/callback/asset path через Lua boundary;
- новый архитектурный механизм без ADR или concrete measured need;
- новый документ, дублирующий существующий source of truth.

## Commits

После успешного выполнения задания AI обязан сделать коммит.

**Когда коммитить.** Задание выполнено целиком и относящиеся к нему проверки прошли: `python3 Tools/Documentation/validate_docs.py` для документации, `ctest` и `gv2-headless --check-scripts` для кода. Если проверки красные или задание выполнено частично — не коммитить, а сообщить, что именно осталось.

**Что попадает в коммит.** Только файлы текущего задания. Рабочее дерево может содержать чужие незакоммиченные изменения; включать их в свой коммит запрещено. Несвязанные изменения — отдельные коммиты. Один коммит на завершённое задание, а не на промежуточный шаг.

**Сообщение.** Русский язык, как и документация. Заголовок — одна строка до 72 символов, повелительное наклонение, по существу изменения: `Разделить Lua на слои движка и игры`, а не `обновить документацию` и не `правки`. Тело — если из заголовка не видно причины: зачем изменение, что оно затрагивает, ссылки на план, ADR или ID задач (`SAV-12`, `ADR-0025`, `INV-016`). Перечислять изменённые файлы в теле не нужно — их показывает сам diff.

**Чего не делать.** Не пушить без явной просьбы. Не коммитить артефакты сборки и временные файлы. Не переписывать историю (`amend`, `rebase`, `reset`) без явной просьбы.

## Unreal Editor API

- Editor-authored assets (`.uasset`), включая Widget Blueprint, Data Asset и CommonUI style assets, AI обязан создавать и изменять через настроенный Unreal Editor API (`unreal-mcp`), а не прямой записью бинарных файлов.
- Если API недоступен, AI обязан проверить, запущен ли Unreal Editor и активен ли `ModelContextProtocol`; отсутствие запущенного Editor не является основанием подменять API генерацией `.uasset` сторонними средствами.
- После изменения asset через Editor API AI обязан выполнить compile затронутых Blueprint, сохранить assets и проверить их загрузку/контракт automation-тестом либо эквивалентной Editor validation.
