---
title: Cpp Foundation Closure Implementation Plan
status: active
version: 1.7
updated: 2026-09-13
depends_on:
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../Architecture/CanonicalStateAndSave.md
  - ../../Architecture/BuildAndTooling.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0021-opaque-save-container.md
  - ../../ADR/0043-presentation-apply-boundary.md
  - ../../ADR/0044-session-replacement-and-registry-sealing.md
  - ../../ADR/0045-atomic-save-slot-generation-publication.md
---

# C++ Foundation Closure: план реализации

> **Материализует:** принятые границы ownership, session publication и opaque save в работающей UE composition и проверяемой процедуре приёмки. Основание — [текущий аудит](../../Status/AuditFindings.md) и accepted ADR выше.
> **Не является нормативным:** проектирование ниже описывает рекомендуемую реализацию существующих правил. Новые API, failure semantics и решения сначала фиксируются задачей CFC-01 в owner contracts; план не подменяет ADR.
> **Исполнение:** использовать `superpowers:executing-plans`, последовательно по задачам. Checkbox задачи — единственный источник её завершения.

**Цель:** получить ограниченную и проверенную C++-основу, на которой gameplay-срез со стартом, командами, UI, сохранением, загрузкой и продолжением игры развивается в Lua без изменения native gameplay-логики.

**Технологии:** существующие UE 5.8 / C++ modules, portable CMake targets, Lua 5.4.8, UE Automation, Python tooling, unreal-mcp для Editor/asset acceptance. Нового runtime module план не требует.

## Почему предыдущие исправления не остановили повторение

Последний план дал полезные механизмы: exact package set, snapshot и отдельный `GV2PresentationApply`. Аудит обнаружил обходы этих механизмов: materializer читает другой кэш; ambient getter выбирает старый snapshot; UE-host разрушает проекцию раньше решения coordinator. Аналогично portable save существует без product callers, а зелёный runner допускает неисполненные тесты. Следовательно, следующие изменения должны удалить обходные пути и связать lifecycle владельцев, а не только усилить проверки уже работающих helpers.

| Подход | Результат и цена | Выбор |
|---|---|---|
| Исправить getter, проверить один `lua_pcall`, расширить regex | Быстро закрывает известные входы; оставляет две schema authorities, два владельца teardown и неполный actual inventory | Недостаточен |
| Завершить существующие ownership boundaries и сделать их обязательными в production | Удаляет второй источник, вводит один протокол replacement и fail-closed приёмку; сохраняет модули и Lua-owned state | Основа плана |
| Переписать runtime/framework или перенести правила игры в C++ | Большая новая поверхность без evidence необходимости; нарушает критерий C++ scope | Не входит в план |

## Рекомендуемое устройство

### Один snapshot и обязательный контекст

`FGV2SessionCoordinator` остаётся единственным владельцем active/candidate session. `FGV2SessionContentSnapshot` владеет всеми presentation authorities. Подготовка initial document получает candidate явно; последующая подготовка получает published snapshot той же generation. `GetContentSnapshotForPrepare()` с выбором из двух источников удаляется. Schema lookup требует ссылку на `FGV2PresentationPrepareContext`; global cache, session cache rebuild и fallback discovery удаляются. `GV2PresentationApply` получает только уже разрешённую транзакцию.

Владение snapshot включает независимость достижимого runtime state: authoring `UGV2ScreenRegistry` — read-only input для compile, а resolved rows/class ownership/placement policy — private value конкретного snapshot. Candidate B не вызывает mutating Build на объекте, которым разрешает экраны A. CFC-04A закрывает этот отдельный пробел внешнего review (SNAP-R1 / SNAP-AF-01); перенос публикации или shallow const wrapper его не устраняют.

### Две границы replacement с разной семантикой отказа

До **commit-to-replace** существует прежняя Ready-сессия A. Можно построить native content candidate B, прочитать opaque save bytes и выполнить read-only Lua preflight в A. Ошибка/отмена здесь сохраняет VM, generation, snapshot, bindings и реально работающий UI A.

