---
title: Migration Tasks
status: active
version: 1.5
updated: 2026-09-03
depends_on:
  - README.md
  - Prerequisites.md
---

# M2 — Migration

> **Материализует:** перевод трёх композитов локации на объявление и удаление специального мостика.
> **Задачи:** DCA-05…08.
> **Результат:** в коде не остаётся C++-класса виджета, существующего только ради композиции существующих элементов.

## Результат этапа

`TopBar` был переведён в `DUC-08` и показал, что механизм работает на простейшем случае: три текстовых свойства и идентичность. Оставшиеся три сложнее ровно тремя вещами, которые снял M1.

Порядок трёх задач перевода произволен, но `DCA-08` строго последний: пока хотя бы один класс локации существует, мостик в планировщике не может быть удалён.

**Критерий каждого перевода одинаков:** объявление воспроизводит capability один в один, C++-класс удаляется, поведение экрана не меняется, и это проверено сквозным прогоном от значения на стороне Lua до захваченного состояния виджетов.

## Задачи

- [x] **DCA-05 — Scene переведён на объявление**
  - Зависимости: DCA-01, DCA-02, DCA-03.
  - `UGV2LocationSceneWidgetBase` объявляет два `Image` (фон и плитка), один `Text` (контекст), одну `CollectionHost` (персонажи) и идентичность. Все пять видов доступны в Designer.
  - Инвариант: объявление воспроизводит capability один в один, поведение экрана не меняется. Миграция, «почти» воспроизводящая набор, — это тихое изменение контракта поля, которое обнаружится на стороне контента.
  - Не считается закрытием: сохранение C++-класса «на время»; объявление, отличающееся от прежнего набора хотя бы одним свойством без записанного обоснования.
  - Done:
    - `WBP_SceneView` переведён на `UGV2DeclaredCompositeWidgetBase` с объявлением из пяти записей;
    - фон и плитка объявлены необязательными, если ассет допускает их отсутствие;
    - `UGV2LocationSceneWidgetBase` удалён из исходников;
    - персонаж таверны виден, фон и плитка применяются, политика масштабирования сохранена;
    - sweep наблюдаемости покрывает новый композит;
    - тест на несовместимое объявление против того же ребёнка краснеет.
  - Evidence: `Content/TextSystem/UI/Widgets/WBP_SceneView.uasset`, `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`, `Source/GV2/Private/Tests/`.
  - **Реализация (2026-09-03):** `WBP_SceneView` перепривязан (`unreal-mcp` `set_parent`) с `UGV2LocationSceneWidgetBase` на `UGV2DeclaredCompositeWidgetBase`; его Designer-дерево (`SceneContextText`/`Background`/`BackgroundTile`/`CharacterRepeater`, все реальные production-виджеты) не тронуто — реордер класса не трогает `WidgetTree`. `DeclaredCapabilities` заданы пятью записями и на CDO, и (тот же живой дефект instance-vs-CDO, что DUC-08 нашёл для TopBar) на самом instance `Scene` внутри `WBP_LocationScreen`: `background_tile_resource_id → BackgroundTile: ResourceRef (optional)`, `background_resource_id → Background: ResourceRef (optional)`, `context_text → SceneContextText: Text (optional)`, `characters → CharacterRepeater: CollectionHost (optional, EntryWidgetClass=WBP_Icon, key)`, `key → (self): Key`. Все пять помечены `bOptional=true` (кроме `key`) для параметрической эквивалентности прежним `!= nullptr`/`HasUsableCharacterRepeaterHost()` guard'ам в удалённом C++.

    `UGV2LocationSceneWidgetBase` полностью удалён из `GV2LocationCompositeWidgetBases.h`/`.cpp` (класс, `DescribeUiCapabilities`, `NativePreConstruct`, `ResolveCharacterRepeater`, `HasUsableCharacterRepeaterHost`, `GetCharacterWidgetClass`). Ветка `Cast<UGV2LocationSceneWidgetBase>` в `GV2UiMutationPlan.cpp`'s target-resolution мостике удалена (частичный прогресс к `DCA-08` — гарантирован компилятором для этого класса, PlayerStatus/CommandPanel остаются). Все тестовые usages (`GV2PropertyConsumersTests.cpp` §13c, `GV2RuntimeSubsystemTests.cpp` — allowlist аудита, `RhStartOpensLocationScreen`, viewport matrix test, `LocationScreenTransitionContract`, CCF-06/07/11 фикстуры) переведены на generic-класс: сопоставление по `HostIdentity == "scene"` вместо `Cast` на конкретный класс, `GetWidgetFromName` вместо BindWidget-reflection. Два теста, чей предмет структурно исчез вместе с классом (DCA-03's "SceneWidget without character widget class" — generic-класс не имеет per-class `Resolve*WidgetClass` для отсутствия отката; PCC-12's "Scene eager internal repeater construction" — generic-класс не создаёт transient-объекты вовсе, чистота гарантирована конструкцией, не lifecycle-инвариантом), удалены с explanatory-комментарием, а не молча.

    Red→green на реальном контенте (по образцу DUC-08): временная очистка `DeclaredCapabilities` на instance `Scene` внутри `WBP_LocationScreen` (без сохранения на диск) уронила `GV2.Runtime.Presentation.RhStartOpensLocationScreen` с `core:diagnostic.ui_capability.unknown_schema_property: Schema property 'background_tile_resource_id' is not supported by widget capabilities`; восстановление — снова чисто, подтверждено live через `AutomationTestToolset.RunTests`.

    Верификация: portable ctest 76/76; полный `GV2.*` UE Automation (live editor) 101/101 без единого фейла — включая сквозные `RhStartOpensLocationScreen`, `LocationScreenViewportMatrix`, `LocationScreenTransitionContract`, `CapabilityObservabilityCompositeSweep`/`Harness` на реальном мигрированном `WBP_SceneView`/`WBP_LocationScreen`.

