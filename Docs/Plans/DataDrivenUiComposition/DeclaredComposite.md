---
title: Declared Composite Tasks
status: active
version: 1.4
updated: 2026-09-01
depends_on:
  - README.md
  - AddressableElements.md
  - ../../UI/ScreenTemplates.md
---

# M2 — Declared Composite

> **Материализует:** барьер «объявление capability недоступно из Blueprint».
> **Задачи:** DUC-05…08.
> **Результат:** блок из существующих элементов собирается ассетом и объявлением, без C++-класса.

## Результат этапа

Композит локации сегодня — это C++-класс, чей `DescribeUiCapabilities` состоит из вызовов вида `AddText("day", "DayText")`: имя свойства, имя дочернего виджета, вид. Ничего, кроме этих троек, там нет.

Этап переносит эти тройки из кода в Designer-свойство generic-композита. Ключевое требование — **объявление остаётся контрактом, а не описанием реализации**: оно проверяется против того, что дочерние виджеты действительно умеют, и расхождение является ошибкой. Иначе повторится `UPP-R1`, где ожидание выводилось из потребителя и потому не могло разойтись с ним.

## Задачи

- [x] **DUC-05 — Generic-композит и объявляемый список capability**
  - Зависимости: DUC-04, `PCC-05` плана доведения.
  - Done: существует generic-класс композита, чьё дерево capability строится из `UPROPERTY(EditAnywhere)` списка троек *(имя свойства, имя дочернего виджета, вид)*; список редактируется в Designer рядом с деревом виджетов; вид поддерживает все значения, у которых есть consumer после `PCC-05`; ссылка на несуществующее имя дочернего виджета отклоняется на проверке экземпляра до публикации экрана, а не молча пропускается; ни один существующий композит на этом шаге ещё не переписан.
  - Evidence: `UGV2DeclaredCompositeWidgetBase`, `WBP_DeclaredCompositeFixture`, `GV2.UI.DeclaredComposite`, `GV2.UI.CapabilityObservabilityCompositeSweep`; red→green sweep доказывает, что real WBP fixture обязателен для нового native host.

