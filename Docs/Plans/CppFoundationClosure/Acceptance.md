---
title: Cpp Foundation Acceptance
status: active
version: 1.2
updated: 2026-09-12
depends_on:
  - ../../Architecture/BuildAndTooling.md
  - ../../Architecture/CompatibilityPolicy.md
---

# Приёмка C++-основы

> **Материализует:** M0 и финальную приёмку M3 [плана](README.md). Сначала исправляется достоверность evidence, затем она используется для закрытия runtime-задач.

## CFC-01 — Согласовать границы и проверяемые outcomes

- [x] CFC-01 — Согласовать границы и проверяемые outcomes

**Файлы:** изменить `Docs/Architecture/BootstrapAndSessionLifecycle.md`, `RuntimeFacadeAndRegistries.md`, `LuaRuntimeContract.md`, `CanonicalStateAndSave.md`, `HeadlessSimulationContract.md`, `BuildAndTooling.md`; `Docs/UI/UIDocumentAndReconciliation.md`, `ScreenTemplates.md`; `Docs/Concepts/PresentationModel.md`; создать [ADR-0044](../../ADR/0044-session-replacement-and-registry-sealing.md) и [ADR-0045](../../ADR/0045-atomic-save-slot-generation-publication.md), добавить их в ADR index. Номера зарезервированы созданными файлами.

**Инвариант:** [Bootstrap lifecycle](../../Architecture/BootstrapAndSessionLifecycle.md), [opaque save](../../ADR/0021-opaque-save-container.md), [C++ scope](../../ADR/0020-cpp-scope-criterion.md). Старую Ready A нельзя одновременно считать сохранённой и лишить её UI; preflight не создаёт вторую VM.

**Решение для реализации:** [ADR-0044](../../ADR/0044-session-replacement-and-registry-sealing.md) фиксирует две границы replacement, private fixed Lua sealing descriptor, phase/result types CFC-06/07 и минимальный public lifecycle API; internal transition token boundary не пересекает. [ADR-0045](../../ADR/0045-atomic-save-slot-generation-publication.md) фиксирует atomic head storage generations CFC-08. Typed save/load requests CFC-09/10 согласованы в owner contracts. Scene policy CFC-11 уточняет existing Screen Fields contract: обязательный массив `characters`, при этом пустой массив допустим; новые обязательные свойства без consumer-смысла не добавляются.

**Не считается закрытием:** новый обзор с обещанием atomicity без разделения двух commit points; новый stateful C++ gameplay service; снятие требований ради зелёного текущего кода.

Compile-to-value API CFC-04A синхронизировать с owner contracts как выполнение уже принятого ADR-0043 D1: authoring DataAsset остаётся входом, resolved runtime state принадлежит snapshot. Новый ADR только ради устранения этого расхождения не требуется.

Дополнение full review: согласовать GC ownership prepared candidates (CFC-04B), перенос state composition в Lua (CFC-05A), canonical number/hash domain (CFC-03A) и seed transport до bootstrap (CFC-07A). Для deterministic streams определить алгоритм и независимые vectors в owner contract; формат полного uint64 seed в manifest/digest и его migration принять до изменения codecs. Это часть существующего foundation scope, а не разрешение новых native gameplay services.

**Шаги:**
1. Сопоставить каждый пункт таблицы находок README с точной нормой и production path из аудита.
2. Зафиксировать ADR и полные failure/ownership/API semantics в существующих owner contracts, включая ограничение первого Linux Development baseline и явные открытые effects/animation gaps.
3. Исправить противоречивые сведения о неготовых migrations и UI document; нормативные утверждения проверить по callers/tests, информационные описания привести к ним.
4. Проверить links/front matter/cycles, semantic diff против accepted ADR и coverage задач; закоммитить завершённое согласование.

**Done:**
- Две границы replacement, одна VM, byte-buffer identity, outcomes отмены и отказа описаны однозначно.
- Native/Lua responsibilities и фиксированные signatures downstream-задач согласованы в owner contracts.
- Для каждого универсального Done этого плана указан actual enumerator; перечень задач берётся из checkbox headings всех файлов этого каталога.
- Ни один STATUS не удалён только вследствие нового документа.