После **commit-to-replace** input A закрывается, её bindings инвалидируются, UE projection уничтожается по команде coordinator, VM A полностью освобождается. Затем создаётся единственная VM B. Начальная проекция B готовится приватно из B и применяется. Только успешный initial Commit разрешает **publish-Ready**: snapshot, bindings, projection и статус B становятся согласованно доступны. Ошибка после разрушения A ведёт в native recovery; обещания отката к уничтоженной VM нет.

```text
Ready A → native candidate + Lua preflight A
                 ├─ отказ/отмена → прежняя Ready A с прежним UI
                 └─ commit-to-replace → teardown A → VM B → initial Prepare/Commit B
                                                            ├─ отказ → recovery
                                                            └─ publish-Ready B
```

`UGV2RuntimeSubsystem` остаётся UE adapter: создаёт/удаляет UObject projection только в фазе, которую разрешил coordinator, и берёт GameShell class из переданного snapshot. Он не принимает независимое решение о начале teardown. Ни side-by-side Lua VM, ни сохранение canonical state в C++ для rollback не вводятся.

### Fail-closed lifecycle без второго gameplay framework

У freeze gate один проверяемый результат. При missing registry, исключении, неверном return type или `is_frozen() != true` state build не начинается. Рекомендуется private Lua bootstrap descriptor: он одновременно определяет создание/установку engine registries и порядок их sealing. Он закрыт для mod registration и не является generic registry-of-everything; semantic entries и policies остаются в конкретных registries. C++ вызывает один фиксированный protected lifecycle entry point и получает typed fault. Изменение способа bootstrap фиксируется ADR задачей CFC-01 до реализации.

### Opaque storage и продуктовый save/load

До persistence замыкается переносимая основа: canonical Number нормализуется на construction boundary, manifest/digest используют один hash-domain validator. Сборка temporary canonical state, merge/collision/mod policies и PRNG принадлежат Lua; host передаёт seed отдельно от generation до bootstrap. Эти направления описаны в [Portable Correctness](PortableCorrectness.md). GC ownership prepared UI и scoped test fixtures проверяются до replacement acceptance, чтобы протекающие roots не скрывали неудерживаемых кандидатов.

Application host владеет одним slot storage; каждая session получает его до старта. Lua-authored кнопки save/load по-прежнему отправляют bound `command_id`: core Lua handlers ставят typed host control request через фиксированную bridge capability. Native host не узнаёт имена команд. Запрос исполняется после успешного завершения вызывающей команды и выхода из Lua; save дополнительно ждёт safe point с пустыми command/event queues. Неуспешная команда не выпускает отложенный control request. Load использует тот же replacement protocol. Lua сериализует, проверяет integrity, мигрирует и назначает state; C++ переносит только bytes, slot ID, generation и outcome.

Preflight и replacement используют один захваченный immutable byte buffer: после preflight повторного чтения изменяемого slot не происходит. C++ сохраняет current и previous committed bytes; пригодность любого поколения определяет Lua. Recovery previous revision — явный load request, без скрытого выбора «валидного» дерева хостом.

Физические поколения slot неизменяемы; один маленький storage-owned head содержит ссылки на Current/Previous. Запись публикует новый head одним atomic rename, поэтому ошибка между подготовкой backup и current не может заменить Previous дубликатом Current. Head — native metadata размещения файлов, не разобранный save container; правила gameplay encoding остаются только в Lua.

### Один критерий успешной приёмки

Local MCP и fresh-process CI используют один report validator. Actual test set происходит из UE discovery, фактическое исполнение — из records текущего запуска. Успех требует равенства множеств, уникальности имён, terminal success каждого record, согласованных totals, отсутствия errors и совпадения build identity. Значения `104` и `141` из аудита — история, не константы приёмки. Для module boundary применяется fail-closed разбор всего разрешённого Build.cs языка плюс проверка реальной сборкой; неизвестная конструкция не означает пустой список зависимостей.

## Объём фиксации

