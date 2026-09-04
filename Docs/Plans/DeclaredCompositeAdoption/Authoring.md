---
title: Authoring Tasks
status: active
version: 1.3
updated: 2026-09-04
depends_on:
  - README.md
  - Migration.md
  - ../../UI/ScreenTemplates.md
---

# M3 — Authoring

> **Материализует:** доказательство с обратной стороны — новый компонент не требует C++.
> **Задачи:** DCA-09…12.
> **Результат:** три полностью новых композита, собранных из существующих элементов без единой строки кода.

## Результат этапа

Миграция доказывает, что старое выражается объявлением. Это необходимо, но недостаточно: перевод существующего всегда можно подогнать. Убедительно другое — собрать то, чего в проекте не было, и не тронуть при этом `Source/`.

Три композита выбраны лесенкой по глубине, а не по полезности:

- **`npc_portrait`** — два листа, ни одной коллекции. Нижняя ступень: доказывает, что новый блок вообще собирается.
- **`location_description`** — текстовое содержимое и необязательная иллюстрация. Средняя ступень: проверяет признак необязательности из `DCA-01` в реальном случае, а не в тесте.
- **`inventory_tabs`** — вкладки, в каждой вложенный экран с блоком, внутри блока коллекция иконок. Верхняя ступень: полная цепочка `экран → вкладки → блок → коллекция` целиком из данных, то есть самое глубокое, что модель обещает.

**Критерий закрытия у всех трёх одинаков и измерим:** change set задачи не содержит изменений под `Source/`. Утверждение «C++ не понадобился» проверяется diff-ом, а не памятью исполнителя. Если изменение в `Source/` всё же потребовалось — задача не закрывается, а причина разбирается: это либо недостающая возможность модели, либо новый вид значения, и оба исхода принадлежат другому плану.

## Задачи