- [x] **DUC-06 — Схема объявляемого композита плоская**
  - Зависимости: DUC-05.
  - Автоматическое разворачивание дерева ребёнка дало бы автору `{ label: { text, key }, value: { percent, label, key } }` с протечкой служебных свойств на каждый уровень.
  - Done: объявленное свойство композита отображается в **одно** поле схемы соответствующего вида; автор пишет `day: TextSpec`, а не `day.text`; форма зафиксирована в [Screen Templates](../../UI/ScreenTemplates.md) с примером блока из двух элементов; тест сверяет форму схемы, порождаемой объявлением, с формой, которую ожидает автор контента.
  - Evidence: `textsystem:schema.ui_field.declared_composite_fixture.v1`, `WBP_DeclaredCompositeFlatFixture`, `GV2.UI.DeclaredComposite.FlatSchema`; red→green доказывает, что content schema и независимое Designer declaration дают ровно `day: text`, `value: number`.
  - **Реализация (2026-09-01):**
    - Механизм уже был плоским по построению: `UGV2DeclaredCompositeWidgetBase::DescribeUiCapabilities` (DUC-05) проецирует каждую тройку `DeclaredCapabilities` в ровно одну top-level capability, без рекурсивного раскрытия ребёнка. DUC-06 доказывает это независимым вторым источником — content schema, а не кодом, который порождает capability tree.
    - `WBP_DeclaredCompositeFlatFixture` (day → DayText: Text, value → ValueBar: Number) и `textsystem:schema.ui_field.declared_composite_fixture.v1` (`day: text`, `value: number`) добавлены как отдельная fixture, не смешанная с production `WBP_DeclaredCompositeFixture` (label → LabelText) из DUC-05. `GV2.UI.DeclaredComposite.FlatSchema` грузит schema через `FGV2UiSchemaCache` и capability tree через живой Designer-класс независимо друг от друга, проверяет отсутствие протёкших `day.text`/`value.percent`, и сверяет обе стороны через `CheckUiSchemaCapabilityCompatibility`.
    - **Найденный и устранённый живой дефект (интеграционный, не схемы)**: `CaptureUiTargetState` не читал `UGV2ProgressBarWidgetBase::GetProgress()` — только сырой `UProgressBar::GetPercent()` — поэтому два разных значения, применённые к `value` (ValueBar), давали одинаковый снимок и `GV2.UI.CapabilityObservabilityCompositeSweep` красил `value` как ненаблюдаемую. Добавлен генерик-блок, читающий adapter state напрямую; откат (удаление блока) подтверждён красным на той же ошибке `capability 'value' is not observable`, восстановление — снова зелёный.
    - **Найдена и устранена рассинхронизация `GameData/mods.lock.json5`**: добавление новой schema под `GameData/textsystem/schemas/` меняет fingerprint пакета `textsystem` (он выводится из полного набора discovered `schema_bindings`/`relative_sources`, а не только из явно перечисленного в `package.json5`), а lock-файл не был перевыпущен — из-за этого `UGV2RuntimeSubsystem::Initialize` не мог собрать `GameDataRepository` (`failed to build the initial GameDataRepository`), что красило практически весь `GV2.*` набор (не только DUC-06-специфичные тесты) в headless Editor automation, хотя portable `ctest` вскрывал точную причину (`core:diagnostic.package.lock.mismatch`). Исправлено пересчётом fingerprint для `textsystem` (значения `core`/`rh` не изменились).
    - **Найдены и устранены два побочных, не-DUC-06 регресса**, обнажившихся только после починки `mods.lock.json5` (до этого весь `GV2.*` набор красился одной и той же причиной):
      1. `GV2.Runtime.UI.ScreenPreflightPredictsDeepChildFailure` строил `CommandPanel`-кнопки из голого `UGV2ButtonWidgetBase::StaticClass()` (без Designer-дерева, значит без ребёнка `LabelText`) — DUC-05's `GV2UiMutationPlan.cpp` сделал отсутствие именованного target'а детерминированным отказом preflight вместо тихого fallback на сам host, поэтому даже "валидный" apply теперь корректно отклонялся. Тест переведён на реальный `WBP_Button` (`LoadClass<UGV2ButtonWidgetBase>(..., "/Game/UI/Widgets/WBP_Button.WBP_Button_C")`), как уже делают соседние тесты в этом же файле.
      2. `GV2.Runtime.UIKit.CentralThemeAndComponents` ссылался на `core:text.screen.test.*` id литералами, ожидая реальный контент в `DA_UITheme_Default.TextCatalog`; отдельная (ранее вынесенная как follow-up) задача корректно переименовала эти записи каталога в `sample:`-namespace, устраняя настоящий namespace drift, но не обновила тест. Поскольку `Source/GV2` не может ссылаться на `sample:`-id литералом (`core_decoupling_gate_contract`), тест переведён на синтетические `core:`-fixture, регистрируемые прямо в тесте через `Theme->TextCatalog.Add(...)` (тот же паттерн, что уже применялся для `FallbackTextCatalog` чуть ниже) — тест проверяет механику text pipeline (аргументы, style token, экранирование), а не реальный контент. Та же задача добавила два новых production WBP (композитные fixture DUC-05/06) под `/Game/TextSystem/UI`, что сдвинуло аудит `WidgetBlueprintCount` с 28 на 30, и дала `WBP_Modal` реальные `TitleText`/`ContentText` (DUC-04) — оба изменения корректны, но не были отражены в hardcoded audit-инвариантах теста; счётчик обновлён, `UGV2ModalWidgetBase` добавлен в allowlist "text-bearing WBP must use a Text Pipeline native base" рядом с остальными composite-хостами (`UGV2LocationTopBarWidgetBase` и т.д.), которые по той же причине легитимно содержат вложенный текстовый примитив не напрямую, а через собственных text-pipeline детей.
    - Верификация: 100/100 UE Automation (headless, `-nullrhi`), 68/68 Headless ctest (включая `core_decoupling_gate_contract`), все content/doc-гейты зелёные.

