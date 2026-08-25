---
title: Collections and Composites Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - RemainingLeaves.md
  - ../../UI/UIDocumentAndReconciliation.md
  - ../../Status/ImplementationStatus.md
---

# M5 — Collections and Composites

> **Материализует:** фазу 4 proposal, разделы 12.6, 15, 22.9…22.14.
> **Задачи:** UPP-20…23.
> **Результат:** отказ ребёнка структурно не может дать успех родителя, а переиспользуемый виджет не остаётся в промежуточном состоянии.

## Результат этапа

Композиты — то место, где старая модель теряла отказы: `ApplyItem` возвращал безусловный `true`, результат применения детей отбрасывался, а `FGV2KeyedCollection` не мог откатить состояние переиспользованных виджетов, потому что не хранил их снимков.

Prepare/Commit убирает причину: к моменту Commit все дети уже подготовлены off-tree, поэтому откат сводится к тому, чтобы не выполнять Commit.

## Задачи

- [x] **UPP-20 — Keyed collection consumer и настоящая транзакционность**
  - Зависимости: UPP-19.
  - Сегодня границы транзакционности `FGV2KeyedCollection` сужены до структуры контейнера: отказ единичного `ApplyItem` оставляет уже применённые переиспользованные виджеты в новом состоянии, а полный откат передан внешнему уровню экрана.
  - Done: consumer коллекции подготавливает состояние **всех** элементов до Commit, поэтому отказ любого элемента не оставляет ни одного переиспользованного виджета мутированным; сужённая формулировка в [UI Document § Границы транзакционности и отката](../../UI/UIDocumentAndReconciliation.md) заменена на фактическую, более сильную; тест проверяет **значения** переиспользованных виджетов сразу после отказа, а не их количество и не восстановимость повторным применением; ключ элемента обязателен и берётся из схемы — позиционный ключ невыразим; интерактивный массив без ключа отклоняется компиляцией схемы.
  - Evidence: `Source/GV2/Public/UI/GV2KeyedCollection.h` (`ReconcilePrepared` — двухфазный prepare/commit шаблон), `Source/GV2/Public/UI/GV2PropertyConsumers.h`/`.cpp` (`FGV2KeyedCollectionPropertyConsumer`), `GameData/core/schemas/ui_field_*_v1.schema.json5`. **2026-08-25, независимый аудит:** тест `GV2.UI.StandardPropertyConsumers` (§8, `Source/GV2/Private/Tests/GV2PropertyConsumersTests.cpp:509`) проверяет именно значение — после отказа item B (binding получил `number` вместо `binding`) переиспользованный `BtnA->GetBindingHandle()` строго равен старому `TestHandleA`, а не новому кандидату; красный тест на откате подтверждён вручную (временное игнорирование `PrepareUiHostProperties()==false` для элемента коллекции в `FGV2KeyedCollectionPropertyConsumer::Prepare` даёт красный `Collection Prepare fails when item B has invalid property` → `false`; восстановление — зелёный). `core:diagnostic.ui_schema.field_spec.missing_keyed_by` (`Source/GV2ContentCore/Private/UiSchema.cpp`, `SpecContainsInteractiveKind`) отклоняет массив с `binding`/`screen_fields` внутри без `keyed_by`, рекурсивно через `object`/`array` — покрыт позитивным, негативным и вложенным-негативным случаем в `UiSchemaConformance.cpp`.

- [x] **UPP-21 — ButtonList и DropdownSelect**
  - Зависимости: UPP-20.
  - Done: оба мигрированы на consumer коллекции; ручные `PrepareButtonList` и `PrepareDropdown` удалены, binding-пути строит generic-обход; отказ подготовки текста или изображения любого элемента отказывает всю коллекцию — тест краснеет, если родитель начнёт игнорировать отказ; заголовок Dropdown и его состояние раскрытия применяются тем же lifecycle, а не отдельным путём; адаптеры, DTO и ветки union удалены.
  - Evidence: `Source/GV2/Public/UI/GV2ButtonListWidgetBase.h`, `Source/GV2/Public/UI/GV2DropdownSelectWidgetBase.h`, `GameData/core/schemas/ui_field_button_list_v2.schema.json5`, `GameData/core/schemas/ui_field_dropdown_select_v1.schema.json5`, тест `GV2.Runtime.UIKit.DropdownSelectWidgetContract`. **2026-08-25, независимый аудит:** `grep` по `GV2ScreenFieldAdapterRegistry.cpp` подтверждает — `PrepareButtonList`/`BuildButtonList`/`PrepareDropdown`/`BuildDropdown` физически отсутствуют.

