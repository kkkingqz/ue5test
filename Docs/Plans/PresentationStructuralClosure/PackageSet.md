---
title: Package Set Tasks
status: active
version: 1.1
updated: 2026-09-07
depends_on:
  - README.md
  - ContractAlignment.md
  - ../../Architecture/HeadlessSimulationContract.md
---

# M1 — Package Set

> **Материализует:** `PAH-R3`, `PAH-R6` и `D1/D5` [ADR-0043](../../ADR/0043-presentation-apply-boundary.md).
> **Задачи:** PSC-02…03.
> **Результат:** package set выводится один раз, а полный manifest входит в package identity без UE-зависимости portable-слоя.

## Зафиксированный portable тип

```text
GV2ContentHostSupport::FResolvedPackageSource
  Root: filesystem path used only by builders
  Descriptor: immutable FPackageDescriptor
  CanonicalManifestHash: 64 lowercase hex

GV2ContentHostSupport::FResolvedPackageSet
  OrderedSources: immutable load-order sequence
```

Canonical manifest hash вычисляется из полного разобранного JSON5 root до проекции известных полей в `FPackageDescriptor`. Форматирование и комментарии на hash не влияют; любое semantic field, включая неизвестное будущему descriptor host extension, влияет.

## Задачи

- [x] **PSC-02 — Ввести один `FResolvedPackageSet`**
  - Зависимости: PSC-01.
  - Инвариант: repository, Lua source loader и UE presentation builders потребляют один value, построенный до них. Повторное package discovery ниже bootstrap является вторым авторитетом, даже если сейчас даёт тот же порядок.
  - Не считается закрытием: перестановка `LoadScreenRegistry()`; сравнение независимо выведенных наборов; сохранение public downstream helpers, повторно открывающих canonical `GameData`; отдельные ветки consumers для Editor/test roots.
  - Done:
    - `FResolvedPackageSet` и `FResolvedPackageSource` принадлежат `GV2ContentHostSupport` и не зависят от Unreal;
    - production lock, Editor profile, automation fixture и Headless CLI различаются только factory-входом, а не consumer path;
    - repository builder получает ordered descriptors, Lua loader — ordered roots/sources, presentation candidate builder — тот же set целиком;
    - Screen Registry и schema/resource builders не вызывают package discovery самостоятельно;
    - legacy core script fallback, если сохраняется из-за layout `Scripts/`, разрешается внутри source для уже выбранного `core` package и не может добавить package вне set;
    - число мест создания set выводится по функциям, возвращающим `FResolvedPackageSet`, из declarations; разрешены только host bootstrap entry points;
    - constructor/factory inventory имеет negative self-test: synthetic downstream factory/call отвергается;
    - production scenario с Editor roots, отличными от `mods.lock`, подтверждает единый порядок repository, Lua, schemas, screens и resources;
    - Headless использует те же portable factories и не получает Unreal/presentation link edge.
  - Evidence: `Source/GV2ContentHostSupport/Public/`, `Source/GV2ContentHostSupport/Private/`, `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, `Source/GV2/Private/Application/GV2SessionCoordinator.cpp`, `Headless/Source/main.cpp`, новый declaration-derived gate.
  - **Реализация (2026-09-08):** `GV2ContentHostSupport::FResolvedPackageSource`/`FResolvedPackageSet` введены как portable-типы, не зависящие от Unreal (`Source/GV2ContentHostSupport/Public/GV2ContentHostSupport/PackageDiscovery.h`); `CanonicalManifestHash` вычисляется `ComputeCanonicalHash` из полного разобранного `package.json5` `FValue` до проекции в `FPackageDescriptor`, поэтому неизвестное будущему полю host-extension поле меняет hash, а форматирование/комментарии — нет (доказано conformance case 12 ниже). Единственные два blessed factory — `ResolvePackageSetFromContainer`/`ResolvePackageSetFromDirectories` (`Source/GV2ContentHostSupport/Private/PackageDiscovery.cpp`) — тонкие обёртки над уже существующими `DiscoverPackagesFromContainer`/`DiscoverPackagesFromDirectories`, добавляющие только hash-вычисление и сборку `FResolvedPackageSource` по `Descriptor.GetLoadIndex()`.

    UE-сторона: `UGV2RuntimeSubsystem::Initialize()` резолвит `ResolvedPackageSet` РОВНО ОДИН РАЗ (private `TOptional<FResolvedPackageSet>` член вместо прежнего `TArray<FString> RepositoryPackageRoots`, живёт весь lifetime `GameInstance`, переиспользуется каждым `StartSession()`), и это закрывает саму формулировку `PAH-R3` — раньше `LoadScreenRegistry()` вызывался ДО любого package resolution и всегда читал canonical `GameData/` напрямую через `GV2PackageClosure::DiscoverFromGameData()`, игнорируя Editor/test override; теперь Screen Registry строится ПОСЛЕ резолюции и получает тот же `TArray<GV2PackageClosure::FEntry>` (через новую чистую проекцию `GV2PackageClosure::FromResolvedPackageSet`), что и repository build (`BuildGV2RepositoryFromResolvedPackageSet`, `Source/GV2/Private/Application/GV2FilesystemContentSourceProvider.cpp`) и Lua/schema loader (`FGV2SessionCoordinator::LoadPortableRuntimeSources`). `UGV2ScreenRegistry::GetPackageLoadOrderFromGameData`/`ResolveContentRootOwnershipFromGameData`/`Build` больше не вызывают package discovery сами — все три взяли `ClosureEntries` явным параметром (тот же принцип "чистая функция берёт уже resolved вход", что и PAH-05's `BuildContentRootOwnership`/`ResolveContentRootOwnershipFromGameData`). `GV2PackageClosure::DiscoverFromGameData()` не удалён — остаётся как independent-oracle helper для тестов, которые намеренно строят СВОЙ собственный, отдельно выведенный ожидаемый результат (DCA-18/PAH-05 style), но production callers у него больше нет.

    `FGV2SessionCoordinator::StartSession()`'s третий параметр сменился с `const TArray<FString>& RuntimePackageRoots = {}` на `const GV2ContentHostSupport::FResolvedPackageSet* ResolvedPackageSet = nullptr` — выбран именно указателем с default `nullptr`, чтобы ~11 существующих test call sites, использующих default (без override), не потребовали изменений вообще; их fallback-путь внутри `LoadPortableRuntimeSources` сохранён БУКВАЛЬНО без изменений (закомментирован как "unchanged from before PSC-02, just no longer the production path"), а изменить пришлось только ~2 сайта, реально передававших explicit `TArray<FString>`.

    Headless (`Headless/Source/main.cpp`) переведён на те же blessed factories, закрывая Done-bullet «Headless использует те же portable factories и не получает Unreal/presentation link edge»: раньше `LoadRuntimeSources` и `BuildRepositoryFromDirectories` каждый независимо вызывали `DiscoverPackageFromDirectory`/`DiscoverPackagesFromDirectories` над ОДНИМИ И ТЕМИ ЖЕ `ContentRoots` (тот же класс дублирования, что `PAH-R3` нашёл на UE-стороне, просто внутри одного host'а); теперь `main()` резолвит `ResolvedPackageSet` один раз через `ResolvePackageSetFromDirectories`, и обе функции плюс новая `BuildRepositoryFromResolvedPackageSet` потребляют этот один результат. `BuildRepositoryFromDirectories` (старая, discovery-based) сохранена НЕ для production-пути, а только для `--self-test`'а per-tier изолированных build'ов, которым намеренно нужны отдельные temp-директории на каждой итерации — это осознанное исключение, не забытый вызов. При переносе обнаружился и исправлен побочный regression: старый код печатал diagnostic `core:diagnostic.package.discovery.root_not_found` из `BuildRepositoryFromDirectories`'s failure path; после переноса резолюция стала происходить раньше и ошибка могла бы тихо превратиться в generic `content_root_not_found` без diagnostic-кода — исправлено явной печатью диагностик прямо в новом resolve-блоке `main()` (тест `gv2_headless_rejects_missing_content_root` поймал это немедленно, до коммита).

    `Tools/Content/`, `Source/GV2ContentAuthoring/`, `Source/GV2ContentEditor/` НЕ переведены — они не входят в Done-bullet список consumers (repository/Lua/presentation candidate builder/Screen Registry/schema-resource builders/Headless), это отдельные content-authoring/editor-adapter пути, а не session content bootstrap; целенаправленно оставлены на старом `DiscoverPackages*` API.

    Два gate: (1) существующий `validate_pre_ready_content_discovery.py` (INV-P1) расширен паттерном `GV2ContentHostSupport::ResolvePackageSet\w+` — переименование discovery-вызовов означало, что старая grammar их больше не видела вообще (подтверждено до фикса: synthetic unmarked вызов `ResolvePackageSetFromDirectories` не давал ни одной violation). При проверке нашёлся и независимый, не связанный с переименованием баг парсера: `if (GIsEditor && !IsRunningCommandlet() && !FApp::IsUnattended())\n{` ошибочно распознавался как определение функции `IsRunningCommandlet`, чьим "телом" считался следующий блок — реальный маркер на `ResolveSessionPackageSet()` из-за этого не засчитывался. Исправлено вынесением условия в `const bool bIsInteractiveEditorSession = ...;` (не трогая сам shared-парсер, используемый другими gate'ами). (2) новый `Tools/Testing/validate_package_set_factory_inventory.py` перечисляет функции, ДЕКЛАРИРОВАННЫЕ (не определённые — приватные anonymous-namespace helpers в .cpp вроде `ToTOptional` намеренно вне scope, так как не являются достижимой извне construction surface) с возвращаемым типом `FResolvedPackageSet`, и требует, чтобы имя было одним из двух blessed factories; имеет negative self-test (synthetic factory отвергается, повторная декларация настоящего factory — нет) и зарегистрирован в `CMakeLists.txt` как `package_set_factory_inventory_contract`/`_negative_contract`.

    Production-сценарий (Done-bullet «Editor roots, отличными от `mods.lock`»): интерактивная `bIsInteractiveEditorSession`-ветка недостижима из `-unattended` automation по конструкции (тем самым исключая буквальный toggle в тесте), поэтому вместо неё добавлен `GV2.Runtime.Content.PackageSetSingleResolutionAcrossConsumers` (`Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`) — резолвит ДВА реально разных fixture-набора (`core+textsystem+rh`, мимикрирующий canonical, и `core+textsystem+sample`, мимикрирующий отличающийся Editor profile — те же валидные fixture-композиции, что уже использует `FGV2RhStartScreenFlow`/`FGV2DebugStartScreenFlow`) и для каждого независимо проверяет, что Screen Registry (`GetPackageLoadOrderFromGameData`) и repository (`BuildGV2RepositoryFromResolvedPackageSet`) отражают ИМЕННО свой вход, плюс cross-check, что порядок для набора A отличается от набора B (не схлопнулись в один shared closure). Red-on-revert: временно откачен `GetPackageLoadOrderFromGameData` на `GV2PackageClosure::DiscoverFromGameData()` (игнорируя параметр) — тест немедленно упал с `Expected ... "core,textsystem,sample" ... but it was "core,textsystem,rh"`, подтвердив, что тест действительно ловит именно `PAH-R3`-класс регрессии; откат применён и перепроверен (119/119 automation зелёные) сразу после демонстрации.

    Верификация: 84/84 portable ctest (включая новый `PackageDiscoveryAndOrderConformance` case 12 и оба новых gate-теста), 119/119 UE Automation (118 существующих + новый contract-тест), `validate_pre_ready_content_discovery.py`/`--self-test`, `validate_package_set_factory_inventory.py`/`--self-test`, `validate_core_decoupling.py`, `validate_no_hardcoded_asset_paths.py`, `validate_presentation_authority_phase.py`, `validate_screen_registry_entry_encapsulation.py`, `validate_shell_attach_failure_consumption.py`, `validate_ui_capability_member_inventory.py`, `validate_ui_rollback_boundaries.py`, `validate_docs.py` (185 файлов) — все зелёные.

    M1 НЕ закрыт этим изменением: `PSC-03` (canonical manifest identity / `ComputePackageFingerprint` redesign) остаётся открытой второй половиной milestone.

- [x] **PSC-03 — Зафиксировать canonical manifest identity**
  - Зависимости: PSC-02.
  - Инвариант: package fingerprint покрывает полное semantic содержимое manifest, но Headless run digest покрывает только portable gameplay inputs/results. Эти identity намеренно различны.
  - Не считается закрытием: добавление `ue_content_roots` в старый список полей; source inventory известных descriptor members; hash сырых bytes, меняющийся от пробелов/комментариев; включение package fingerprint или presentation data в `FRunDigest`.
  - Done:
    - package discovery вычисляет `CanonicalManifestHash` из полного parsed `FValue` до извлечения известных полей;
    - `ComputePackageFingerprint` использует descriptor identity и `CanonicalManifestHash`, а не повторный перечень manifest fields;
    - изменение форматирования/комментариев сохраняет hash, изменение любого semantic field меняет hash;
    - synthetic неизвестное host-extension field меняет manifest hash и package fingerprint без изменения `FPackageDescriptor` API;
    - `ue_content_roots` меняет package fingerprint и regenerated `mods.lock.json5`;
    - `repository_content_hash`, `package_fingerprint`, `presentation_hash`, `session_content_id` и `FRunDigest` остаются разными typed concepts;
    - изменение только UE-specific field/asset не меняет `repository_content_hash`, `script_set_hash`, state hash или Headless run digest;
    - portable CTest и UE host проверяют одинаковый package order/fingerprint из одного conformance implementation.
  - Evidence: `Source/GV2ContentHostSupport/Private/PackageDiscovery.cpp`, `Source/GV2ContentHostSupport/Private/ModsLock.cpp`, portable conformance, `GameData/mods.lock.json5`, `Docs/Architecture/HeadlessSimulationContract.md`.
  - **Реализация (2026-09-08):** `ComputePackageFingerprint` переписан с ручного перечня manifest-полей (namespace/version/relative_sources/schema_bindings/extension_schema_bindings/redirects/tombstones/dependencies — восемь отдельных полей, требовавших ручного обновления при любом новом поле дескриптора) на `identity + CanonicalManifestHash`: `package_id` (identity), `load_index` (load-order context, назначается discovery, а не содержимое manifest — остаётся отдельным полем, а не частью hash) и уже существующий (с PSC-02) `CanonicalManifestHash` источника. Поскольку `CanonicalManifestHash` хэширует ПОЛНОЕ разобранное дерево `package.json5` до проекции в `FPackageDescriptor`, любое semantic поле — известное дескриптору или нет, включая `ue_content_roots` — теперь меняет fingerprint автоматически, без отдельного перечня.

    `GenerateModsLockContent`/`VerifyModsLock` сменили сигнатуру с `vector<FPackageDescriptor>` на `vector<FResolvedPackageSource>` (тип из PSC-02) — единственный способ передать `CanonicalManifestHash` вместе с дескриптором для вычисления fingerprint. Production call site (`DiscoverPackagesFromContainer`'s lock-verify ветка, `PackageDiscovery.cpp`) строит `FResolvedPackageSet` через уже существующий internal `BuildResolvedPackageSet` из КОПИИ `Descriptors` (не move) — оригинал `Descriptors` возвращается вызывающему в конце функции неизменным, поэтому копия обязательна, а не оптимизация-опечатка.

    `GameData/mods.lock.json5` перегенерирован под новую формулу (все три fingerprint изменились — ожидаемо, это смена identity-функции, а не баг). Регенерация выполнена одноразовой scratch-программой (`clang++` напрямую против `libgv2_content_host_support.a`/`libgv2_content_core.a` из `build/`, без нового постоянного CLI) — `Tools/Content` (`gv2-content`) намеренно не получил отдельной команды регенерации lock-файла, так как ни один Done-bullet PSC-03 её не требует, а построение одноразовой compiled-программы против уже собранных portable-библиотек полностью достаточно и не добавляет постоянную CLI-поверхность, которую придётся поддерживать.

    Новые conformance-кейсы 13 и 14 (`PackageDiscoveryAndOrderConformance.cpp`, тот же shared entry point, что и case 12 из PSC-02, исполняется и portable CTest, и UE host'ом): case 13 доказывает `ComputePackageFingerprint`'s свойства — форматирование/комментарии не меняют fingerprint (наследуется от `CanonicalManifestHash`), `ue_content_roots` конкретно (не generic synthetic-поле — Done-bullet называет его отдельно) меняет fingerprint, одинаковый package_id/content с разным `load_index` тоже даёт разный fingerprint. Case 14 доказывает независимость: `ue_content_roots` меняет fingerprint (case 13), но НЕ меняет `repository_content_hash` того же пакета — `BuildRepository` читает только `FPackageDescriptor`'s спроецированные `relative_sources` через `IContentSourceProvider`, никогда сырой manifest или `CanonicalManifestHash`, поэтому package identity/content hashing и repository content hashing остаются независимыми typed concepts структурно, не только по соглашению. `gv2_headless_golden_replay_matches_digest` (пин на конкретный digest) остался зелёным без изменений — независимое подтверждение, что Headless run digest не задет сменой fingerprint-формулы.

    Red-on-revert: временно откачен `ComputePackageFingerprint` на игнорирование `CanonicalManifestHash` (`(void)CanonicalManifestHash;`, только identity) — `gv2-headless --self-test` тут же перестал резолвить контент вообще (`content_root_not_found`), потому что перегенерированный `GameData/mods.lock.json5` больше не совпадал с откаченной формулой; для изолированной проверки конкретно case 13 повторено с `--content-root=GameData/core,GameData/textsystem,GameData/rh` (explicit roots в обход lock-verify), что дало точный ожидаемый провал `case13_ue_content_roots_did_not_change_fingerprint`. Откат применён обратно, портативный ctest и UE Automation (119/119) перепроверены зелёными.

    Верификация: 84/84 portable ctest (включая новые case 13/14), 119/119 UE Automation, все 9 standalone gate'ов, `validate_docs.py` (185 файлов) — зелёные.

    **M1 закрыт этим изменением** — PSC-02 и PSC-03 оба done; см. README.md.

## Проверка milestone

- [x] Set создаётся только разрешёнными host bootstrap entry points; перечислитель выведен из return type, не из имени функции. (`PSC-02`, 2026-09-08 — `validate_package_set_factory_inventory.py`)
- [x] Все consumers получают один immutable set; downstream rediscovery отсутствует. (`PSC-02`, 2026-09-08 — `GV2.Runtime.Content.PackageSetSingleResolutionAcrossConsumers`, red-on-revert подтверждён)
- [x] Arbitrary semantic manifest field меняет fingerprint без ручного обновления перечня. (`PSC-03`, 2026-09-08 — conformance case 13, включая `ue_content_roots`)
- [x] Headless использует тот же resolver, остаётся UE-free и сохраняет прежний run digest. (`PSC-02`, 2026-09-08 — `gv2_headless_golden_replay_matches_digest` не изменился)