- [x] **DUC-07 — Объявление проверяется против умений детей**
  - Зависимости: DUC-06, `PCC-02` и `PCC-03` плана доведения.
  - Это то место, где план может воспроизвести `UPP-R1`. Если дерево capability композита выводить из того, что умеют дети, проверка станет истинной по построению.
  - Done: объявление composite-свойства сверяется с capability дочернего виджета как **независимый** источник — объявленный вид обязан быть совместим с тем, что ребёнок действительно принимает, и несовместимость отклоняется до `Ready` с различимым кодом; тест фиксирует, что обе стороны сравнения происходят из разных источников; отрицательный случай: объявить `number` на дочернем текстовом блоке невозможно; capability композита проходят sweep наблюдаемости наравне с C++-объявленными.
  - Evidence: `Source/GV2/Private/UI/GV2UiCapability.cpp`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-09-01):**
    - Новая функция `DoesCapabilityTreeSupportKind` (`GV2UiCapability.h/.cpp`) — принимает уже построенное дерево capability ребёнка (не выводит его сама) и проверяет, что среди его записей есть хотя бы одна с тем же `SupportedKind`, что declared `Kind` composite-свойства. Подключена в `PrepareUiHostProperties` (`GV2UiMutationPlan.cpp`) во всех трёх местах разрешения target (apply-с-значением, reset-без-значения, reset-property-больше-не-в-schema): если именованный target сам реализует `IGV2UiPropertyHost`, его собственный `DescribeUiCapabilities()` вызывается напрямую и сверяется — второй источник, независимый от того, что объявила composite. Несовпадение отклоняется до `Ready` (тот же preflight-гейт, что уже отклоняет `missing_target`/`unsupported_kind`) новым кодом `core:diagnostic.ui_consumer.target_kind_mismatch`.
    - Проверка ограничена `Cap.TargetType == RendererControl`. Первая попытка без этого ограничения покраснила 4 существующих теста (`RhStartOpensLocationScreen`, `LocationScreenTransitionContract`, `ScreenPreflightPredictsDeepChildFailure`, `StandardPropertyConsumers`) — все через `CollectionHost`-таргеты (`ButtonRepeater` и аналоги): репитер тоже `IGV2UiPropertyHost` ради общей `PropertyHostState`/`HostIdentity` поверхности (DUC-04), но не обязан самообъявлять capability того же смысла, которым его адресует родительская коллекция. Сужение до `RendererControl` вернуло все 4 теста в зелёное состояние без ослабления самой проверки для её реального назначения — прямых значений (`Text`/`Number`/`Key`/…), где именованный ребёнок — это WBP-обёртка конкретного значения, а не generic-контейнер.
    - Новый тест `GV2.UI.DeclaredComposite.ChildKindCompatibility`: транзиентный `UGV2DeclaredCompositeWidgetBase` с реальным `WidgetTree` и настоящим native-child `UGV2TextWidgetBase` ("DayText", чья `DescribeUiCapabilities` авторски объявляет только `Text` — независимо от того, что впишет тест). Отрицательный случай — `DeclaredCapabilities = [{day, DayText, Number}]` с содержательно совместимой schema (`day: number`, чтобы шаг 1 `Schema ⊆ Capabilities` прошёл и красноту дал именно шаг 2) — `PrepareUiHostProperties` отклоняет с `target_kind_mismatch`. Положительный случай меняет только `Kind` на `Text` на том же ребёнке — принимается без диагностик, доказывая, что обе стороны читаются независимо, а не выводятся друг из друга.
    - Sweep `GV2.UI.CapabilityObservabilityCompositeSweep` уже (с DUC-04/06) прогоняет capability объявляемого composite наравне с C++-объявленными — DUC-07 не добавляет отдельный механизм для этого пункта Done, только подтверждает регрессионным прогоном, что sweep остался зелёным после нового кода.
    - Красный тест на откате подтверждён дважды: (1) сначала `&& false` на guard-условии — компилятор отверг как `-Werror,-Wunreachable-code`, поэтому откат сделан на уровне функции (`DoesCapabilityTreeSupportKind` временно всегда `true`); (2) полный прогон дал ровно один красный тест (`ChildKindCompatibility`, обе новых assertion, без каскада на другие 100 тестов) — восстановление функции вернуло 101/101.
    - Документация: `Docs/UI/ScreenTemplates.md` получил раздел «Объявление против capability ребёнка (DUC-07)»; `Docs/UI/WidgetRegistry.md` — абзац с кодом диагностики и границей `RendererControl`-only.
    - Верификация: 101/101 UE Automation, 68/68 Headless ctest, все content/doc-гейты зелёные. (Один изолированный прогон дал случайный `NestedInstancesAndTabsContract` fail на `LogModelContextProtocol: Call to unknown method "server/discover"` — MCP-шум без единого `Expected`-assertion; повторный прогон сразу же чистый 101/101, не воспроизводится.)

- [ ] **DUC-08 — Один композит локации переписан на объявление**
  - Зависимости: DUC-07.
  - Доказательство, что механизм покрывает реальный случай, а не только синтетический.
  - Done: `UGV2LocationTopBarWidgetBase` — простейший из четырёх, три текстовых свойства и идентичность — заменён generic-композитом с объявлением; его C++-класс **удалён**, а не оставлен рядом; экран локации применяется без изменений в поведении; automation и Lua-спеки зелёные; в отчёте задачи записано, что из оставшихся трёх композитов выражается объявлением, а что требует C++ и почему.
  - Evidence: `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`, `Content/TextSystem/UI/Widgets/`, отчёт change set.

## Проверка milestone

- [x] Блок из двух существующих элементов собран без C++.
- [x] Схема блока плоская.
- [x] Объявление и умения детей — независимые источники сравнения.
- [ ] Один реальный композит переписан, его класс удалён.