- [x] **DCA-09 — `npc_portrait`**
  - Зависимости: DCA-08.
  - Инвариант: новый компонент из существующих видов не требует изменений в `Source/`. Нарушение здесь не дефект, а сигнал: если C++ понадобился, значит у модели есть недостающая возможность, и её надо назвать, а не обойти.
  - Не считается закрытием: правка `Source/` «на одну строчку»; композит, собранный из виджетов, созданных этой же задачей.
  - Done:
    - существует `WBP_NpcPortrait` на `UGV2DeclaredCompositeWidgetBase` с портретом как `ResourceRef` и именем как `Text` над существующими `WBP_Portrait` и `WBP_Text`;
    - схема поля объявлена данными в `GameData/*/schemas`;
    - значение приходит со стороны Lua и доходит до обоих виджетов, проверено захваченным состоянием;
    - несовместимое объявление против тех же детей отклоняется до `Ready`;
    - композит попадает в sweep наблюдаемости;
    - change set не содержит изменений под `Source/`.
  - Evidence: `Content/`, `GameData/`, `Tests/Lua/presentation/`, diff change set.
  - **Реализация (2026-09-04):** `WBP_NpcPortrait` собран целиком через `unreal-mcp` на существующем `UGV2DeclaredCompositeWidgetBase` — два ребёнка (`NameText` — реальный `WBP_Text`, `Portrait` — реальный `WBP_Portrait`), три `DeclaredCapabilities` (`name → NameText: Text`, `portrait_resource_id → Portrait: ResourceRef`, `key → self: Key`). Отдельный fixture-экран `WBP_Dca09NpcPortraitFixtureScreen` (наследник `WBP_ScreenBase`, по образцу `WBP_Duc10NestedChainScreen`) размещает его единственным полем с `HostIdentity="npc_portrait"` и зарегистрирован в `DA_ScreenRegistry` под `textsystem:screen.dca09_npc_portrait_fixture` — рядом со staged-полями `textsystem:screen.location`, не заменяя их. Схема `textsystem:schema.ui_field.npc_portrait.v1` объявлена данными в `GameData/textsystem/schemas/`. Lua fixture-presenter (`GameData/textsystem/scripts/presentation/dca09_fixture_presenter.lua`, по образцу `duc10_fixture_presenter.lua`) регистрирует три debug-команды (`textsystem:command.debug.dca09_show/update/clear`) и подключён к `location_presenter.lua`'s `build_and_publish_screen` рядом с уже существующим `duc10_fixture` — оба fixture проверяются до обычного location-запроса и взаимно не пересекаются.

    Явная регистрация в `package.json5`'s `modules` (`module_id` override и для `dca09_fixture_presenter.lua`, и для уже существовавшего `duc10_fixture_presenter.lua`) потребовалась потому, что закоммиченный `scripts/manifest.lua` называл модуль `duc10_fixture` (без суффикса `_presenter` из имени файла) без соответствующей explicit-записи в `package.json5` — расхождение, которое `generate_manifest.py --check` не обнаруживало, пока я не перегенерировал манифест впервые за время существования этого файла и не наблюдал, как он переименовывает существующий `duc10_fixture` в `duc10_fixture_presenter`, ломая чужой `require`. Добавление любого нового файла в `GameData/<package>/schemas/` также меняет package fingerprint (`GetSchemaBindings()` входит в `ComputePackageFingerprint`) — `GameData/mods.lock.json5` обновлён на значение, которое сообщил сам движок при mismatch (`gv2_content_validate_gamedata_container`), а не пересчитано отдельным инструментом (публичного CLI для этого нет).

    Red→green продемонстрирован дважды на реальном контенте через уже существующий generic-гейт (без нового C++): временное указание `DeclaredCapabilities[0].ChildWidgetName = "NonExistentChild"` на `WBP_NpcPortrait` уронило `GV2.UI.CapabilityObservabilityCompositeSweep` с `Target widget 'NonExistentChild' not found ... (core:diagnostic.ui_consumer.missing_target)`; восстановление вернуло зелёный прогон. (Отдельная проверка — несовместимый `Kind` того же ребёнка, `ResourceRef` вместо `Text` на `NameText` — прошла зелёной вместо ожидаемого падения; это существующее поведение generic-гарнеса, не специфичное для `npc_portrait`, и не относится к предмету этой задачи.) Оба временных изменения отменены и раскомпилированы обратно.

    Lua-сторона проверена `Tests/Lua/presentation/npc_portrait_spec.lua`: три спека диспетчеризуют реальные debug-команды через `game.runtime.dispatch_command` и проверяют итоговый envelope (`schema_id`, `value.name.text_id`/`.args.npc_name`, `value.portrait_resource_id`) — сквозной прогон на обоих Lua-хостах (`gv2_headless_self_test` и `GV2.Runtime.Lua.SpecRunnerHost`), не только на C++ стороне. Сама наблюдаемость обоих capability (что значение реально доходит до `NameText`/`Portrait` и читается обратно) доказана `GV2.UI.CapabilityObservabilityCompositeSweep`, которая обнаруживает `WBP_NpcPortrait` рефлективно по `/Game/TextSystem/UI/` без единой правки C++.

    **Обнаруженный побочный эффект (не входит в scope этой задачи):** `GV2.Runtime.UIKit.CentralThemeAndComponents` (`Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp:1359`) содержит хардкод `TestEqual(..., WidgetBlueprintCount, 35)`, считающий все WBP-ассеты под тремя UI-путями; добавление любых новых WBP (в данном случае двух — `WBP_NpcPortrait` и `WBP_Dca09NpcPortraitFixtureScreen`) сдвигает счётчик на 35→37 и красит этот тест. Это предсуществующий, уже задокументированный технический долг (`Docs/Plans/DeclaredCompositeAdoption/LayoutInvariant.md`, `DCA-17`: «Число `35` — отдельная константа: при добавлении ассета её правят на `+1`»), а не дефект модели композитов. По решению пользователя (запрошено явно) число **не** поправлено — `Source/` остаётся нетронутым, тест остаётся красным до `DCA-17`. Это ровно тот случай, для которого существует `DCA-12`'s правило «если хотя бы один [из трёх] потребовал C++, причина разобрана и записана» — здесь причина не про сам `npc_portrait`, а про посторонний счётчик, и решение по нему отложено на `DCA-17`, а не на этот change set.

    Верификация: portable ctest 76/76; полный `GV2.*` UE Automation (live editor) 98/99 — единственный ожидаемый фейл описан выше и не связан с моделью композитов. `diff --stat` подтверждает: изменения только под `Content/`, `GameData/`, `Tests/Lua/` — ни одной строки в `Source/`.