План включает 19 задач: CFC-01…13 и CFC-02A, CFC-03A, CFC-04A/04B, CFC-05A, CFC-07A. Он закрывает подтверждённые findings текущего аудита, включая SNAP-R1 и проверенные REVIEW-01…15, `STATUS-001`, `STATUS-013…025` и scene-presence gap `STATUS-011`. Отклонённые или суженные утверждения review имеют явный исход в аудите и не превращаются в лишние архитектурные задачи. Это один связанный путь: приёмка → lifecycle/authority → persistence → сквозной gameplay.

Первая фиксируемая поверхность: **Linux Development, UE game host и headless**, synchronous desired presentation, текущие centralized text/image/fields/input paths, команды/services/events, new/menu/restart/load/reload/shutdown, opaque slots и Lua authoring. Shipping/cook/package и другие ОС не считаются проверенными этим baseline. One-shot effects (`STATUS-002`) и enter/exit animations (`STATUS-003`) остаются открытыми и вне первой обещанной gameplay-поверхности. Они не блокируют синхронный gameplay-срез, но запрещено объявлять весь исходный contract полностью реализованным.

Это фиксация поддерживаемого API и evidence, не обещание отсутствия любых дефектов или запрет исправлять C++ впоследствии. Расширение native capability требует обоснования по [When to Write Cpp](../../Guides/WhenToWriteCpp.md) и новой приёмки затронутой границы.

## Этапы и зависимости

| Milestone | Задачи | Проверяемый результат |
|---|---|---|
| M0 — Достоверная приёмка | CFC-01…03, CFC-02A/03A; [Acceptance](Acceptance.md), [Portable Correctness](PortableCorrectness.md) | Строгий runner, изоляция fixtures, dependency gate и canonical value/codecs |
| M1 — Session ownership | CFC-04…07, CFC-04A/04B, CFC-05A/07A; [Session Lifecycle](SessionLifecycle.md), [Portable Correctness](PortableCorrectness.md) | Изолированные authorities, GC lifetime, Lua-owned state, seed input и replacement |
| M2 — Save/load в игре | CFC-08…10, [Save and Gameplay](SaveAndGameplay.md) | Previous copy, safe save, active-session preflight и product load |
| M3 — Lua baseline | CFC-11…13, [Save and Gameplay](SaveAndGameplay.md) | Обязательность сцены, сквозной сценарий и зафиксированная поддержанная поверхность |

Зависимости: `01 → 02 → 02A → 03 → 03A → 04 → 04A → 04B → 05 → 05A → 06 → 07 → 07A → 08 → 09 → 10 → 11 → 12 → 13`. Порядок намеренно последовательный: следующая приёмка использует уже исправленный runner, а save/load использует уже испытанный replacement. Реализацию одного этапа можно ревьюить и отклонять независимо от следующего; массовое переписывание всех surfaces одним commit не требуется.

- [x] M0 — CFC-01…03 и CFC-02A/03A приняты по Done/Evidence. (перепроверено 2026-09-13 на текущей ревизии)
- [x] M1 — CFC-04…07 и CFC-04A/04B, CFC-05A/07A приняты по Done/Evidence. (2026-09-13)
- [x] M2 — CFC-08…10 приняты по Done/Evidence. (принято ревью 2026-09-14)
- [ ] M3 — CFC-11…13 приняты по Done/Evidence.