**Evidence:** validator и сопоставление таблицы находок с task IDs; semantic review ADR/contracts с результатом по каждому решению. Это document evidence, оно не подтверждает runtime readiness.

## CFC-02 — Сделать UE-приёмку единой и fail-closed

- [ ] CFC-02 — Сделать UE-приёмку единой и fail-closed

**Файлы:** изменить `Tools/MCP/run_ue_tests.py`, `Tools/MCP/mcp_client.py`, `.github/workflows/linux-ci.yml`, `Source/CMakeLists.txt`, `Docs/Architecture/BuildAndTooling.md`; создать `Tools/Testing/ue_test_report.py`, `Tools/Testing/test_ue_test_report.py`, `Tools/Testing/run_ue_acceptance.py`, `Tools/Testing/test_mcp_transport.py`. Report module только валидирует normalised evidence; runners отвечают за transport/process/discovery.

**Интерфейс:** `validate_run(discovered: set[str], report: dict, run_identity: dict) -> list[str]`; пустой список разрешён только для полного успешного текущего запуска. Common normalised record: `name`, `state`, `errors`; identity: `run_id`, `source_revision`, `source_diff_hash`, `build_fingerprint`. Adapters MCP и UE JSON не принимают неизвестную форму отчёта молча.

**Инвариант:** [Build and Tooling](../../Architecture/BuildAndTooling.md). «Проверен GV2» означает исполненный актуальный набор, не marker в старом log. Actual enumerator — UE Automation discovery текущего build; expected success predicate задаётся независимыми fixtures протокола.

**Не считается закрытием:** заменить только filter; проверить `passed > 0`; хранить число 141; принять report предыдущего task; считать транспортный ACK завершением тестов.

**Шаги:**
1. Воспроизвести VERIFY-AF-02 через настоящий `main()` с fake client: total 1, passed 0, failed 0, skipped 1, единственный NotRun; требовать ненулевой exit.
2. Реализовать общий validator и fixtures: empty, missing/extra/duplicate names, Unknown/NotRun/InProcess/Fail, errors при Success, несовпадающие totals, stale run, отсутствующая identity, усечённый JSON; positive Success и Success с warnings проверяются отдельно.
3. Подключить validator в оба runners; убрать неидентифицированный fallback `GetTestResults`. При async API дождаться terminal результата связанного task с timeout. Discovery читать полностью, без default limit.
4. Исправить SSE чтение: короткий event, несколько chunks, malformed event, disconnect и timeout воспроизводятся локальным fake server; сетевой error всегда non-success, бесконечного ожидания нет.
5. CI и local acceptance запускают весь `GV2` из fresh editor process и сверяют discovery/report. Для fresh discovery использовать штатный automation inventory API через commandlet/automation adapter; если inventory нельзя получить, runner завершается ошибкой, не выводит его из completed report. MCP в уже открытом Editor допускается как дополнительный developer run; mismatch загруженных библиотек запрещает считать его freeze evidence.
6. Зарегистрировать runner/transport self-tests в CTest, выполнить их и полный fresh UE run; обновить contract, закоммитить задачу.

**Проверка алгоритма:**
```python
assert validate_run({"GV2.A"}, valid_success_report, matching_identity) == []
assert validate_run({"GV2.A", "GV2.B"}, valid_success_report, matching_identity)
assert validate_run({"GV2.A"}, not_run_report, matching_identity)
assert validate_run({"GV2.A"}, valid_success_report, stale_identity)
```
Fixtures содержат явные counters и records, а не результат преобразования проверяемого validator.

**Done:**
- Local и CI вызывают один validator; negative fixtures проверяют его exit через runners, а не только helper.
- Actual discovered set совпадает с уникальным completed set; каждый state успешен, errors отсутствуют, counters согласованы.
- Evidence связано с проверенными исходниками и загруженными бинарниками; отсутствие связи не считается успехом.
- Полный набор включает UI и Editor test namespaces автоматически при их регистрации.
- Transport self-tests и реальный MCP вызов завершились успешно; fresh UE run принят новым validator.

