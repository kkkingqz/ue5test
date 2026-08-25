---
title: LocationScreen Migration Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - CollectionsAndComposites.md
  - ../../UI/ScreenTemplates.md
---

# M6 — LocationScreen

> **Материализует:** фазу 5 proposal, разделы 22.21…22.24, 27.
> **Задачи:** UPP-24…26.
> **Результат:** единственный производственный экран работает на новой архитектуре, а его контент приведён к целевой форме.

## Результат этапа

Экран локации — это место, где проявились все четыре экземпляра семейства «значение исчезло на границе». Он мигрирует последним из содержательных, потому что состоит из четырёх композитов, каждый из которых опирается на коллекции и листья предыдущих этапов.

Обратной совместимости нет, поэтому Lua-форма приводится к целевой в тех же change set, а не переносится как есть с обещанием нормализовать позже. Перенос кривой формы «чтобы не ломать» — это способ получить следующий раунд findings об идентичности и потерянных ключах.

## Задачи

- [x] **UPP-24 — TopBar и PlayerStatus**
  - Зависимости: UPP-23.
  - Done: оба композита объявляют capability на каждое своё свойство, включая коллекции `meters`, `items` и `effects`; метка метра, иконки предметов и эффектов имеют наблюдаемых потребителей — harness перебирает их и краснеет при отвязке любого target; схемы полей объявлены данными; ручные `PrepareLocationTopBar`/`BuildLocationTopBar`/`PrepareLocationPlayerStatus`/`BuildLocationPlayerStatus` и оба DTO удалены; Lua-форма приведена тем же change set.
  - Evidence: `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`, `GameData/textsystem/scripts/presentation/location_presenter.lua`.

- [x] **UPP-25 — Scene и CommandPanel**
  - Зависимости: UPP-24.
  - Done: оба композита мигрированы; коллекция персонажей использует ключ схемы, отрисовка идёт через consumer ресурса, `PreserveAspect` и привязка к нижнему краю остаются объявленными свойствами, а не C++-константами внутри лямбды; CommandPanel строит binding-пути generic-обходом; ни один из четырёх композитов не имеет собственной пары `CanApply`/`Apply`; адаптеры, DTO и ветки union удалены; экран локации применяется полностью и проверен end-to-end от Lua до захваченного состояния виджетов.
  - Evidence: `Source/GV2/Public/UI/GV2LocationCompositeWidgetBases.h`, `Content/TextSystem/UI/`.
  - **Независимый аудит (2026-08-25):** последний пункт Done неверен на уровне виджетов. `GV2ScreenWidgetBase.cpp` не тронут этим change set и не знает ни про `IGV2UiPropertyHost`, ни про `PrepareUiHostProperties`/`CommitUiHostProperties`; универсального конвертера `GV2RuntimeCore::FValue` (сырое значение поля экрана из Lua) → `FGV2PreparedUiValue` (подготовленное дерево для хоста) в кодовой базе нет вообще. Как следствие `GV2.Runtime.Presentation.RhStartOpensLocationScreen` и `GV2.Runtime.Presentation.LocationScreenTransitionContract` красные (сцена таверны показывает 0 персонажей вместо 1, кнопка travel не находится) — экран локации сейчас не рендерится в production. Это ровно работа UPP-27/M7 (`../ScreenAndDocument.md`), в это же аудит попутно найден и исправлен реальный регресс в `ExtractGenericBindings` (`Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp`): эвристика извлечения bindings ошибочно отклоняла валидную форму `location_player_status.v1` (несколько соседних коллекций без binding в каждом элементе) как «неизвестный ключ» — сужена до `IsFlatBindableItemsSchema` (`location_commands.v1`, `button_list.v2`), сохранив BAI-03/REV3-03 там, где форма действительно известна. По явному решению пользователя мост откладывается к UPP-27 и не строится в рамках UPP-26.

- [ ] **UPP-26 — Приведение контента и Lua**
  - Зависимости: UPP-25.
  - Done: в презентере и контенте не осталось формы, существующей ради старого транспорта; идентичность всех коллекций явная и семантическая — ни позиционной, ни выведенной из `resource_id`; свойства, которые раньше принимались и игнорировались, отсутствуют либо реализованы; спека на стороне Lua проверяет форму каждого поля экрана локации против скомпилированной схемы, а не против записанного вручную списка ключей — тест краснеет при расхождении формы презентера и схемы; `gv2-headless --check-scripts` зелёный.
  - Evidence: `GameData/`, `Tests/Lua/presentation/`, `gv2-headless --check-scripts`.
  - **Статус (2026-08-25):** первые три пункта Done проверены и выполнены. (1) В `GV2ScreenFieldAdapterRegistry` от location не осталось ни одного адаптера (`Adapters({})`), только generic-хелперы; в презентере (`GameData/textsystem/scripts/presentation/location_presenter.lua`) нет ни одной ветки ради старого транспорта. (2) Идентичность коллекций везде явная и семантическая: `items` (инвентарь) ключуется `item.instance_id`, `characters` — `char_entry.key` из контента экрана, `meters` — константным `"stamina"`, `commands`/travel-кнопки — `act.key`/`"travel_" .. path`; ни один ключ не выведен из `resource_id` и не позиционный (проверено чтением презентера и существующих `Tests/Lua/presentation/{declarative_location_spec,location_scene_boundary_spec}.lua`). (3) Ни одного игнорируемого свойства не осталось — `DescribeUiCapabilities` всех четырёх композитов (`GV2LocationCompositeWidgetBases.cpp`) объявляет ровно те поля, что есть в соответствующей `ui_field_location_*.v1` схеме, один в один, включая `portrait_resource_id` (через `AddImage`). `gv2-headless --check-scripts` зелёный (`{"ok":true,"status":"ok","modules_checked":41,...}`), вся связка `Tests/Lua/presentation/*.lua` (в т.ч. `closed_schema_spec.lua`) проходит против настоящего `GameData` в `gv2-headless --self-test` (ctest `gv2_headless_self_test`, 68/68 портативных тестов зелёные).
    Четвёртый пункт — «спека проверяет форму против **скомпилированной** схемы, а не записанного вручную списка ключей» — **не выполнен, честно оставлен как gap**. `GV2ContentCore::ValidateUiFieldValue`/`FCompiledUiFieldSpec` (`Source/GV2ContentCore/Public/GV2ContentCore/UiSchema.h`) уже делают ровно такую валидацию, но схема после `RepositoryBuilder` нигде не сохраняется для последующих запросов, а Lua-песочница (`FRuntimeSession::FImpl::OpenEnvironment`, `Source/GV2RuntimeCore/Private/GV2RuntimeSession.cpp`) не даёт скриптам никакого способа прочитать схему — ни компилированную, ни сырой json5. Закрыть это означало бы добавить новую production-способность рантайма (сохранить `FSchemaRegistry` после сборки репозитория + Lua-биндинг для чтения схемы) — по той же логике, что и мост UPP-27, это не «причесать контент/Lua», а новая инфраструктура, и в рамках этого прохода сознательно не построена. `closed_schema_spec.lua` (BAI-03) остаётся с ручным списком ключей и продолжает проходить как регресс-тест конкретно закрытой схемы `location_commands.v1`, но не закрывает этот пункт Done.

## Проверка milestone

- [ ] Экран локации применяется целиком через новый pipeline.
- [x] Каждое свойство каждого из четырёх композитов наблюдаемо.
- [ ] Форма презентера сверяется со скомпилированной схемой автоматически.
- [x] Ни одного schema-specific адаптера для экрана локации не осталось.