| Находка / gap | Закрывающие задачи |
|---|---|
| VERIFY-AF-01, VERIFY-AF-02 | CFC-02, финальная сверка CFC-13 |
| PSC-AF-06 / STATUS-016 | CFC-03 |
| PSC-AF-03 / STATUS-013 | CFC-04 |
| SNAP-AF-01 (внешнее SNAP-R1) / STATUS-020 | CFC-04A; верхний replacement path — CFC-06 |
| RUNTIME-AF-01 / STATUS-017 | CFC-05 |
| PSC-AF-04 / STATUS-014 | CFC-06 |
| PSC-AF-05 / STATUS-015 | CFC-06 и CFC-07 |
| STATUS-001 | CFC-07 и CFC-10; строка не удаляется после частичного new/restart |
| SAV-AF-02 / STATUS-019 | CFC-08 |
| SAV-AF-01 / STATUS-018 | CFC-09 и CFC-10 |
| STATUS-011 | CFC-11 |
| REVIEW-01 / CFC-AF-01, STATUS-021 (registry classes) | CFC-04A; не возвращать runtime map на authoring DataAsset |
| REVIEW-02/10 / CFC-AF-02/10 (test hygiene) | CFC-02A |
| REVIEW-03 / CFC-AF-03, STATUS-021 (UI candidates) | CFC-04B |
| REVIEW-04 / CFC-AF-04, STATUS-023 | CFC-07A; seed не является generation |
| REVIEW-05 / CFC-AF-05, STATUS-022 | CFC-05A; composition целиком Lua-owned |
| REVIEW-06/07 / CFC-AF-06/07, STATUS-024/025 | CFC-03A; NaN-утверждение отдельно отклонено |
| REVIEW-08 / CFC-AF-08 | Misuse guard в CFC-04B; production off-thread defect не подтверждён |
| REVIEW-09 / CFC-AF-09 | Уже покрыт single-writer/atomic publication CFC-08 |
| REVIEW-11…14 / CFC-AF-11…14 | Отклонены как самостоятельные correctness/performance blockers; условия повторного открытия в аудите |
| REVIEW-15 / CFC-AF-15 | Сопутствующее форматирование участка CFC-05A |

## Приёмка M0 (перепроверка 2026-09-13)

Галочка M0 была поставлена на коммите `6d2ac6a`, после чего собственные поставки этапа переписали: `aa0eb7e` заменил `ue_test_report.py`, `test_ue_test_report.py`, `validate_test_fixture_ownership.py`, `run_ue_acceptance.py`, CI-шаг и добавил машинерию build identity, а `cdb4217` дотащил остаток той же работы. То есть принятое состояние перестало существовать вскоре после приёмки. Перепроверка перепривязывает M0 к тому коду, который лежит в дереве сейчас.

**Центральная поставка проверена тем, что отказала.** Первый прогон `Tools/Testing/run_ue_acceptance.py` дал 158/158 пройденных тестов и при этом вернул ненулевой exit:

```text
VALIDATION FAILED (FAIL-CLOSED):
  - Run identity mismatch for 'source_revision': expected 'b163e03…', got '0bbb5fb…'
  - Run identity mismatch for 'source_diff_hash': expected 'clean', got 'b41e006…'
```

Загруженный бинарник был собран из более раннего и грязного дерева, и раннер не засчитал зелёное. После пересборки — `SUCCESS: All 158/158 discovered tests passed validation`. Это и есть смысл `VERIFY-AF-01/02`: «проверен GV2» означает исполненный актуальный набор на сверенных бинарниках, а не маркер в логе.

Связь evidence с бинарником не подстраивается: `GV2BuildIdentity.gen.h` генерируется `PreBuildSteps` обоих `Target.cs`, константа компилируется в модуль, тест `GV2.Runtime.ModuleIdentity` печатает её из загруженного кода, ожидаемая сторона считается из рабочего дерева, а раннер передаёт нормализатору только `run_id` — сама идентичность извлекается из рантайма и при её отсутствии валидатор краснеет.

**Проверено мутациями, а не чтением.** `validate_run` выполняет все четыре утверждения «Проверки алгоритма» плюс дубликаты имён, пустую идентичность, `Success` с warnings и `Success` с errors. Ограниченный парсер `Build.cs` отверг весь корпус: `AddRange(new[] { … })` — исходный обход аудита, — одиночный `Add`, условную зависимость, helper, алиас через переменную и `PublicIncludePathModuleNames`. Гейт fixture ownership краснеет на сыром `AddToRoot`, непарном `RemoveFromRoot` и на доступе к `GForgeryModeForNextInstance`, включая алиас через ссылку; сам сеттер режима закрыт `private` + единственным `friend`, то есть первично компилятором. Откат нормализации нуля в `FValue` красит шесть портируемых тестов; подмена вызова общего hash-валидатора в codec немедленно красит inventory-гейт.

