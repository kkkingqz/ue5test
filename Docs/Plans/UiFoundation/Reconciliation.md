---
title: Layered Reconciliation Tasks
status: active
version: 1.2
updated: 2026-08-20
depends_on:
  - Identity.md
  - CoreBaseline.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# M4 — Reconciliation

> **Материализует:** разделы «Game Shell», «Reconciliation» и «Route/layer rules» [UI Document](../../UI/UIDocumentAndReconciliation.md).
> **Задачи:** UIF-17…21.
> **Результат:** маршрут, оверлеи и модалки существуют одновременно; перестраивается только изменившееся.

## Результат этапа

`UGV2RuntimeSubsystem` перестаёт быть держателем единственного активного экрана. Этап закрывает первый пункт ближайших пробелов из [Implementation Status](../../Status/ImplementationStatus.md).

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

- [ ] **UIF-21 — Атомарность и переход `rh` на слои** — blocked: у `rh` нет собственного presentation-кода, переносить на слои нечего
  - Зависимости: UIF-20.
  - Done: отказ любого кандидата оставляет предыдущую ревизию и её handles без изменений; публикация binding set остаётся атомарной; полный `ctest`, `gv2-headless --self-test`, `--check-scripts` и Unreal automation зелёные.
  - **Уточнение по итогам повторной проверки:** `rh` на момент завершения этапа не имеет собственного presentation-кода (`GameData/rh/` не вызывает `screens.publish`, `show_overlay` или `show_modal` ни разу) — единственный потребитель конверта документа в контенте остаётся `GameData/sample/scripts/debug/start.lua`. Формулировка «экраны `rh` переведены на конверт со слоями, покупка использует модалку подтверждения» была недостоверной и удалена; сам механизм (маршрут+оверлеи+модалки, атомарность, `AttachScreenToLayer` через реальный `WBP_GameShell`) реализован и проверен automation-тестом на синтетическом сценарии, но не имеет игрового потребителя в `rh` и не может считаться закрытым в исходной формулировке.
  - Evidence: `GameData/rh/scripts/` (проверено на отсутствие presentation-вызовов), `GV2.UI.LayeredReconciliationContract`, golden-прогон, отчёт CTest.

## Проверка milestone

- [x] Маршрут, оверлей и модалка существуют одновременно.
- [x] Ревизия, меняющая одно поле, не пересоздаёт остальные экземпляры и элементы.
- [x] Изменение порядка элементов не пересоздаёт ни одного виджета.
- [x] Отказ кандидата не оставляет частично обновлённого экрана.
- [x] Загрузка сейва возвращает маршрут; стек оверлеев пуст.
