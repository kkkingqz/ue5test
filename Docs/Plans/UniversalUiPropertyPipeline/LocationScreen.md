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
  - **Независимый аудит (2026-08-25):** последний пункт Done был неверен на уровне виджетов — `GV2ScreenWidgetBase.cpp` не знал ни про `IGV2UiPropertyHost`, ни про `PrepareUiHostProperties`/`CommitUiHostProperties`, конвертера `GV2RuntimeCore::FValue` → `FGV2PreparedUiValue` не существовало, `RhStartOpensLocationScreen`/`LocationScreenTransitionContract` были красными. Попутно найден и исправлен регресс в `ExtractGenericBindings`: эвристика ошибочно отклоняла валидную форму `location_player_status.v1` как «неизвестный ключ». **Закрыто UPP-27 (2026-08-25)** — см. `../ScreenAndDocument.md`: мост построен (`GV2UiSchemaCache` + переписанный `GV2ScreenFieldAdapterRegistry` + `IGV2ScreenFieldHost` + `UGV2ScreenWidgetBase` на Prepare/Commit), `ExtractGenericBindings`/`IsFlatBindableItemsSchema` полностью удалены и заменены генерическим schema-driven обходом, оба теста зелёные — Done этого пункта теперь верен.

- [ ] **UPP-26 — Приведение контента и Lua**
  - Зависимости: UPP-25.
  - Done: в презентере и контенте не осталось формы, существующей ради старого транспорта; идентичность всех коллекций явная и семантическая — ни позиционной, ни выведенной из `resource_id`; свойства, которые раньше принимались и игнорировались, отсутствуют либо реализованы; спека на стороне Lua проверяет форму каждого поля экрана локации против скомпилированной схемы, а не против записанного вручную списка ключей — тест краснеет при расхождении формы презентера и схемы; `gv2-headless --check-scripts` зелёный.
  - Evidence: `GameData/`, `Tests/Lua/presentation/`, `gv2-headless --check-scripts`.
  - **Статус (2026-08-25):** первые три пункта Done проверены и выполнены. (1) В `GV2ScreenFieldAdapterRegistry` от location не осталось ни одного адаптера (`Adapters({})`), только generic-хелперы; в презентере (`GameData/textsystem/scripts/presentation/location_presenter.lua`) нет ни одной ветки ради старого транспорта. (2) Идентичность коллекций везде явная и семантическая: `items` (инвентарь) ключуется `item.instance_id`, `characters` — `char_entry.key` из контента экрана, `meters` — константным `"stamina"`, `commands`/travel-кнопки — `act.key`/`"travel_" .. path`; ни один ключ не выведен из `resource_id` и не позиционный (проверено чтением презентера и существующих `Tests/Lua/presentation/{declarative_location_spec,location_scene_boundary_spec}.lua`). (3) Ни одного игнорируемого свойства не осталось — `DescribeUiCapabilities` всех четырёх композитов (`GV2LocationCompositeWidgetBases.cpp`) объявляет ровно те поля, что есть в соответствующей `ui_field_location_*.v1` схеме, один в один, включая `portrait_resource_id` (через `AddImage`). `gv2-headless --check-scripts` зелёный (`{"ok":true,"status":"ok","modules_checked":41,...}`), вся связка `Tests/Lua/presentation/*.lua` (в т.ч. `closed_schema_spec.lua`) проходит против настоящего `GameData` в `gv2-headless --self-test` (ctest `gv2_headless_self_test`, 68/68 портативных тестов зелёные).
    Четвёртый пункт — «спека проверяет форму против **скомпилированной** схемы, а не записанного вручную списка ключей» — по-прежнему не выполнен буквально на стороне **Lua-спеки**: `closed_schema_spec.lua` (BAI-03) остаётся с ручным списком ключей, Lua-песочница так и не даёт скриптам способа прочитать схему. Это честно остаётся gap.
    **Частично закрыто UPP-27 (2026-08-25) на стороне C++-рантайма**, что было указано здесь как ближайшая альтернатива: `FGV2UiSchemaCache` (`Source/GV2/Private/UI/GV2UiSchemaCache.h/.cpp`) резолвит любой `schema_id`, сканируя реальные `*.schema.json5`, и `FGV2ScreenFieldAdapterRegistry::PrepareBindingDefinitions`/`BuildFields` теперь валидируют форму каждого поля экрана локации против этой скомпилированной схемы на **каждом** реальном apply в production (не только в тесте) — `IsKnownSchema`'s захардкоженный список и эвристика `ExtractGenericBindings` удалены полностью. Это не то же самое, что требует Done (Lua-тест против схемы), но closed-schema-нарушение теперь ловится по-настоящему для ЛЮБОй схемы, а не только для узкого `IsFlatBindableItemsSchema`-подмножества — реального разрыва между презентером и схемой в production больше нет, только в конкретно оформленном Lua-тесте.

## Проверка milestone

- [x] Экран локации применяется целиком через новый pipeline (закрыто UPP-27, 2026-08-25).
- [x] Каждое свойство каждого из четырёх композитов наблюдаемо.
- [x] Форма презентера сверяется со скомпилированной схемой автоматически (в C++-рантайме на каждом apply, UPP-27; Lua-спека всё ещё против ручного списка — см. UPP-26 gap выше).
- [x] Ни одного schema-specific адаптера для экрана локации не осталось.
