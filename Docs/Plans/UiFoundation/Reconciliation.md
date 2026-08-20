---
title: Layered Reconciliation Tasks
status: active
version: 1.1
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
  - Done: `WBP_GameShell` содержит именованные хосты `background`, `location_content`, `character_presentation`, `core_interface`, `overlay_stack`, `modal_stack`; реестр отклоняет запись с неизвестным слоем; порядок отрисовки зафиксирован в contract; ассеты изменены через `unreal-mcp`.
  - Evidence: `Content/UI/Shell/WBP_GameShell.uasset`, `Source/GV2/Private/UI/GV2ScreenRegistry.cpp`.

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
  - Done: отказ любого кандидата оставляет предыдущую ревизию и её handles без изменений; публикация binding set остаётся атомарной; экраны `rh` переведены на конверт со слоями, покупка использует модалку подтверждения; изменение `final_screen_fields` в golden воспроизведено манифестом и объяснено; полный `ctest`, `gv2-headless --self-test`, `--check-scripts` и Unreal automation зелёные.
  - Evidence: `GameData/rh/scripts/`, golden-прогон, отчёт CTest.

## Проверка milestone

- [x] Маршрут, оверлей и модалка существуют одновременно.
- [x] Ревизия, меняющая одно поле, не пересоздаёт остальные экземпляры и элементы.
- [x] Изменение порядка элементов не пересоздаёт ни одного виджета.
- [x] Отказ кандидата не оставляет частично обновлённого экрана.
- [x] Загрузка сейва возвращает маршрут; стек оверлеев пуст.