**Evidence:** отрицательный исход каждого fixture, discovery/report/identity реального запуска, exit codes local и CI-equivalent команд. Изменение YAML не доказывает успешность удалённого CI; фактический remote run указывать отдельно.

## CFC-02A — Изолировать lifetime и mutable настройки automation fixtures

- [ ] CFC-02A — Изолировать lifetime и mutable настройки automation fixtures

**Зависимость:** CFC-02. **Файлы:** `Source/GV2/Private/Tests/GV2UiPrepareCommitTests.cpp`, `GV2UiCapabilityObservabilityTests.cpp`, `GV2ForgeryTestWidgets.h/.cpp`, `GV2PresentationTestFixtures.h` в том же каталоге; actual остальные test root/settings mutators вывести из `Source/**/Tests` и test support. Создать `Tools/Testing/validate_test_fixture_ownership.py` с negative self-tests и включить в CTest; обновить `Docs/Architecture/BuildAndTooling.md`.

**Инвариант:** [достоверная приёмка](../../Architecture/BuildAndTooling.md). Fixture не оставляет rooted GameInstance/world и изменённый global mode следующему тесту. Требование test isolation закрепляется в contract этим change set; текущие leaks не выдаются за уже воспроизведённый production OOM.

**Решение:** общий scoped world/GameInstance owner возвращает borrowed World/Widget и выполняет полный teardown на любом выходе. Удержание root и Shutdown/world cleanup принадлежат одному RAII owner; factory не возвращает widget с потерянным owner. `FScopedForgeryMode` сохраняет прежний Mode и восстанавливает его при destruction; setter становится private для scoped helper. Экземпляр forgery widget фиксирует выбранный mode, если используется после выхода scope, чтобы следующая смена global mode не меняла уже созданный объект.

**Не считается закрытием:** RemoveFromRoot только в конце happy path; обнулить global настройку вместо возврата прежней; посчитать только число Add/Remove в исходнике; маскировать unrooted candidates вечным root fixture.

**Шаги:**
1. Через повторный запуск реальных `GV2.UI.PrepareCommitAndFailureInjection` и capability tests измерить оставшиеся fixture-owned rooted objects/world contexts; отдельно проверить Mode до/после forgery test. Исходные AddToRoot не имеют paired cleanup, последний Mode остаётся UnimplementableKind.
2. Ввести scoped owners и перевести actual helper callers; создать fixture с ранним failure/return. Проверить очищенные world contexts, root set и collectibility через weak refs после scope; normal suite success не заменяет teardown evidence.
3. Закрыть direct forgery mode assignment; проверить вложенные scopes, early return и разные порядки двух настоящих tests. Expected initial/final Mode задаёт тест до запуска, а не helper после cleanup.
4. Gate выводит actual root/mode mutator sites из test sources и допускает их только в scoped owners. Новые raw AddToRoot или global assignment в temporary fixture делают полный gate красным. Object counts измеряются по test ownership, не по всем Editor roots, которые пользователь вправе менять.
5. Повторить affected suite в одном fresh process 20 раз и в двух заданных порядках; roots/worlds возвращаются к исходному fixture baseline, Mode восстановлен. Затем выполнить полный CFC-02 runner и docs validation.

**Done:**
- Actual root/mode mutation sites принадлежат scoped owners, неизвестный direct writer обнаруживается gate.
- Fixture-owned objects/worlds освобождены при success и early failure; weak refs подтверждают collectibility после GC.
- Mode и поведение соседнего теста не зависят от порядка запуска; foreign initial value восстановлен точно.
- 20 повторов не накапливают fixture-owned roots/world contexts; это конкретный corpus, не утверждение о всей памяти UE.
- CFC-AF-02/10 получают исход CFC-02A; production OOM или настоящий flaky run без воспроизведения не заявлены.