**Открытый пункт evidence.** Evidence `CFC-02` требует отдельно указать фактический удалённый CI run: «Изменение YAML не доказывает успешность удалённого CI». Workflow настроен правильно и вызывает тот же раннер с тем же валидатором, но ссылки на прогон на `origin` в записях нет. Пункт остаётся открытым до её появления; он не блокирует M0, потому что локальный fresh-process прогон выполнен и зафиксирован выше.

Верификация перепроверки: `run_ue_acceptance.py` — 158/158 с прошедшей identity-валидацией, `ctest` — 123/123, 26 structural gates и их self-test'ы, `validate_docs` — 188 файлов.

## Приёмка M2 (2026-09-14)

Этап принят со второго захода. Первая версия была отклонена не за архитектуру — протокол публикации поколений выдержал мутации сразу, — а за приёмку: продуктовый слой save/load проверялся структурно (файлы существуют, имена различаются, outcome `Completed`), и четыре теста зеленели на тех самых регрессиях, ради которых были написаны.

**Что именно не ловилось и что теперь ловится.** Каждый пункт перепроверен мутацией: вводился дефект, фиксировалось, какой тест краснеет, дефект откатывался.

| Регрессия | Было | Стало |
|---|---|---|
| Обнулить восстановленные PRNG-потоки при загрузке | 23/23 зелёные, включая `PrngStreamContinuation` | краснеет `PrngStreamContinuation` |
| Восстановить валидное, но чужое состояние (игрок в другой локации) | не проверялось | краснеет `ProductionRequestLoad` |
| Сохранить не то состояние / потерять Previous по содержимому | сверялись только имена файлов | сверяются state hash и локация внутри контейнера, байты Current и Previous обязаны различаться |
| Сбросить отложенный save при отказе команды в диспетчере | тест отказывал по грамматике slot id | краснеет `CommandRefusalDiscardsSave` на отказе настоящего handler с валидным slot id |
| Проглотить отказ `Exists` на проверке legacy | операция вне перечисления | `fault_injection_pre_commit_did_not_fail ordinal=2` |
| Вернуть игнорирование `Head.Version` при сериализации | поле молча не round-trip'илось | `head_version_not_serialized` |

**Оракулы стали независимыми.** Граница коммита больше не ищется сканированием трассы по строке `"commit_head"`, которую печатает сама реализация: появились контрактные таблицы стадий с `bIsCommitPoint`, и вся фактическая трасса сверяется поэлементно — ordinal, kind, description — с независимо написанной таблицей. Значения PRNG заданы golden-вектором, а не вычисляются реализацией. Содержимое контейнера читается обратно через Lua, поэтому непрозрачность save container для C++ ([ADR-0021](../../ADR/0021-opaque-save-container.md)), восстановленная в M1, не нарушена.

**Добавлено при приёмке.** Все одиннадцать save/load-тестов работали с одним слотом, то есть одинаково прошли бы реализацию, привязанную к последнему сохранённому слоту; при этом `STATUS-001` был удалён, хотя его собственная формулировка называла `load-another-save`. Добавлен `GV2.Runtime.SaveAndLoad.LoadAnotherSaveAndRestart`: два слота с разным состоянием, загрузка A из живой сессии, затем **из уже загруженной сессии** загрузка B, повторная загрузка того же слота и restart загруженной сессии обычным NewGame. Red-on-revert: переиспользование ранее захваченного контейнера для последующих загрузок красит этот тест.

**Осталось за рамками и записано честно.** Устойчивость к потере питания не заявляется — CFC-08 обещает атомарную видимость и восстановление при отказе операции и падении процесса на поддержанном Linux filesystem, и не больше.

Верификация на принятой ревизии: `run_ue_acceptance.py` — 171/171 с identity-валидацией, `ctest` — 125/125, 26 structural gates и их self-test'ы, `validate_docs` — 188 файлов.

