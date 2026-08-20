---
title: Layered Reconciliation Tasks
status: archived
version: 1.3
updated: 2026-08-20
depends_on:
  - Identity.md
  - CoreBaseline.md
  - ../../../UI/UIDocumentAndReconciliation.md
---

# M4 — Reconciliation

> **Материализует:** разделы «Game Shell», «Reconciliation» и «Route/layer rules» [UI Document](../../../UI/UIDocumentAndReconciliation.md).
> **Задачи:** UIF-17…21.
> **Результат:** маршрут, оверлеи и модалки существуют одновременно; перестраивается только изменившееся.

## Результат этапа

`UGV2RuntimeSubsystem` перестаёт быть держателем единственного активного экрана. Этап закрывает первый пункт ближайших пробелов из [Implementation Status](../../../Status/ImplementationStatus.md).

## Задачи

- [x] **UIF-17 — Слои Game Shell**
  - Зависимости: UIF-12.
  - Done: `WBP_GameShell` содержит шесть именованных хостов-панелей (`BackgroundHost`, `LocationContentHost`, `CharacterPresentationHost`, `CoreInterfaceHost`, `OverlayStackHost`, `ModalStackHost` — привязка по имени к `BindWidgetOptional`-полям `UGV2GameShellWidgetBase`); реестр отклоняет запись с неизвестным слоем; порядок отрисовки зафиксирован в contract; `UGV2RuntimeSubsystem::StartSession()` реально создаёт `ActiveGameShell` через сконфигурированный `GameShellClass` (`Config/DefaultGame.ini`, `GV2ScreenRegistrySettings`), а не только объявляет поле; `AttachScreenToLayer` возвращает честный `false`, если хост-панель не привязана, вместо прежнего маскирующего `return true`; ассеты изменены через `unreal-mcp`.
  - Evidence: `Content/UI/Shell/WBP_GameShell.uasset`, `Config/DefaultGame.ini`, `Source/GV2/Private/UI/GV2ScreenRegistry.cpp`, `Source/GV2/Private/UI/GV2GameShellWidgetBase.cpp`, `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, `GV2.UI.LayeredReconciliationContract`.

- [x] **UIF-18 — Конверт документа в Lua**
  - Зависимости: UIF-17.
  - Done: источник презентации возвращает конверт `{ ui_instance_id, revision, route, overlays, modals }`; `revision` монотонна внутри экземпляра; более одного маршрута — типизированный отказ; авторский слой объявляет оверлей и модалку без ручной сборки конверта; открытость хранится в состоянии вне сейва.
  - Evidence: `Scripts/runtime/`, `Scripts/authoring/`, `Tests/Lua/`.

- [x] **UIF-19 — Сопоставление экземпляров и элементов**
  - Зависимости: UIF-18, UIF-03.
  - Done: Screen Instances сопоставляются по паре `layer + instance_key`; совпадение идентичности и `screen_id` переиспользует виджет и применяет полный набор полей; смена `screen_id` заменяет класс через реестр; внутри повторяемого поля элементы сопоставляются по паре «тип + `key`»; изменение порядка при неизменном наборе ключей выполняется переупорядочиванием без пересоздания; UI-local состояние переиспользованного элемента сохраняется.
  - Evidence: `Source/GV2/Private/UI/`, `GV2.Runtime.Presentation.*`.

- [x] **UIF-20 — Правила слоёв**
  - Зависимости: UIF-19.
  - Done: модалка блокирует нижние слои, интерактивна только верхняя пригодная; оверлей по умолчанию маршрут не блокирует; удаление владельца каскадно удаляет его модалки; неизвестный `screen_id`, несовместимый класс реестра и несовместимый контракт поля делают документ невалидным до интерактивного apply.
  - Evidence: `Source/GV2/Private/UI/`, `GV2.Runtime.Presentation.*`.

- [x] **UIF-21 — Атомарность и переход `rh` на слои**
  - Зависимости: UIF-20.
  - Done: покупка в магазине `rh` использует модальное окно подтверждения (`show_modal`/`close_modal`, `request_buy`, `confirm_buy`, `cancel_buy`); модалка — отдельный зарегистрированный экран `core:screen.modal_confirm` (`WBP_ModalConfirmScreen`, слой `modal_stack`, единственное поле `modal` схемы `core:schema.ui_field.modal.v1`), а не поле, встроенное в маршрутный экран — `WBP_Modal` является `CommonActivatableWidget`, и его статическое встраивание внутрь `WBP_Testscreen` ломало `NativeConstruct` (ensure `bInputDataLoaded` в `CommonInputSettings`) при построении дерева виджетов; реальные экшены покупки (`rh:action.buy_sword`, `rh:action.buy_armor`) переключены на `request_buy` — при первой реализации они по-прежнему указывали на `buy` напрямую, и подтверждение было недостижимо ни для одной кнопки в игре; отказ любого кандидата оставляет предыдущую ревизию и её handles без изменений; публикация binding set остаётся атомарной; полный `ctest`, `gv2-headless --self-test`, `--check-scripts` и Unreal automation зелёные.
  - Evidence: `GameData/rh/scripts/authoring/gameplay.lua`, `GameData/rh/definitions/actions.json5`, `GameData/rh/definitions/texts.json5`, `Content/UI/Widgets/WBP_ModalConfirmScreen.uasset`, `Content/UI/Registry/DA_ScreenRegistry.uasset`, `Tests/Lua/presentation/rh_layered_presentation_spec.lua`, `Tests/Lua/presentation/declarative_location_spec.lua`, `Tests/Lua/presentation/dynamic_menu.lua`, `GV2.UI.LayeredReconciliationContract`, `GV2.Runtime.UIKit.CentralThemeAndComponents`, отчёт CTest.

## Проверка milestone

- [x] Маршрут, оверлей и модалка существуют одновременно.
- [x] Ревизия, меняющая одно поле, не пересоздаёт остальные экземпляры и элементы.
- [x] Изменение порядка элементов не пересоздаёт ни одного виджета.
- [x] Отказ кандидата не оставляет частично обновлённого экрана.
- [x] Загрузка сейва возвращает маршрут; стек оверлеев пуст.