**Evidence:** actual mutator inventory, before/after root/world/Mode measurements, early-exit and order fixtures, negative mutations, repeated/полный UE reports. GC acceptance CFC-04A/04B проводится без протекающих roots, которые могли скрыть дефект.

## CFC-03 — Закрыть неполный inventory графа сборки

- [ ] CFC-03 — Закрыть неполный inventory графа сборки

**Файлы:** `Tools/Testing/validate_presentation_apply_module_graph.py`, `Source/GV2PresentationApply/GV2PresentationApply.Build.cs`, `Source/GV2/GV2.Build.cs`, `Source/CMakeLists.txt`, `Docs/Architecture/BuildAndTooling.md`; новые fixtures/self-tests размещать рядом с существующим gate.

**Инвариант:** [ADR-0043 D2/D4](../../ADR/0043-presentation-apply-boundary.md). Неизвестный dependency expression — отказ анализа. Новый authority module вне UE allowlist автоматически запрещён, его имя не нужно добавлять в denylist.

**Решение:** для Apply использовать маленькую полностью разбираемую декларативную форму Build.cs. Gate проверяет весь constructor/class body после корректного tokenization строк/comments; допустимы только согласованные assignments и literal dependency lists. Helpers, inheritance от custom rules, условные/dynamic dependency expressions, aliases, include-path/dynamic module lists и неизвестные statements отклоняются явно. Для общего GV2 Build.cs использовать отдельную политику, не ослаблять ею Apply grammar. CMake inventory получать из configured codemodel/trace фактических targets и link inputs, не из рукописного списка команд.

**Не считается закрытием:** добавить regex только для `new[]`; молча игнорировать helper call; объявить текстовый список настоящим UBT evaluated graph.

**Шаги:**
1. Через полный `validate_repository()` воспроизвести пропуск `AddRange(new[] { "GV2ContentCore" })` из аудита.
2. Реализовать restricted parser с учётом всех tokens; сам allowlist взять из contract, actual module references — из parsed program. Unknown token/statement выдаёт diagnostic.
3. Через временную копию repository проверить explicit/inferred arrays, Add, assignment, helper, alias, conditional, dynamic/include dependency, custom base class и синтетический новый module. Это independent mutation corpus, не генератор ожидаемых ответов из parser.
4. Проверить configured CMake target/source/link inventory; подмешанный UE source через macro либо входит в inventory и отклоняется, либо unsupported configure не принимается как green.
5. Реальной UBT сборкой negative fixture подтвердить недоступность authority header/type из Apply при разрешённом графе. Положительный UBT и portable build проходят; fixtures не меняют рабочий production tree.

**Done:**
- Весь Apply build declaration разобран или отвергнут; непокрытые куски не игнорируются.
- Каждый отрицательный fixture останавливает полный gate, включая первоначальный inferred-array bypass.
- Dependency allowlist и запрет authority access подтверждены gate и compiler-negative fixture.
- Portable actual build graph не содержит UE presentation; ограничение проверенных configurations записано явно.

**Evidence:** self-tests, temporary mutation diagnostics, compiler negative/positive outputs, CTest, UBT. `STATUS-016` удалить только после всех проверок.

## CFC-13 — Зафиксировать поддержанную C++/Lua-поверхность

- [ ] CFC-13 — Зафиксировать поддержанную C++/Lua-поверхность

**Зависимость:** CFC-01…12, включая CFC-02A, CFC-03A, CFC-04A/04B, CFC-05A, CFC-07A. **Файлы:** `Docs/Architecture/BuildAndTooling.md`, `Docs/Guides/WhenToWriteCpp.md`, `Docs/Guides/AddLuaSpec.md`, `Docs/Authoring/README.md`, `Docs/Status/AuditFindings.md`, `Docs/Status/ImplementationStatus.md`; CI artifacts и локальный `Saved/Audit/` для полных отчётов.

**Инвариант:** [scope](../../Architecture/Overview.md), [совместимость](../../Architecture/CompatibilityPolicy.md). Готовность относится к зафиксированной поверхности и ревизии, а не к абстрактному «всему C++».

