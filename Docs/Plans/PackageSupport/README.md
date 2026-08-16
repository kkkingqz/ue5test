---
title: Package Support Implementation Plan
status: normative
version: 1.1
updated: 2026-08-16
depends_on:
  - ../../Architecture/Modding.md
  - ../../Architecture/LuaRuntimeContract.md
  - ../../Architecture/GameDataRepositoryContract.md
  - ../../Architecture/HeadlessSimulationContract.md
  - ../../Proposals/ModPackageLifecycleProposal.md
  - ../../Proposals/LuaModuleOverrideProposal.md
decisions:
  - ../../ADR/0006-repository-reload-and-session-pinning.md
  - ../../ADR/0018-portable-content-core-module.md
  - ../../ADR/0019-content-host-support-module.md
  - ../../ADR/0025-lua-module-replacement-and-export-freezing.md
---

# План реализации поддержки пакетов

> **Материализует:** [Modding](../../Architecture/Modding.md) и [Lua Runtime Contract](../../Architecture/LuaRuntimeContract.md) в рамках [ModPackageLifecycle](../../Proposals/ModPackageLifecycleProposal.md) и [LuaModuleOverride](../../Proposals/LuaModuleOverrideProposal.md).
> **Задачи:** PKG-01…23.
> **Результат:** пакет несёт контент и Lua-код, замещает модули ядра и попадает в digest и сейв.

## Цель

Сделать пакет полноценной единицей поставки: он несёт definitions, схемы, переводы, ресурсы **и Lua-код**, подключается в явном порядке, может заместить модуль ядра и получить замещённую реализацию, а его состав фиксируется в run digest и в сейве.

Это подготовка к разделению разработки на два уровня: то, что обязан поддерживать движок, остаётся в `core`, геймплей конкретной игры уезжает в отдельный пакет. Сам вынос геймплея в план не входит — см. «Границы».

## Состояние на входе

Content-сторона к нескольким пакетам готова: `BuildRepository(PackageSet, …)` принимает вектор дескрипторов, `FPackageDescriptor` несёт `package_id`, `namespace`, `load_index`, redirects и tombstones, а namespace ownership, full override, redirect-цепочки и tombstones реализованы и покрыты фикстурой `test_mod`.

Не готово всё, что выше и вокруг:

- `package_id` и namespace выводятся из имени каталога, `load_index` жёстко равен `0`. Манифеста пакета как документа нет: `package.json5` необязателен и несёт только `redirects` и `tombstones`.
- Каждый хост вызывает `DiscoverPackageFromDirectory` ровно один раз для одного корня. Набор пакетов, порядок, зависимости и lock-файл отсутствуют.
- Lua внутри пакета не обнаруживается вообще: UE берёт `FPaths::ProjectDir()/Scripts`, headless — рабочий каталог. `module_id` не резолвится по провайдерам.
- Таблицы экспорта модулей мутируемы, включая модули ядра. Проверено экспериментом: патч чужой таблицы проходит молча, единственный заслон — `require` только для объявленной зависимости.
- `FRunManifest` не фиксирует набор скриптов, поэтому замещение модуля не отразится ни в digest, ни при replay.

## Принятые решения

- **Замещение — единый механизм замены и расширения.** Пакет объявляет тот же `module_id`; не вызвал базу — замена, вызвал `require_base()` — расширение. Правило `INV-010` («override заменяет целиком») переносится на модули без исключений.
- **Мутация таблиц экспорта закрывается.** Иначе спроектированный механизм останется в стороне: патч короче и будет использоваться, а прогон перестанет быть воспроизводимым.
- **Запечатано по умолчанию.** `replaceable` ставится явно и только по конкретной потребности. Ядро (`runtime/`, `boundary/`, `bootstrap/`) не замещается никогда.
- **Хуки жизненного цикла вызываются только у победителя.** Автоматический вызов базовых хуков сделал бы полную замену невозможной.
- **Зависимости модуля — объединение по цепочке.** Базовый source исполняется и вызывает свои `require`; его зависимости обязаны быть загружены, даже если замещающий пакет о них не знает.
- **Набор скриптов входит в идентичность прогона.** `ScriptSetHash` добавляется в run manifest вместе с возможностью привезти Lua из пакета, а не после неё.
- **Манифест пакета становится обязательным.** Вывод `package_id` из имени каталога прекращается: identity пакета не может зависеть от того, как его распаковали.