## Приёмка M1 (2026-09-13)

Этап принят после отдельного ревью и двух раундов исправлений. Запись нужна потому, что чекбокс сам по себе не говорит, чем именно приёмка отличалась от «тесты зелёные».

Принято по механизмам, а не по описаниям: множество устанавливаемых реестров и порядок их запечатывания выводятся из одного `DESCRIPTOR`; подмена слота фасада запрещена в рантайме (`safe_rawset` плюс `__newindex`), а не только текстовым гейтом; каждый сайт мутации UE-проекции классифицирован обходом всего дерева `Source/GV2`; доступ C++ к canonical state выводится из фактических Lua C API литералов с отказом на неклассифицированном; непрозрачность save container выводится из фактических использований `LoadContainerBytes`.

Проверено мутациями, а не чтением docstring: `Status = Other` целиком, суррогатный `return` после исчерпывающего `switch` в заголовке, неклассифицированный сайт публикации проекции, отключение правила seed для загрузки — каждая проба краснила свой механизм, откат возвращал зелёное. Отдельно проверено, что спека `registry_sealing` действительно исполняется, а не проходит вхолостую.

Исправления, потребовавшиеся по ходу: регрессия DUC-03 при переходе на ролевые интерфейсы; отказ транзакции на собранной GC цели central style; восстановление проекции, остававшееся выше границы; молчаливый нулевой seed в трёх местах; C++-разбор container bytes, внесённый первым исправлением seed и вернувший тот же дефект в другой форме.

Не закрыто и вынесено: [`STATUS-026`](../../Status/ImplementationStatus.md) — модуль изоляции реестров присутствует в графе production-сессии, хотя сама capability одноразовая и из пакета недостижима.

Верификация на принятой ревизии: `Automation RunTests GV2` — 158/158, `ctest` — 123/123, 26 structural gates и их self-test'ы, `validate_docs` — 188 файлов, golden перегенерирован по документированной процедуре с неизменным `state_hash`.

## Соответствие задач и change set

Правило 2 требует коммит на завершённую задачу. Часть задач M1 была зафиксирована одним коммитом, поэтому их evidence не разделяется историей; таблица восстанавливает соответствие явно, чтобы ревью и последующая архивация не выводили его заново из содержимого diff.