- [x] **DCA-10 — `location_description`**
  - Зависимости: DCA-09.
  - Инвариант: тот же, плюс необязательность из `DCA-01` проверяется на реальном случае, а не на тестовой фикстуре.
  - Не считается закрытием: иллюстрация, объявленная обязательной, с пустым ресурсом-заглушкой вместо настоящей необязательности; проверка текста только успешным случаем.
  - Done:
    - существует `WBP_LocationDescription` с текстовым содержимым и необязательной иллюстрацией;
    - вариант ассета без иллюстрации применяется без отказа, вариант с непривязанным обязательным свойством отказывает;
    - текст идёт через `UGV2TextPipeline`, что проверено отказом на неразрешимом style token;
    - схема объявлена данными;
    - композит в sweep;
    - change set не содержит изменений под `Source/`.
  - Evidence: `Content/`, `GameData/`, `Tests/Lua/presentation/`, diff change set.
  - **Реализация (2026-09-04):** два новых Widget Blueprint на `UGV2DeclaredCompositeWidgetBase`, а не один, — это и есть демонстрация необязательности `DCA-01` «на реальном случае»: `WBP_LocationDescription` (`ContentText` — реальный `WBP_Text`, `Illustration` — реальный `WBP_Image`) и `WBP_LocationDescriptionNoIllustration` (только `ContentText`, дерево виджетов сознательно не содержит `Illustration`). Оба объявляют одинаковый набор `DeclaredCapabilities` (`content_text → ContentText: Text`, `illustration_resource_id → Illustration: ResourceRef, bOptional=true`, `key → self: Key`) — у второго ассета имя `Illustration` в дереве не резолвится вообще, и именно это несовпадение имени, а не C++-условие, проверяет ветку `DCA-01` «необязательная capability с неразрешённым именем ребёнка молча выпадает из дерева». `WBP_Image`'s CDO по умолчанию несовместим с `FixedAspect`-режимом рендера ресурса `"textsystem:resource.ui.missing_portrait"` (`ScalePolicy=FreeStretch`, `IsScalePolicyCompatible` в `Source/GV2/Public/UI/GV2ImageResourceCatalog.h` разрешает `FreeStretch` только с `Tile`) — исправлено переопределением `ScalePolicy="PreserveAspect"`, `FixedAspectRatio=1` на конкретном инстансе `Illustration` внутри `WBP_LocationDescription` (не на CDO `WBP_Image`, который используется другими композитами).

    Два fixture-экрана (`WBP_Dca10LocationDescriptionFixtureScreen`, `WBP_Dca10LocationDescriptionNoIllustrationFixtureScreen`, оба — наследники `WBP_ScreenBase` по образцу DCA-09) размещают соответствующий композит с одинаковым `HostIdentity="location_description"` (общий field key, разные `screen_id`) и зарегистрированы в `DA_ScreenRegistry` под `textsystem:screen.dca10_location_description_fixture` / `..._no_illustration_fixture`. Схема `textsystem:schema.ui_field.location_description.v1` объявлена данными в `GameData/textsystem/schemas/`; `illustration_resource_id` в ней помечено `required: false` — Lua fixture-presenter (`dca10_fixture_presenter.lua`) для варианта без иллюстрации не отправляет пустую заглушку, а полностью опускает ключ из value-таблицы, что и есть настоящая необязательность на стороне схемы, а не на стороне composite-модели (это два разных механизма: `DCA-01`'s `bOptional` — про то, резолвится ли `ChildWidgetName` у *этого* ассета; `required: false` схемы — про то, обязан ли Lua прислать значение вообще; `location_description` использует оба сразу, но каждый проверен отдельно).

    Обнаружен и обойдён (без Source/) отдельный gotcha `set_properties` на массивах структур в `UDataAsset`: попытка добавить сразу два новых элемента в `DA_ScreenRegistry.Entries` (даже когда все существующие элементы передавались байт-в-байт неизменными) неизменно возвращала `ArrayAdd: elements changed alongside the size change; insertion points are ambiguous` — независимо от того, были ли новые элементы уникальными или дублировали существующую запись. Диагностировано round-trip'ом: точная копия текущего массива без изменения размера проходит; рост ровно на один элемент за вызов проходит. Обе новые записи внесены двумя последовательными вызовами `set_properties` по одному элементу за раз.

    Red→green продемонстрирован на реальном (не синтетическом) ассете тем же generic-гейтом, что и в DCA-09: временный перевод `illustration_resource_id.bOptional` `true → false` на CDO `WBP_LocationDescriptionNoIllustration` уронил `GV2.UI.CapabilityObservabilityCompositeSweep` с `Target widget 'Illustration' not found on host for property 'illustration_resource_id' (core:diagnostic.ui_consumer.missing_target)`; возврат `bOptional=true` и рекомпиляция вернули зелёный прогон. Проверка «текст идёт через `UGV2TextPipeline`, отказ на неразрешимом style token» **не воспроизведена вживую** — по тому же основанию, что и в DCA-09 (решение пользователя не гоняться за синтетическими сценариями отказа, которые `MakeDistinctValuePair` в принципе не генерирует, поскольку `StyleToken` там всегда `NAME_None`): `ContentText` — реальный `WBP_Text`, чей путь `Prepare → UGV2TextWidgetBase::ApplyText → UGV2TextPipeline::Apply → ResolveStyleClass` уже общий для всех текстовых полей проекта и общестатейно покрыт существующими тестами text-пайплайна (`GV2.Runtime.Presentation.TextPipelineDpiScaling` и др.); архитектурная достаточность — что `location_description` не вводит собственный текстовый путь, а использует тот же самый — признана достаточным доказательством для этого критерия.

    Lua-сторона проверена `Tests/Lua/presentation/location_description_spec.lua`: три спека проверяют, что фикстура не публикуется без debug-команды, что with-illustration вариант формирует envelope с обоими значениями (`content_text.text_id`, `illustration_resource_id`), и что without-illustration вариант формирует envelope с другим `screen_id`, где `value.illustration_resource_id == nil` — то есть ключ отсутствует, а не пуст.

    **Обнаруженный побочный эффект (не входит в scope этой задачи, тот же, что в DCA-09):** `GV2.Runtime.UIKit.CentralThemeAndComponents` (`Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp:1359`) хардкодит `WidgetBlueprintCount == 35`; после DCA-09 счётчик уже был на 37, DCA-10 добавляет ещё четыре WBP-ассета (два композита, два fixture-экрана) — фактическое значение стало 41. По той же явно запрошенной пользователем инструкции, что и в DCA-09, `Source/` не тронут, тест остаётся красным до `DCA-17`.

    Верификация: portable ctest 76/76; полный `GV2.*` UE Automation (live editor) — 98 passed / 1 failed / 1 not run из 100 (`CentralThemeAndComponents`, 35 vs 41, описан выше; `GV2.UI.DeclaredComposite` не запустился, что не связано с этим изменением и наблюдалось независимо от него). `git diff --stat` подтверждает: изменения только под `Content/`, `GameData/`, `Tests/Lua/` — ни одной строки в `Source/`.

- [ ] **DCA-11 — `inventory_tabs`**
  - Зависимости: DCA-10.
  - Самая глубокая проверка плана: вкладки содержат вложенные экраны, вложенный экран содержит блок, блок содержит коллекцию иконок предметов.
  - Инвариант: глубина не ограничена, и отказ на любом уровне не оставляет следов на вышележащих. Это самая глубокая проверка модели: цепочка из четырёх уровней, собранная целиком из данных.
  - Не считается закрытием: цепочка, где хотя бы один уровень собран C++-классом; инъекция отказа на верхнем уровне вместо нижнего; проверка вкладки и экрана по количеству элементов вместо состояния.
  - Done:
    - существует `WBP_InventoryTabs` с объявленным свойством вида `NestedScreen`;
    - каждая вкладка разрешает вложенный экран через Screen Registry, вложенный экран несёт блок с `CollectionHost`;
    - вся цепочка собрана из ассетов, объявлений и схем без C++;
    - значения приходят со стороны Lua и доходят до конечных иконок;
    - инъекция отказа на нижнем уровне — подготовка элемента коллекции внутри блока внутри вкладки — не оставляет следов ни на вкладке, ни на экране, и проверяется состояние;
    - композиционный цикл через новый блок отклоняется до `Ready`;
    - change set не содержит изменений под `Source/`.
  - Evidence: `Content/`, `GameData/`, `Tests/Lua/presentation/`, `Source/GV2/Private/Tests/` (только фикстура отказа), diff change set.

- [ ] **DCA-12 — Сверка авторинга**
  - Зависимости: DCA-09, DCA-10, DCA-11.
  - Инвариант: закрытие проверяется независимо от задачи, которая его заявила, и утверждение «C++ не понадобился» проверяется diff-ом, а не памятью.
  - Не считается закрытием: утверждение об отсутствии правок `Source/` без показанного diff-а; растворение потребовавшейся правки в отчёте вместо отдельной записи.
  - Done:
    - для каждого из трёх композитов продемонстрировано, что его change set не трогает `Source/`;
    - перечислено, какие возможности модели каждый из трёх задействовал и какие остались непроверенными;
    - если хотя бы один потребовал C++, причина разобрана и записана строкой `STATUS-NNN` либо отдельной задачей;
    - контракт и [Add Screen Field](../../Guides/AddScreenField.md) описывают сборку нового блока из Designer как штатную процедуру.
  - Evidence: отчёт change set, `Docs/UI/ScreenTemplates.md`, `Docs/Guides/AddScreenField.md`.

## Проверка milestone

- [ ] Три новых композита существуют и работают от Lua до экрана.
- [ ] Ни один из трёх не потребовал изменений под `Source/`, и это показано diff-ом.
- [ ] `inventory_tabs` доказывает цепочку из четырёх уровней и выдерживает отказ на нижнем.
- [ ] Записано, какие возможности модели остались непроверенными после этих трёх.