- [x] **DCA-06 — PlayerStatus переведён на объявление**
  - Зависимости: DCA-01, DCA-02, DCA-03.
  - Самый крупный из трёх: `Text` (имя), `Image` (портрет), три `CollectionHost` (метры, предметы, эффекты) и идентичность. Именно здесь исторически жили четыре экземпляра семейства «значение исчезло на границе», поэтому сквозная проверка обязана дойти до каждой из трёх коллекций.
  - Инвариант: тот же, что у `DCA-05`. Здесь он острее: именно в этом композите жили четыре экземпляра семейства «значение исчезло на границе», поэтому сквозная проверка обязана дойти до каждой из трёх коллекций, а не до композита в целом.
  - Не считается закрытием: проверка коллекций по количеству элементов вместо значений; покрытие одной коллекции из трёх.
  - Done:
    - `WBP_PlayerStatusPanel` переведён;
    - портрет объявлен необязательным, если ассет допускает его отсутствие;
    - `UGV2LocationPlayerStatusWidgetBase` удалён;
    - метка метра, иконки предметов и эффектов доходят до экрана — проверено значениями;
    - sweep наблюдаемости покрывает новый композит, включая элементы всех трёх коллекций.
  - Evidence: `Content/TextSystem/UI/Widgets/WBP_PlayerStatusPanel.uasset`, `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`.
  - **Реализация (2026-09-03):** `WBP_PlayerStatusPanel` перепривязан (`unreal-mcp` `set_parent`) с `UGV2LocationPlayerStatusWidgetBase` на `UGV2DeclaredCompositeWidgetBase`; его Designer-дерево (`PlayerNameText`/`Portrait`/`MeterRepeater`/`ItemRepeater`/`EffectRepeater`, все реальные production-виджеты) не тронуто. `DeclaredCapabilities` заданы шестью записями и на CDO, и (тот же живой дефект instance-vs-CDO) на самом instance `PlayerStatus` внутри `WBP_LocationScreen`: `name → PlayerNameText: Text` (единственное required-поле композита — в удалённом C++ было `BindWidget`, а не `BindWidgetOptional`, как всё остальное в классе), `portrait_resource_id → Portrait: ResourceRef (optional)`, `meters → MeterRepeater: CollectionHost (optional, EntryWidgetClass=WBP_ProgressBar, key)`, `items → ItemRepeater: CollectionHost (optional, EntryWidgetClass=WBP_Icon, key)`, `effects → EffectRepeater: CollectionHost (optional, EntryWidgetClass=WBP_Icon, key)`, `key → (self): Key`.

    `UGV2LocationPlayerStatusWidgetBase` полностью удалён из `GV2LocationCompositeWidgetBases.h`/`.cpp` (класс, `DescribeUiCapabilities`, `NativePreConstruct`, `HasUsableMeterRepeaterHost`/`HasUsableItemRepeaterHost`/`HasUsableEffectRepeaterHost`, `Resolve*Repeater`). Ветка `Cast<UGV2LocationPlayerStatusWidgetBase>` в `GV2UiMutationPlan.cpp`'s target-resolution мостике удалена (после `DCA-05` и `DCA-06` в мостике остаётся только ветка `CommandPanel` — `DCA-07` последняя). Все тестовые usages (`GV2PropertyConsumersTests.cpp` §13b, `GV2RuntimeSubsystemTests.cpp` — allowlist аудита, `RhStartOpensLocationScreen`, viewport matrix test, `LocationScreenTransitionContract`) переведены на generic-класс: сопоставление по `HostIdentity == "player_status"` вместо `Cast` на конкретный класс, реальный `WidgetTree`/`ConstructWidget` вместо BindWidget-reflection на bare `NewObject`. PCC-12's "PlayerStatus eager internal repeater construction" блок (generic-класс не создаёт transient-объекты вовсе, чистота гарантирована конструкцией) удалён с explanatory-комментарием, не молча.

    §13b дополнительно усилен по требованию инварианта задачи (не количество, а значения): метры теперь проверяются по `GetProgress()` (percent) и по применённому тексту `LabelText` (reflection-чтение — `LabelText` `protected`, но резолвится корректно на реальном Blueprint-классе `WBP_ProgressBar_C`, сконструированном через `WidgetTree`/`ConstructWidget`, а не bare `NewObject`); предметы и эффекты — по применённой текстуре иконки (`GetImageBrush().GetResourceObject()` против текстуры, разрешённой из каталога напрямую). Собственный `AppliedResourceId` элемента коллекции здесь не годится для проверки: `UGV2ImageWidgetBase::DescribeUiCapabilities` объявляет `resource_id` на дочернем `Image`, поэтому при разрешении вложенного property host'а (`items`/`effects` как `CollectionHost` из `IGV2UiPropertyHost`-элементов) `Commit` идёт напрямую в `UImage`, минуя `ApplyImageResource()` и его bookkeeping — это существующее поведение пайплайна, не дефект этой задачи, но оно делает `GetAppliedResourceId()` неверным инструментом проверки именно для элементов коллекции (в отличие от top-level `Image`-поля композита, где `Commit` идёt через сам host и `AppliedResourceId` обновляется).

    Content-миграция вскрыла транзиентный артефакт порядка загрузки: после удаления C++-класса и до перепривязки `WBP_PlayerStatusPanel` виджет `PlayerStatus` временно выпадал из `WidgetTree` экрана `WBP_LocationScreen` при компиляции (UMG компилятор не мог разрешить тип ещё-не-перепривязанного дочернего Blueprint-класса и orphan'ил узел — тот самый `[PlayerStatus] was deleted but still has a GUID reference` ensure). Сохранение исправленного `WBP_PlayerStatusPanel` на диск и свежий relaunch редактора (перекомпиляция `WBP_LocationScreen` уже против валидного parent chain) вернули `PlayerStatus` в дерево с прежним `HostIdentity="player_status"` без ручного пересоздания — исходное размещение (`Body.HorizontalBoxSlot_0`, рядом с `Scene`) не пострадало.

    Red→green на реальном контенте (по образцу DUC-08/DCA-05): временная очистка `DeclaredCapabilities` на instance `PlayerStatus` внутри `WBP_LocationScreen` (без сохранения на диск) уронила `GV2.Runtime.Presentation.RhStartOpensLocationScreen` с `core:diagnostic.ui_capability.unknown_schema_property: Schema property 'name' is not supported by widget capabilities`; восстановление — снова чисто, подтверждено live через `AutomationTestToolset.RunTests`.

    Верификация: portable ctest 76/76; полный `GV2.*` UE Automation (live editor) 101/101 без единого фейла — включая сквозные `RhStartOpensLocationScreen`, `LocationScreenViewportMatrix`, `LocationScreenTransitionContract` на реальном мигрированном `WBP_PlayerStatusPanel`/`WBP_LocationScreen`.

- [x] **DCA-07 — CommandPanel переведён на объявление**
  - Зависимости: DCA-01, DCA-02, DCA-03, DCA-04.
  - После удаления мёртвого `OnBindingInvoked` у композита остаются одна `CollectionHost` (кнопки) и идентичность.
  - Инвариант: тот же. Дополнительно проверяется, что удаление мёртвого `OnBindingInvoked` в `DCA-04` не задело работающий путь семантического binding кнопок.
  - Не считается закрытием: перенос `OnBindingInvoked` в объявляемый композит вместо удаления.
  - Done:
    - `WBP_CommandPanel` переведён;
    - `UGV2LocationCommandPanelWidgetBase` удалён;
    - кнопки локации применяются, семантический binding каждой доходит до реестра и остаётся отклоняемым для устаревшего handle;
    - отказ подготовки текста любой кнопки по-прежнему отказывает всю коллекцию.
  - Evidence: `Content/TextSystem/UI/Widgets/WBP_CommandPanel.uasset`, `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`.
  - **Реализация (2026-09-03):** `WBP_CommandPanel` перепривязан (`unreal-mcp` `set_parent`) с `UGV2LocationCommandPanelWidgetBase` на `UGV2DeclaredCompositeWidgetBase`; его Designer-дерево (`ButtonRepeater`, реальный `WBP_ListView_WrapButtons`) не тронуто — DCA-02 уже сделало его именованным ребёнком `WidgetTree`, так что `ButtonRepeater`/`ButtonContainer`-специфичная ветка в `GV2UiMutationPlan.cpp`'s target-resolution мостике оказалась мёртвой ещё до этой задачи и просто удалена, без замены. `DeclaredCapabilities` заданы двумя записями и на CDO, и на самом instance `Commands` внутри `WBP_LocationScreen`: `items → ButtonRepeater: CollectionHost (optional, EntryWidgetClass=WBP_Button, key)`, `key → (self): Key`.

    `UGV2LocationCommandPanelWidgetBase` был последним классом в `GV2LocationCompositeWidgetBases.h`/`.cpp` — после его удаления файлы не содержали ничего, кроме мёртвых include'ов, поэтому пара файлов удалена целиком (не оставлена пустой), а `#include "UI/GV2LocationCompositeWidgetBases.h"` снят из шести файлов, которые его больше не используют. Это расширяет DCA-05/06's практику точечного редактирования: там сокращаемый файл ещё не опустел. Ветка `Cast<UGV2LocationCommandPanelWidgetBase>` в `GV2UiMutationPlan.cpp`'s target-resolution мостике удалена — это была последняя из трёх, мостик теперь содержит только общий путь по `GetWidgetFromName`/reflection-fallback (DCA-08 удалит саму структуру, не ветки).

    Все тестовые usages (`GV2PropertyConsumersTests.cpp` §13d, `GV2RuntimeSubsystemTests.cpp` — allowlist аудита, `RhStartOpensLocationScreen`, viewport matrix test, `LocationScreenTransitionContract`, `CoreRepeaterContract`, `ScreenPreflightPredictsDeepChildFailure`) переведены на generic-класс: сопоставление по `HostIdentity == "commands"` вместо `Cast` на конкретный класс, `GetWidgetFromName` вместо `GetRepeater()`/BindWidget-reflection. Два теста, чей предмет структурно исчез вместе с классом (`LocationCompositeUnresolvedClassRejection`'s CommandPanel half — тот же generic "EntryWidgetClass не дефолтится" инвариант, уже доказанный class-agnostically DCA-01/03; PCC-12's CommandPanel half — generic-класс не создаёт transient-объекты вовсе), были **удалены целиком, а не оставлены пустой оболочкой**: после DCA-05/06 уже убрали половины Scene/PlayerStatus, CommandPanel был последним реальным содержимым в обоих тестах — заглушка, создающая и уничтожающая `UWorld` без единой проверки, прошла бы всегда, обещая своим именем покрытие, которого больше нет.

    Существующие проверки семантического binding кнопок (`B1 text committed`, `B1 binding handle valid`, `отказ CmdPanel Prepare fails on invalid text kind` отклоняет всю коллекцию) сохранены без изменений — DCA-07's дополнительное требование ("удаление мёртвого `OnBindingInvoked` в `DCA-04` не задело работающий путь семантического binding") подтверждено тем, что эти тесты продолжают проходить после миграции.

    Red→green на реальном контенте (по образцу DUC-08/DCA-05/06): временная очистка `DeclaredCapabilities` на instance `Commands` внутри `WBP_LocationScreen` (без сохранения на диск) уронила `GV2.Runtime.Presentation.RhStartOpensLocationScreen` с `core:diagnostic.ui_capability.unknown_schema_property: Schema property 'items' is not supported by widget capabilities`; восстановление — снова чисто, подтверждено live через `AutomationTestToolset.RunTests`.

    Верификация: portable ctest 76/76; полный `GV2.*` UE Automation (live editor) 99/99 без единого фейла (101 минус два удалённых пустых теста) — включая сквозные `RhStartOpensLocationScreen`, `LocationScreenViewportMatrix`, `LocationScreenTransitionContract`, `ScreenPreflightPredictsDeepChildFailure` на реальном мигрированном `WBP_CommandPanel`/`WBP_LocationScreen`.

- [ ] **DCA-08 — Специальный путь под конкретный виджет удалён и не может вернуться**
  - Зависимости: DCA-05, DCA-06, DCA-07.
  - Исходно `GV2UiMutationPlan.cpp` содержал три `Cast` по конкретным классам локации с перечислением имён контейнеров — путь разрешения цели, существующий только ради того, что нынешние ассеты держали репитер вне `WidgetTree`. `DCA-05…07` удалили все три ветки вместе с классами, которые они называли (компилятор гарантировал это для каждой при удалении класса) — сама conditional-структура вокруг них, генерический reflection-fallback (`FindFProperty<FObjectPropertyBase>`, не привязанный к конкретному классу) и задача формально закрыть возможность нового Cast-по-классу остаются целью `DCA-08`.
  - Инвариант: общий путь разрешения цели знает интерфейсы, а не конкретные классы виджетов. Это главный измеримый результат плана: пока мостик существует, утверждение «модель покрывает любой композит» имеет живое исключение.
  - Гарантия для трёх названных классов даётся **компилятором, а не гейтом**: `DCA-05…07` удаляют `UGV2LocationSceneWidgetBase`, `UGV2LocationPlayerStatusWidgetBase` и `UGV2LocationCommandPanelWidgetBase`, после чего `Cast` к ним не компилируется. Компилятор перечисляет все точки использования сам, и обойти его переносом кода или сборкой имени по частям нельзя. Отдельный скан на эти три имени избыточен и не нужен.
  - Гейт охраняет **общий образец** — будущий `Cast` к конкретному классу виджета в разрешении цели, где типы существуют и компилятор не помогает. Его множество: классы, реализующие `IGV2ScreenFieldHost` или `IGV2UiPropertyHost`, — перечисляется по интерфейсу, поэтому новый хост попадает под правило автоматически, а не после ручного дополнения списка.
  - Не считается закрытием: обобщение мостика на произвольный хост — это закрепило бы адресацию объекта вне `WidgetTree` как часть модели; сохранение хотя бы одной ветви под конкретный класс «для совместимости»; скан, привязанный к имени одного файла, — при переносе разрешения цели в другой файл он молча перестанет что-либо охранять.
  - Done:
    - блок удалён целиком;
    - разрешение цели остаётся общим — по имени в `WidgetTree`, затем по свойству объекта;
    - ни одного упоминания конкретного класса виджета в разрешении цели не остаётся;
    - гейт реализован runtime automation-тестом по образцу `GBH-05` (рекурсивный обход исходников модуля с исключением собственного файла), а не как отказ компиляции;
    - гейт проверяет два условия сразу: в коде разрешения цели нет `Cast` к классу, реализующему `IGV2ScreenFieldHost` или `IGV2UiPropertyHost`, **и** логика разрешения цели присутствует ровно в одном месте — иначе перенос её в другой файл выводит код из-под проверки;
    - есть отрицательный самотест: синтетический `Cast` к конкретному хосту в разрешении цели обязан уронить гейт;
    - удаление подтверждено: возврат блока не требуется ни одному тесту, его отсутствие не ломает ни один.
  - Evidence: `Source/GV2/Private/UI/GV2UiMutationPlan.cpp`, `Source/GV2/Private/Tests/`.

## Проверка milestone

- [ ] Ни одного C++-класса виджета, существующего только ради композиции, в исходниках не осталось.
- [ ] Разрешение цели не упоминает ни одного конкретного класса виджета; для трёх удалённых классов это гарантирует компилятор, для будущих — гейт с отрицательным самотестом.
- [ ] Экран локации работает без изменения поведения, проверено от Lua до состояния виджетов.
- [ ] Sweep наблюдаемости покрывает все три новых объявляемых композита.