| Задача | Commit | Примечание |
|---|---|---|
| CFC-01 | `aa7c781` | ADR-0044/0045 и owner contracts до кода |
| CFC-02, CFC-02A, CFC-03 | `651500a` | fail-closed runner, fixture lifetime, module graph |
| CFC-02A (порядок teardown) | `6d2ac6a` | закрытие M0 |
| CFC-03A | `4ae4967` | canonical zero и общий SHA-256 validator |
| CFC-04 | `a9b2508` | |
| CFC-04A | `2aaa92a` | |
| CFC-04B, CFC-05, CFC-05A, CFC-06, CFC-07, CFC-07A | `981a4f1` | шесть задач одним change set; разделить историю нельзя, ревью M1 выполнено по содержимому |
| Переписывание поставок M0 после его приёмки | `aa0eb7e` | без task ID: `ue_test_report.py`, `test_ue_test_report.py`, `validate_test_fixture_ownership.py`, `run_ue_acceptance.py`, CI-шаг, build identity (`GV2BuildIdentity.gen.h`, `GV2.Runtime.ModuleIdentity`, `generate_build_identity.py`), RAII fixtures, CMake codemodel, hash codec helpers. Принятое на `6d2ac6a` состояние M0 этим коммитом перестало существовать; перепривязка — «Приёмка M0» выше |
| Ревью M1: B1 | `cdb4217` | также несёт остаток работы M0 по acceptance/MCP (`Tools/MCP/*`, `ue_test_report.py`, `validate_test_fixture_ownership.py`, `test_mcp_transport.py`), которая на момент ревью лежала незакоммиченной |
| Ревью M1: B2…B5, N3…N7 | `0bbb5fb` | |
| Ревью M1: R1 | `c0268f6` | непрозрачность save container и контракт seed по формам старта |
| Приёмка M1 и `STATUS-026` | `b163e03` | |
| Перепроверка и перепривязка приёмки M0 | `fff4f7f` | evidence-прогон `run_ue_acceptance.py` на актуальной ревизии |
| CFC-08, CFC-09, CFC-10 | `a4e6e27` | три задачи одним change set; ревью M2 выполнено по содержимому, заявленная в сообщении верификация была prefix-подмножеством, а не полным набором |
| Ревью M2: B1 (PRNG-континуация) | `e672630` | |
| Ревью M2: B2 (восстановление состояния и пост-сейв мутация) | `bc87726` | |
| Ревью M2: B3 (содержимое контейнера, различие Current/Previous) | `1e961e2` | |
| Ревью M2: B4 (отказ команды отбрасывает отложенный save) | `b1b3c20` | |
| Ревью M2: (a) Exists/IsRegularFile/ListDirectory через адаптер | `8a83518` | |
| Ревью M2: (b) контрактные таблицы стадий вместо вывода границы из реализации | `110dde0` | |
| Ревью M2: (c) сериализация `FSlotHead::Version` | `2698435` | |
| Приёмка M2: `load-another-save`, повторный load, restart и запись о приёмке | `063485b` | закрывает последний сценарий формулировки `STATUS-001` |
| CFC-11 | `2431209` | v2 schema поля сцены с обязательным массивом `characters` |
| CFC-12 | `f92c5c0` | первая итерация gameplay-среза |
| Ревью M3: устранение сайд-эффекта CFC-12 и приёмка задачи | текущий change set | изоляция debug/start.lua, семантический UI input в slice-тесте |

Дальнейшие задачи фиксируются по одной; таблица дополняется в том же change set, что и задача.

## Правила выполнения и остановки

1. Перед кодом прочитать owner contracts задачи, соседние contracts и актуальный diff. Семь пользовательских изменений, перечисленных в аудите, не являются результатом этого плана; изменения тех же файлов согласовать с их текущим содержимым без отката и без присвоения evidence чужой работы.
2. Для каждой задачи сначала выполнить конкретный red scenario, затем изменить production path и его structural gate в одном change set, проверить green и отрицательную мутацию. Коммит — после завершения всей задачи и её проверок, согласно `AGENTS.md`.
3. Универсальное утверждение несёт actual enumerator, независимый oracle и хотя бы один production path. Рукописные fault fixtures задают ожидаемую семантику; они не выдаются за полный actual inventory.
4. Изменение публичного API/ошибки/schema синхронно обновляет owner contract, соответствующий Guide и Authoring reference. Breaking changes классифицировать по [Compatibility Policy](../../Architecture/CompatibilityPolicy.md); постоянные aliases не добавлять.
5. Нет успешного MCP/сборки/asset save/обязательного test run — нет закрытия соответствующей задачи. Ошибка этапа оставляет следующий зависимый этап открытым. Непредвиденный architectural change сначала получает ADR, а не скрытую реализацию в «исправлении».
6. Удалять STATUS только при полном закрытии его формулировки. Audit findings получают конкретный исход и task ID. История не заменяет активный status; архивирование — отдельная двухкоммитная процедура после всех исходов.
7. Evidence выполнения хранить в итоговом отчёте change set/проверяемых CI artifacts: ревизия, команды, discovery/report, результаты негативных сценариев. В задачи не подставлять результаты прежнего аудита как текущий успех.

## Критерий выхода

Все задачи закрыты по собственным Done/Evidence; красные при возврате дефекта проверки выполняются штатным pipeline. На чистой ревизии работают полный portable и UE gates, сохранение/загрузка через игровой host и новый Lua-only gameplay сценарий. CFC-13 фиксирует точную поддержанную поверхность и открытые исключения. При этом у механизма невозможно незаметно добавить новый путь, не расширив его перечислитель или не сломав структурный gate.