- [x] **UPP-22 — RichText spans и Modal**
  - Зависимости: UPP-20.
  - Done: интерактивные span-ы и popover переведены на consumers; span без ключа отклоняется; Modal объявляет capability `title`, `content`, `buttons`, `backdrop_close_action`, и **каждое** имеет наблюдаемого потребителя — частичное потребление невозможно; заголовок и содержимое Modal остаются в текстовом конвейере; отказ подготовки списка кнопок отказывает Modal; адаптеры и DTO удалены.
  - Evidence: `Source/GV2/Public/UI/GV2RichTextWidgetBase.h` (`FGV2RichTextSpansPropertyConsumer`), `Source/GV2/Public/UI/GV2ModalWidgetBase.h`, `GameData/core/schemas/ui_field_rich_text_v3.schema.json5`, `GameData/core/schemas/ui_field_modal_v1.schema.json5`. **2026-08-25, независимый аудит:** `UGV2ModalWidgetBase::DescribeUiCapabilities` объявляет ровно `title`(Text)/`content`(Text)/`buttons`(KeyedCollection)/`backdrop_close_action`(Binding, через `IGV2UiBindingTarget`) — все четыре из Done; `PrepareModal`/`BuildModalField` физически отсутствуют в `GV2ScreenFieldAdapterRegistry.cpp`. `IGV2DynamicScreenElement` на `UGV2RichTextWidgetBase` сохранён намеренно — интерактивные span-ы (`rich_text.v3` как top-level composite field) остаются за этой же задачей лишь частично: `FGV2RichTextSpansPropertyConsumer` покрывает препарацию spans, но полное снятие старого интерфейса требует, чтобы `rich_text.v3` перестал быть top-level legacy-полем — это не блокирует M6, отслеживается как известный остаток.

- [x] **UPP-23 — TabContainer и вложенный экран**
  - Зависимости: UPP-21, UPP-22.
  - Done: `screen_id` вкладки разрешается Screen Registry в Prepare, а не при переключении; вложенный экран создаётся off-tree и присоединяется только Commit-ом; отказ подготовки любой вкладки отказывает весь контейнер — результат применения детей больше не отбрасывается; handle неактивной вкладки остаётся отклоняемым Semantic Input; `screen_fields` как kind схемы используется здесь впервые и покрыт положительным и отрицательным случаем; адаптеры и DTO удалены.
  - Evidence: `Source/GV2/Public/UI/GV2TabContainerWidgetBase.h`, `Source/GV2/Public/UI/GV2ScreenRegistry.h`, `GameData/core/schemas/ui_field_tab_container_v1.schema.json5`, тест `GV2.Runtime.UI.NestedInstancesAndTabsContract`.

**2026-08-25, независимый аудит M5 (UPP-20…23):** полная сборка (`RunUBT.sh GV2Editor`, редактор закрыт во избежание hot-reload) и полный прогон `GV2.*` (92/92 зелёных) подтверждены. `grep` по `GV2ScreenFieldAdapterRegistry.cpp` подтвердил — из 15 top-level полей legacy-регистра остались только 4 (`LocationTopBar`/`LocationPlayerStatus`/`LocationScene`/`LocationCommands`, все — M6); `button_list.v2`/`dropdown_select.v1`/`modal.v1`/`rich_text.v3`/`tab_container.v1` полностью удалены из адаптеров, DTO и union (`FGV2ScreenFieldValue` сократился до 4 payload-членов). Гейт убывания зафиксировал 10 функций/4 payload-члена/10 DTO (снижение с 20/8/16 после UPP-19), `--self-test` зелёный.

Красный тест на откате подтверждён вручную для главного утверждения UPP-20 (атомарность коллекции): временное игнорирование отказа `PrepareUiHostProperties()` для элемента коллекции в `FGV2KeyedCollectionPropertyConsumer::Prepare` (`GV2PropertyConsumers.cpp`) даёт красный `Collection Prepare fails when item B has invalid property` → `false` в `GV2.UI.StandardPropertyConsumers`; восстановление — зелёный.

**Важное уточнение по интеграции (не дефект, а сознательная граница этапа):** удаление legacy-адаптеров означает, что `button_list.v2`/`dropdown_select.v1`/`modal.v1`/`rich_text.v3`/`tab_container.v1` сегодня **не достижимы** как top-level Screen Field через живой pipeline — `FGV2ScreenFieldAdapterRegistry::BuildFields` возвращает `false`, если адаптер не найден (`GV2ScreenFieldAdapterRegistry.cpp:693`), а `GV2ScreenWidgetBase.cpp`/`GV2SessionCoordinator.cpp` не были изменены в этом этапе и по-прежнему диспетчеризуют исключительно через `IGV2DynamicScreenElement`/legacy-регистр — моста к `PrepareUiHostProperties`/`IGV2UiPropertyHost` на уровне экрана ещё нет. `grep` по `GameData/*/definitions/`, `GameData/*/scripts/` подтвердил — ни один существующий контент не объявляет эти схемы как top-level поля, поэтому это не действующая регрессия. Постройка этого моста — явно предмет **UPP-27** («`UGV2ScreenWidgetBase` переведён на подготовку полного плана мутаций до единственного Commit»), не этого этапа: M5 сознательно ограничен тем, чтобы сделать каждый виджет внутренне корректным и проверяемым в изоляции (что и проверено — все claims выше подтверждены), оставляя экранную оркестрацию M7.

## Проверка milestone

- [x] Отказ ребёнка не может дать успех родителя ни в одной коллекции или композите.
- [x] Отказ элемента не оставляет переиспользованные виджеты мутированными, и это проверяется значениями.
- [x] Позиционный ключ элемента невыразим схемой.
- [x] Ни один композит не строит binding-путь вручную.
