---
title: Cpp Foundation Closure Implementation Plan
status: active
version: 1.2
updated: 2026-09-12
depends_on:
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/RuntimeFacadeAndRegistries.md
  - ../../Architecture/CanonicalStateAndSave.md
  - ../../Architecture/BuildAndTooling.md
decisions:
  - ../../ADR/0020-cpp-scope-criterion.md
  - ../../ADR/0021-opaque-save-container.md
  - ../../ADR/0043-presentation-apply-boundary.md
---

# C++ Foundation Closure: план реализации

> **Материализует:** принятые границы ownership, session publication и opaque save в работающей UE composition и проверяемой процедуре приёмки. Основание — [текущий аудит](../../Status/AuditFindings.md) и accepted ADR выше.
> **Не является нормативным:** проектирование ниже описывает рекомендуемую реализацию существующих правил. Новые API, failure semantics и решения сначала фиксируются задачей CFC-01 в owner contracts; план не подменяет ADR.
> **Исполнение:** использовать `superpowers:executing-plans`, последовательно по задачам. Checkbox задачи — единственный источник её завершения; все задачи сейчас открыты.

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
| M3 — Lua baseline | CFC-11…13, [Save and Gameplay](SaveAndGameplay.md), [Acceptance](Acceptance.md) | Обязательность сцены, сквозной сценарий и зафиксированная поддержанная поверхность |

Зависимости: `01 → 02 → 02A → 03 → 03A → 04 → 04A → 04B → 05 → 05A → 06 → 07 → 07A → 08 → 09 → 10 → 11 → 12 → 13`. Порядок намеренно последовательный: следующая приёмка использует уже исправленный runner, а save/load использует уже испытанный replacement. Реализацию одного этапа можно ревьюить и отклонять независимо от следующего; массовое переписывание всех surfaces одним commit не требуется.

- [ ] M0 — CFC-01…03 и CFC-02A/03A приняты по Done/Evidence.
- [ ] M1 — CFC-04…07 и CFC-04A/04B, CFC-05A/07A приняты по Done/Evidence.
- [ ] M2 — CFC-08…10 приняты по Done/Evidence.
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