## Границы

Входят: обязательный манифест пакета, обнаружение набора корней, явный порядок и lock-файл, заморозка таблиц экспорта, разметка замещаемости, обнаружение `scripts/` внутри пакета, резолюция `module_id` по провайдерам, `require_base()`, `ScriptSetHash` в run manifest и fingerprint пакетов в сейве.

Не входят:

- **Вынос геймплея в отдельный пакет.** Требует реестра обработчиков команд, доменных методов и ссылочных полей — сейчас `gameplay/root.lua` разбирает `if command_id == …`, а `boundary/ingress.lua` жёстко перечисляет обработчики. Это следующая работа, которую настоящий план делает возможной.
- Replacement session и смена набора пакетов без перезапуска: изменение enabled set по-прежнему требует полного restart.
- Cooked Pak, mod kit и подключение UE-ассетов из мода.
- UI управления модами; порядок задаётся конфигурацией.
- Sandbox для враждебного кода; trust model из [Modding](../../Architecture/Modding.md) не меняется.
- Декораторы по именам функций и любые формы патча чужих таблиц.

## Milestones

- [x] M1 — [Package Manifest](PackageManifest.md): пакет объявляет identity, версию и совместимость.
- [x] M2 — [Discovery and Order](DiscoveryAndOrder.md): набор корней, явный порядок, зависимости, lock-файл.
- [x] M3 — [Module Sealing](ModuleSealing.md): заморозка таблиц экспорта и разметка замещаемости.
- [x] M4 — [Modules from Packages](ModulesFromPackages.md): Lua из пакета, резолюция по провайдерам, `require_base()`.
- [x] M5 — [Determinism and Save](DeterminismAndSave.md): `ScriptSetHash`, fingerprint пакетов в сейве, вывод цепочек.

## Критический путь

```text
Package Manifest
→ Discovery and Order
→ Modules from Packages
→ Determinism and Save

Module Sealing
→ Modules from Packages
```

M3 не зависит от пакетов и может идти параллельно с M1 и M2. Начинать стоит именно с него: это единственный этап с риском регресса на существующем коде, и его лучше изолировать в отдельном change set.

## Общие правила выполнения

1. Filesystem-операции принадлежат `GV2ContentHostSupport`; `GV2ContentCore` остаётся без владения файловой системой ([ADR-0018](../../ADR/0018-portable-content-core-module.md), [ADR-0019](../../ADR/0019-content-host-support-module.md)).
2. Порядок перечисления файлов и каталогов не является семантикой ни на одном этапе.
3. Каждый этап оставляет ровно один reference execution path: состояние, где часть пакетов резолвится по-старому, а часть по-новому, не допускается.
4. Новый C++ обязан проходить критерий [ADR-0020](../../ADR/0020-cpp-scope-criterion.md). Резолюция провайдеров, заморозка и хэш его проходят по условию «до создания VM»; правила, выразимые в Lua, проверяются спеками ([ADR-0024](../../ADR/0024-lua-spec-runner.md)).
5. Каждая задача, меняющая failure semantics, добавляет negative case.
6. Диагностика адресуется `package_id` и package-relative путём; абсолютный путь пользователю не показывается.
7. Новое observable behavior синхронно отражается в contract; изменение инварианта требует ADR до отметки `[x]`.

## Итоговый Definition of Done

- [x] Хост собирает репозиторий из нескольких пакетов в явном порядке, повторно и детерминированно.
- [x] Мод замещает definition ядра и Lua-модуль ядра; оба пути покрыты фикстурой, исполняемой обоими хостами.
- [x] Замещающий модуль получает базу через `require_base()` и делегирует ей необработанные случаи.
- [x] Замещение запечатанного модуля и создание нового `core:*` ID отвергаются раздельными типизированными диагностиками.
- [x] Запись в таблицу экспорта после инициализации отвергается.
- [x] Одинаковый набор пакетов даёт одинаковый `ScriptSetHash` и одинаковый run digest в UE и headless.
- [x] Сейв фиксирует фактический набор пакетов и их fingerprints.
