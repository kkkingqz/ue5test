---
title: Cpp Foundation Save and Gameplay
status: active
version: 1.1
updated: 2026-09-14
depends_on:
  - ../../Architecture/CanonicalStateAndSave.md
  - ../../Architecture/BootstrapAndSessionLifecycle.md
  - ../../Architecture/AuthoringSurfaceContract.md
  - ../../Architecture/BuildAndTooling.md
  - ../../Architecture/CompatibilityPolicy.md
---

# Save/load и проверка Lua-геймплея

> **Материализует:** M2 и финальную приёмку M3 [плана](README.md). Storage остаётся native capability, state/preflight/migrations и игровые правила — Lua-owned.

## CFC-08 — Сохранять предыдущее поколение opaque slot

- [x] CFC-08 — Сохранять предыдущее поколение opaque slot

**Файлы:** `Source/GV2RuntimeCore/Public/GV2RuntimeCore/GV2HostServices.h`, `Source/GV2RuntimeCore/Private/GV2SaveSlotStorage.cpp`, `GV2SaveSlotStorageConformance.cpp` в том же каталоге; `Docs/Architecture/CanonicalStateAndSave.md`, `Docs/Architecture/BuildAndTooling.md`. Для process-crash harness создать `Tools/Testing/test_save_slot_crash.py` и отдельный test executable в portable test build.

**Интерфейс:** `ESaveSlotRevision { Current, Previous }`; `ReadSlot(SlotId, Revision)` возвращает прежний `FSaveSlotReadResult`, default Current допускается только как C++ source convenience, не как скрытая fallback policy. `WriteSlot` по-прежнему принимает только SlotId/opaque bytes. Current успешной записи равен новым bytes, Previous — непосредственно предшествующим committed bytes. Нет Previous для первой записи: `NotFound`. Добавить `Busy` в `ESaveSlotResult` и factory `FFilesystemSaveSlotStorage::Open(RootDir)` с `FSaveSlotStorageOpenResult { Result, Storage }`, где Storage — unique owner либо null при отказе; constructor становится private, успешно созданный object всегда владеет lock. Actual constructor callers переводятся через compile errors и reference inventory.

**Инвариант:** [ADR-0021](../../ADR/0021-opaque-save-container.md). Хост не знает, являются ли bytes валидным gameplay save. Задача гарантирует атомарную видимость и recovery при ошибке операции/падении процесса на поддержанном Linux filesystem; устойчивость к потере питания не заявляется без отдельного fsync/directory-durability протокола и его evidence.

**Решение:** immutable generation files и один storage-owned head, содержащий имена Current/Previous. Один exclusive storage-owner lock запрещает второму writer process открыть тот же root (`Busy`); внутри owner чтения и записи сериализуются, чтобы cleanup не удалял читаемую generation. Запись создаёт новые bytes во временном файле, flush/close и rename публикуют immutable generation; затем временный head с парой `(new_generation, old_current)` атомарно заменяет прежний head. Это единственная commit point. До неё error сохраняет оба старых указателя, после неё cleanup failure не превращает committed write в false failure. На следующем запуске cleanup удаляет только файлы, на которые не ссылается прочитанный head. Head имеет собственную version/строгую bounded grammar и не содержит gameplay metadata; все имена разрешаются только внутри slot scope. Malformed head даёт typed Unreadable, не догадку о save validity. Power-loss durability не обещается.

**Не считается закрытием:** backup только при неуспешной записи; копия в test helper; парсинг Lua container в C++; автоматический выбор «валидного» previous хостом.

**Шаги:**
1. Через реальный filesystem storage дважды записать независимо заданные bytes с NUL. Требовать Current=B и Previous=A через public API и actual directory inventory; исходная реализация теряет A.
2. Реализовать описанный publish protocol и Revision read. Сохранить slot grammar/path confinement, отказ чтения неверного типа файла, write serialization и typed errors. Классифицировать native API/storage-layout change по CompatibilityPolicy: при отсутствии head существующий single-current slot читается как Current; первый overwrite копирует его bytes в immutable generation до commit head, после которого он становится Previous. Отказ до commit оставляет legacy slot читаемым. Это migration размещения opaque bytes, не содержимого Lua container.
3. Вынести filesystem operations в узкий internal adapter для fault injection. Каждая фактически выполняемая операция проходит adapter; trace нумерует обращения. Для каждого ordinal из реального baseline исполнения инъецировать I/O failure; отдельно добавить исходы первой записи, overwrite и cleanup после commit.
4. Process harness завершает отдельный writer после каждого фактически зарегистрированного этапа, затем свежий reader проверяет Current/Previous по независимой таблице допустимых byte pairs. Это проверка process crash, не имитация power loss.
5. Выполнить одновременную запись двух requests и попытку второго process owner; проверить обещанную serialization/Busy semantics. Никакого silent last-wins за пределами одного защищённого writer.
6. Общую conformance выполнить в UE/headless, harness — portable CI; синхронизировать contracts и error semantics, закрыть STATUS-019/SAV-AF-02 только после read Previous и crash matrix.