**Не считается закрытием:** повтор старых 104/141 результатов; review только helper tests; удаление известных STATUS-002/003; план со всеми checkbox без red-on-revert evidence.

**Шаги:**
1. Сверить каждый Done с именем проверки, actual enumerator и независимым oracle. Проверить новые public native entry points по исходникам `Public/` и bindings; новые enum values обязаны попадать в exhaustive dispatch/test inventory.
2. Выполнить targeted negative mutations каждой устранённой причины: второй schema source, shared mutable Screen Registry между A/B, старый candidate, ранний host teardown, игнорируемый freeze result, неподключённый storage, потерянная previous copy, запрещённый dependency statement, NotRun и missing UE record. Дополнительно вернуть untraced owning widget/class pointer, leaked test root/global mode, native semantic state merge, ignored seed, signed-zero mismatch и len-only hash validation; штатные tests/gates обязаны обнаружить каждый. Для registry повторить failed B и successful B с отличающимися inputs/package closure, проверяя exact Resolve A, а не только snapshot pointer/hash. Мутации живут в временных checkout и обязаны краснеть в штатном pipeline.
3. На чистой ревизии выполнить приведённый ниже runbook, full UE test inventory и CFC-12. Зафиксировать revision, build fingerprints, package/script hashes, environment, warnings и ограничения.
4. Выполнить portable ASan/UBSan build и CTest/shared conformance для Lua/native marshalling и storage; документировать unsupported toolchain отдельным препятствием для этой задачи. Это проверка памяти на исполненных сценариях, не доказательство всего возможного ввода.
5. Удалить только полностью закрытые status rows; записать исход каждой audit-находки и task ID. Обновить Guide: новое native API требует scope reason, production consumer, negative fixture и enumerator в одном change set; обычные Lua commands/services/presentation не требуют нового C++.
6. Зафиксировать supported Linux Development baseline и открытые effects/animations/Shipping/platform limits; выполнить docs validator, закоммитить завершённую приёмку. Архивировать план/аудит затем по отдельной двухкоммитной процедуре, без фиктивных исходов.

**Runbook:**
```bash
cmake -S . -B cmake-build-ci -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-ci --parallel 2
ctest --test-dir cmake-build-ci --output-on-failure
./cmake-build-ci/Headless/gv2-headless --self-test
./cmake-build-ci/Headless/gv2-headless --check-scripts
./cmake-build-ci/Tools/Content/gv2-content validate GameData/core
./cmake-build-ci/Tools/Content/gv2-content coverage GameData/core
/opt/unreal-engine/Engine/Build/BatchFiles/Linux/Build.sh GV2Editor Linux Development /home/king/ue5/GV2/GV2.uproject -WaitMutex -NoHotReloadFromIDE
python3 Tools/Testing/run_ue_acceptance.py --filter GV2 --fresh-process
python3 Tools/Documentation/validate_docs.py
git diff --check
```
`run_ue_acceptance.py` и его CLI создаются CFC-02; до её выполнения команда не существует. UE root может быть задан текущим environment; пользовательский Editor не завершать ради fresh-process run. Sanitizer configuration CFC-13 использует отдельный build directory, чтобы не подменить release evidence.

**Done:**
- Каждый checkbox плана сопоставлен с выполненным evidence; enumerator — actual task headings, а не ручная сводка выполненного.
- Все targeted mutations отвергнуты pipeline; ожидаемые причины отказа проверены, не только nonzero exit.
- Fresh portable, full UE, sanitizer и вертикальные проверки прошли на зафиксированных inputs.
- Реальные remote CI результаты отделены от local equivalent; если remote запуск недоступен, он не объявлен выполненным.
- Поддержанная поверхность и оставшиеся gaps описаны без обещания абсолютной корректности.

**Evidence:** итоговый отчёт с командами/exits, именами тестов, artifact identity, результатами mutations и ссылкой на ревизию. Находки после проверки вне проверенной поверхности создают новый конкретный gap; не устраняются общим заявлением «архитектура чистая».