**Done:**
- После каждой успешной overwrite Current/Previous совпадают с независимо заданными новыми/предыдущими bytes.
- Каждый actual filesystem stage участвует в fault/crash enumeration; ожидаемые пары не вычисляются из storage implementation.
- До publish failure Current и Previous нетронуты; после publish cleanup failure не даёт двусмысленного failed-write result.
- Temp recovery и concurrency behavior проверены отдельным процессом.
- В C++ отсутствуют container parsing/integrity/migration rules; bytes с NUL проходят roundtrip.

**Evidence:** actual operation trace, per-stage failure/crash results, byte-pair oracle, first/overwrite/concurrent cases, conformance обоих hosts. Не расширять вывод на другие ОС/потерю питания.

## CFC-09 — Подключить storage и safe-point save к UE-host

- [x] CFC-09 — Подключить storage и safe-point save к UE-host

**Файлы:** `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, public header; coordinator/bridge types из CFC-06/07; `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` и public header; `Scripts/boundary/entrypoints.lua`, `Scripts/boundary/outbound.lua`, `Scripts/runtime/save.lua`, `Scripts/runtime/command_dispatcher.lua`; создать `Scripts/runtime/session_controls.lua` и добавить в bootstrap manifest/module graph; `Tests/Lua/save/save_path.lua`, UE subsystem tests. Docs: `CanonicalStateAndSave.md`, `LuaRuntimeContract.md`, `BootstrapAndSessionLifecycle.md`, `RuntimeFacadeAndRegistries.md`; добавить раздел save/load controls в существующий `Docs/Authoring/PresentationAuthoringReference.md` и обновить его index в `Docs/Authoring/README.md`. Отдельный дублирующий reference не создавать.

**Интерфейс:** `RequestSave(SlotId)` — typed host control request, возвращает operation ID/outcome в единой operation policy CFC-07. Fixed portable `FRuntimeSession::SaveToSlot(SlotId, OutFault)` вызывает существующий Lua `save` entry point только на safe point. Lua entry `game.bridge.request_save(slot_id)` ставит value-only request; core Lua handler зарегистрирован под `core:command.session.save` и вызывается обычным bound UI command. Outbound request буферизуется до успешного окончания command dispatch; при отказе команды отбрасывается. Outcome возвращается typed technical input, не post-commit gameplay fact. Game-instance composition владеет успешно открытым `FFilesystemSaveSlotStorage` под разрешённым application save root и передаёт ссылку runtime до `Start`/load; lifetime storage длиннее session. Lua не получает root path. Storage остаётся optional для library hosts, но capability явно присутствует в игровом UE profile.

**Инвариант:** [Save safe point](../../Architecture/CanonicalStateAndSave.md#safe-point), [C++ façade](../../Architecture/BootstrapAndSessionLifecycle.md), [Semantic Input](../../UI/SemanticInput.md). Save не является gameplay mutation и физически не исполняется внутри command handler: handler только запрашивает отложенную операцию. UI продолжает посылать bound command ID через существующий input path, без special widget callback или обхода dispatcher.

**Не считается закрытием:** `SetSaveSlotStorage` только в тесте; прямой WriteSlot вместо Lua save; вызвать save синхронно из mutation window; проверить файл без достигнутого игрового состояния.

**Шаги:**
1. Из реального `UGV2RuntimeSubsystem` выполнить gameplay command и RequestSave. Проверить terminal successful outcome и Lua-produced container; исходное production wiring возвращает unavailable. Затем пройти тот же путь кнопкой Lua-authored экрана через bound `core:command.session.save`, без прямого вызова RequestSave из финального acceptance-теста.
2. Подключить application-owned storage во всех session creation paths единой composition. Перечислитель — actual Start/StartFromSave callers и closed start-mode enum; новый путь без injection отвергается gate/обязательным host context.
3. Добавить fixed save entry point и bridge queue, core Lua command handler; request от неуспешного dispatch отбрасывается. При работающем Lua запрос только ставится в очередь; drain command/event queues согласно safe-point contract перед экспортом. Overflow, stale generation, teardown и storage failure имеют typed terminal outcomes; save не перескакивает через queued mutation. C++ dispatch не содержит `core:command.session.save`: он получает typed request из boundary.
4. Проверить request из idle и во время command execution; последний command effect входит в save только согласно согласованному FIFO/safe-point порядку. Lua specs задают expected scalar hash и IDs, сериализацию C++ не анализирует.
5. Проверить два последовательных product saves и Previous через storage; обновить owner contracts/Authoring сигнатуры и примеры, проверить source consumer inventory, shared/UE tests.

**Done:**
- Production UE session получает storage до старта и успешно сохраняет достигнутое командой состояние через Lua.
- Lua-authored save button достигает той же storage через обычный bound command; native special-case имени команды отсутствует.
- Request во время исполнения Lua не вызывает re-entry и выполняется только на safe point.
- Отказ вызывающей команды отбрасывает pending save request; failed mutation не оставляет внешнего write.
- Все start modes получают один host storage capability из composition; second test-only injection path не является единственным consumer.
- Ошибки/отмена/teardown завершают operation типизированно, без публикации ложного success.
- Authoring/reference объясняет различие gameplay Command и save control request, содержит рабочий пример и error outcomes.
- STATUS-018 уточнён как частичный до успешной load-приёмки CFC-10, не удалён преждевременно.

**Evidence:** public runtime request → Lua save → native storage trace, expected hashes/IDs, queued safe-point scenario, actual callers и typed failure fixtures, UE run.

## CFC-10 — Загрузить захваченные bytes через единый replacement

- [x] CFC-10 — Загрузить захваченные bytes через единый replacement

**Дополнение CFC-07A:** Load восстанавливает Lua-owned stream state из save; seed нового прохождения не переинициализирует загруженные streams. Проверить actual следующий random результат, а не только наличие meta.prng в контейнере.

**Файлы:** coordinator/subsystem/bridge types и transition policy CFC-07; `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp` и public header; `Scripts/runtime/load.lua`, `Scripts/boundary/entrypoints.lua`, `Tests/Lua/save/load_path.lua`; существующий `GV2ColdStartLoadConformance.cpp` и UE transition tests. Docs: `CanonicalStateAndSave.md`, `BootstrapAndSessionLifecycle.md`, `LuaRuntimeContract.md`, save/load Authoring reference CFC-09.

**Интерфейс:** `RequestLoad(SlotId, ESaveSlotRevision)` дополняет typed start descriptor и использует ту же operation policy. `game.bridge.request_load(slot_id, revision)` принимает только revision `current`/`previous`; core Lua `core:command.session.load` переводит bound UI input в этот fixed request с тем же post-dispatch buffering, что CFC-09. `FRuntimeSession::PreflightSaveBytes(Bytes, OutFault)` выполняет read-only проверку в active Lua VM и отдаёт только outcome; `StartFromSaveBytes(...)` принимает immutable buffer и вызывает тот же internal load builder, что существующий `StartFromSave`. Slot-reading overload делегирует bytes overload, а не дублирует lifecycle. Переносимый input — opaque bytes, не Lua table. Результат preflight не обещает успех будущих module hooks B.

**Инвариант:** [Load](../../Architecture/CanonicalStateAndSave.md#load), [одна VM](../../Architecture/BootstrapAndSessionLifecycle.md), [ADR-0021](../../ADR/0021-opaque-save-container.md). До teardown active VM проверяет захваченный save; после teardown B получает ровно эти bytes и повторно выполняет полную validation/migration перед присвоением state.

**Не считается закрытием:** вызвать только portable cold-load; preflight во второй VM; передать decoded tree в C++; проверить slot A, затем после teardown прочитать уже перезаписанный slot B; автоматически fallback на Previous.

**Шаги:**
1. Запустить настоящую UE game session A, изменить состояние командой, сохранить слот; изменить состояние ещё раз и запросить load сохранённого через Lua-authored кнопку с bound command. Требовать новую generation, прежние сохранённые hash/IDs и успешную следующую команду. Реализация Lua load handler живёт в `Scripts/runtime/session_controls.lua` из CFC-09, C++ не распознаёт имя команды.
2. Прочитать выбранную revision один раз в request-owned immutable buffer. Active Lua выполняет preflight без изменения canonical state/registries/queues; проверки capability/mod compatibility остаются Lua-owned. Для cold start без active VM применяется существующий cold-load путь; никакая вторая VM не создаётся.
3. Использовать CFC-07 для BeginReplace/teardown/start B; repository version перепроверить перед BeginReplace. При смене current repository отклонить request до разрушения A. После BeginReplace B pinned к выбранному snapshot, дальнейшая application publication не переключает его.
4. Проверить absent/corrupt/incompatible/current и explicit Previous revision. Failed preflight сохраняет identity и работоспособность A/UI; post-teardown migration/start/initial-apply failure даёт recovery. Ожидаемые error codes берутся из owner contract fixtures.
5. Между preflight и teardown перезаписать filesystem slot: B обязана загрузить исходный captured buffer. Проверить cancel до/после BeginReplace, load-another-save, повторный load и restart загруженной session через public entry points.
6. Подтвердить полную матрицу STATUS-001 вместе с CFC-07; обновить load API/Authoring examples и текущие status/audit outcomes в одном change set.

**Done:**
- UE RequestLoad выполняет настоящий replacement и продолжает gameplay с сохранённым состоянием.
- Lua-authored load button достигает этого же пути через existing semantic input, без test-only/API-only wiring.
- Preflight сохраняет hash/state/registries/queues A и работает в единственной active VM.
- Byte identity сохранена между preflight и B даже при внешней перезаписи слота.
- До BeginReplace ошибки/отмена оставляют рабочую A; после него — согласованное recovery без partial Ready.
- New/Menu/Restart/Load/Reload/Shutdown и phase cancellation покрыты actual request/phase enum inventory и production traces CFC-07.
- STATUS-001 и STATUS-018 удалены только при полном выполнении их формулировок; SAV-AF-01 получает исход CFC-09/CFC-10.

**Evidence:** production save/load/continue trace, independent state hash/instance ID assertions, one-VM count, preflight immutability и slot replacement adversarial scenario, shared conformance/UE results.

## CFC-11 — Сделать присутствие сцены проверяемым контрактом данных

- [x] CFC-11 — Сделать присутствие сцены проверяемым контрактом данных

**Файлы:** создать `GameData/textsystem/schemas/ui_field_location_scene_v2.schema.json5`, заменить references прежней версии из actual schema-ID inventory; publisher — `GameData/textsystem/scripts/presentation/location_presenter.lua`; `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`; Lua presentation specs; `Docs/UI/ScreenTemplates.md`, `Docs/Authoring/PresentationAuthoringReference.md`. Затронутые Widget Blueprint найти через Screen Registry → фактический WidgetTree → schema references, а не рукописный список ассетов.

**Инвариант:** [Screen Fields](../../UI/UIDocumentAndReconciliation.md), [schema defaults](../../ADR/0009-explicit-schema-defaults.md). Сцена может быть визуально пустой по явным данным; отсутствие обязательной публикации не должно выглядеть корректной пустой сценой.

**Рекомендуемое решение:** `characters` становится обязательным массивом, пустой массив остаётся допустимым; backgrounds/context и root `key` остаются optional. Actual publisher уже всегда формирует `characters`; наличие root `key` он не обещает, поэтому искусственно требовать его незачем. Присутствие scene field охраняет существующий `UGV2ScreenWidgetBase::BuildPreparedFieldPlans`: actual configured hosts из WidgetTree обязаны иметь payload. Проверить эту production связь отдельно, не создавать второй required-field механизм. Version 2 schema получает новый ID `textsystem:schema.ui_field.location_scene.v2`; прежний ID не меняет смысл молча.

**Не считается закрытием:** «хотя бы одно значение пришло» только в тесте; требование optional поля без сценария; обязательность children вместо присутствия parent field.

**Шаги:**
1. Воспроизвести принятие `{}` через настоящий Lua publication → Prepare. Missing scene проверить отдельно как уже ожидаемый отказ существующего host inventory; зелёный baseline этого свойства не выдавать за новое исправление.
2. Обновить owner declaration/schema/publisher; классифицировать breaking schema change и version/typed refusal по policy. Изменение Stable ID версии не переиспользует опубликованный ID с другим смыслом; fixtures/consumers переводятся явно.
3. Проверить валидную пустую сцену с `characters = {}`, заполненную сцену и отсутствие каждого required field. Actual required set выводится из schema, expected визуальные/identity значения задаются fixture независимо.
4. Проверить missing parent field отдельно в production document path. Все найденные assets с versioned schema reference загрузить, обновить, скомпилировать и сохранить через unreal-mcp; проверить повторной загрузкой. Пустой actual asset-reference inventory фиксируется как отсутствие необходимых asset edits, не заменяет sweep.
5. Обновить human-facing authoring example и STATUS-011 после полного закрытия.

**Done:**
- Отсутствие scene field и отсутствие обязательных schema properties типизированно отклоняются production Prepare.
- Явная пустая сцена остаётся допустимой и реконструируется корректно.
- Schema/owner publisher/examples согласованы; required-set enumeration непусто для заявленного свойства.
- Compatibility outcome и version change подтверждены positive/negative fixtures.

**Evidence:** missing-parent/empty-object/valid-empty/filled production cases, independently expected children state, schema/asset inventory, MCP compile/save/reload и UE/Lua results. Это content/Lua closure, не повод добавлять C++-знание о сцене.

## CFC-12 — Подтвердить gameplay-срез без новой native логики

- [x] CFC-12 — Подтвердить gameplay-срез без новой native логики

**Дополнение CFC-07A:** fixture включает детерминированную seeded команду и продолжение stream после save/load; ожидаемая последовательность задаётся независимо. Полный сценарий выполняется после GC/test-lifetime исправлений CFC-02A/04B, чтобы leaked roots не маскировали ownership defects.

**Файлы:** добавить Lua specs в `Tests/Lua/` по существующему tier discovery; для сценария использовать `GameData/sample` fixture package и существующие authoring patterns; общий portable conformance только для C++-механизма host-control transport при необходимости, не для новых Lua-правил. UE сценарий — `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp` и общая test fixture data. Docs: save/load Authoring reference CFC-09 и релевантные `AddCommand`/presentation references.

**Инвариант:** [Overview acceptance](../../Architecture/Overview.md#vertical-slice-acceptance), [Lua authoring](../../Architecture/AuthoringSurfaceContract.md), [commands/events](../../Architecture/CommandsAndEvents.md). Native host не знает команды, entity shape и gameplay transitions fixture.

**Сценарий:** package-owned start → bound UI command → service mutation → post-commit event → desired presentation → typed save request → следующая mutation → typed load сохранённого → reconstructed UI → следующая bound command. Добавить одну новую Lua command/service и поле presentation в fixture через существующий authoring surface; production C++ source для этой новой игровой операции не меняется.

**Не считается закрытием:** тест напрямую пишет `game.state`; UI не создаётся; save/load заменён вызовом library helper; ожидаемый hash прочитан из того же запуска и объявлен golden; добавлен C++ handler новой игровой команды.

**Шаги:**
1. Задать независимый fixture: исходные scalar значения, ожидаемые IDs, количество/порядок post-commit facts и значения presentation до save/после load/после продолжения. Hash получить из отдельной явной expected Lua data fixture, не из actual state getter как oracle.
2. Исполнить gameplay portion одной Lua spec в обоих hosts; host control использует fixed save/load API, UI interaction часть — реальный UE binding handle и widget consumer. Общие semantics не дублировать host-specific Lua assertions.
3. Выполнить полную последовательность в UE product composition с реальным filesystem storage и replacement. Проверить geometry/fields/binding resolution реконструированного UI; старый handle отвергается, новый исполняется.
4. Добавить новую command/service только Lua/definitions и повторить сценарий. Source diff/inventory подтверждает отсутствие gameplay-specific изменений native production; test adapter additions не выдаются за native gameplay capability.
5. Выполнить bounded lifecycle stress: 100 повторений save/load/restart с детерминированным порядком, weak-object/VM/operation counts после teardown; это конкретный stress corpus, не универсальная гарантия отсутствия leaks. Синхронизировать authoring examples и переходить к CFC-13.

**Done:**
- Весь сценарий выполнен production UE путём с реальным input, save/load и продолжением команды.
- Expected state/events/IDs определены независимо от actual getters; canonical tree не пересекает boundary.
- После load проверены значения потребителей и bindings, а не только число widgets.
- Shared gameplay semantics совпадают в UE/headless; новая операция реализована Lua-only.
- 100 lifecycle cycles завершаются без живых чужих generations, незавершённых operations и сохранённых native callbacks Lua.

**Evidence:** tier/spec inventory, manifest/package/script hashes, expected fixture, UE step traces и consumer assertions, native production diff, stress counts. Финальная готовность объявляется только CFC-13, после полной приёмки.

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
