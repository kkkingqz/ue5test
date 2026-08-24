---
title: Remaining Leaves Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - ProvingSliceAndGate.md
  - ../../UI/WidgetRegistry.md
---

# M4 — Remaining Leaves

> **Материализует:** оставшуюся часть фазы 3 proposal, разделы 22.5…22.12, 27.3.
> **Задачи:** UPP-16…19.
> **Результат:** ни один листовой виджет не имеет собственного адаптера и собственной пары `CanApply`/`Apply`.

## Результат этапа

Механизм доказан на трёх элементах. Этап распространяет его на остальные листья и снимает поверхность, существовавшую только ради старой модели.

Порядок задач внутри этапа перестановочен: листья независимы. Единственное жёсткое требование — все листья закончены до M5, потому что композит нельзя мигрировать раньше своих детей.

## Задачи

- [x] **UPP-16 — Checkbox и InputField**
  - Зависимости: UPP-15.
  - `is_read_only` и `max_length` были удалены из принимаемых ключей как не имеющие потребителя. Схема обязана их вернуть уже настоящими свойствами, а не снова объявлением без реализации.
  - Done: оба виджета мигрированы; `placeholder_text` идёт через `UGV2TextPipeline`, а не через сырой `SetHintText` — тест краснеет при возврате прямой установки; `is_read_only` и `max_length` реализованы как свойства с наблюдаемым эффектом и покрыты harness-ом; попытка объявить их capability без наблюдаемого эффекта ломает сборку; адаптеры, DTO и ветки union удалены.
  - Evidence: `Source/GV2/Public/UI/GV2CheckboxWidgetBase.h`, `Source/GV2/Public/UI/GV2InputFieldWidgetBase.h`, `Source/GV2/Private/Application/GV2ScreenFieldAdapterRegistry.cpp`.

- [x] **UPP-17 — ProgressBar и Portrait**
  - Зависимости: UPP-15.
  - Done: оба мигрированы; `percent` объявлен свойством с диапазоном в схеме, а не проверяется вручную в адаптере — числовой подтип обрабатывается схемой, нечисловое значение отклоняется, значение вне диапазона обрабатывается объявленной политикой; метка ProgressBar остаётся в текстовом конвейере; Portrait требует привязанного renderer target в Prepare, а не возвращает успех; `style` не возвращается в схему как принимаемое-и-игнорируемое; адаптеры и DTO удалены.
  - Evidence: `Source/GV2/Public/UI/GV2ProgressBarWidgetBase.h`, `Source/GV2/Public/UI/GV2PortraitWidgetBase.h`.

- [x] **UPP-18 — RichText: листовая часть и popover**
  - Зависимости: UPP-15.
  - Сегодня при недоступном классе popover hover-содержимое либо не показывается, либо ранее подставлялось в обход центральной стилизации. И то и другое — следствие того, что проверка происходит во время отрисовки, а не до применения.
  - Done: `UGV2RichTextWidgetBase` мигрирован в части простого содержимого и стиля; отсутствие обязательного renderer для popover является **отказом Prepare**, а не поведением во время наведения; сырой fallback отсутствует и не может вернуться — тест краснеет на прямом создании текстового виджета в обход конвейера; интерактивные span-ы остаются за UPP-22.
  - Evidence: `Source/GV2/Public/UI/GV2RichTextWidgetBase.h`, `Source/GV2/Public/UI/GV2RichTextPopoverWidgetBase.h`.

- [x] **UPP-19 — Снятие листовой legacy-поверхности**
  - Зависимости: UPP-16, UPP-17, UPP-18.
  - Обратной совместимости нет, поэтому парная поверхность старой модели удаляется сразу, а не помечается устаревшей.
  - Done: у мигрированных листьев удалены триады `CanApplyXxx`/`ApplyXxx`/`CaptureXxx` — их роль выполняет lifecycle хоста свойств; удалены варианты `ApplyOptionalImageResource` и `ApplyOptionalPortrait` — политика подстановки заглушки выражена свойством схемы, а не второй перегрузкой C++; `BindWidgetOptional` заменён на обязательную привязку там, где схема объявляет свойство обязательным, и это проверяется на экземпляре; остатки `DeprecatedProperty` удалены; ассеты приведены через `unreal-mcp`; гейт убывания показал снижение.
  - **2026-08-24, аудит и завершение:** `grep` по `CanApplyScreenField|CaptureScreenField|ApplyScreenField|GetScreenFieldDescriptor` подтвердил — ни один из восьми мигрированных листьев (Text, Image, Button, Checkbox, InputField, ProgressBar, Portrait) не реализует `IGV2DynamicScreenElement`; триады были корректно убраны в собственных задачах миграции (UPP-12…18), само по себе UPP-19 не нашло их остатков. Найдено и исправлено: (1) `AcceptedRenderMode_DEPRECATED` в `GV2ImageWidgetBase.h` — неиспользуемый остаток `DeprecatedProperty`, удалён; (2) `PortraitImage` в `GV2PortraitWidgetBase.h` был `BindWidgetOptional`, хотя `resource_id` обязателен в `core:schema.ui_field.portrait.v1` — заменён на `BindWidget` (несовпадающий ассет теперь не компилируется как Blueprint, а не молча остаётся без renderer); `Checkbox`/`ProgressBar`/`Text`/`Image`/`Button` уже были корректны (обязательные renderer targets — `BindWidget`, опциональные — `BindWidgetOptional`, соответствуют `required` в своих схемах). `ApplyOptionalImageResource`/`ApplyOptionalPortrait` **не удалены** — `GV2LocationCompositeWidgetBases.cpp` (LocationScene/LocationPlayerStatus, немигрированные composite до M6/UPP-24…26) продолжает вызывать их напрямую в обход Prepare/Commit; удаление сейчас сломало бы работающий код ради несуществующей замены. Это не нарушает «двойной стек только между элементами»: `UGV2ImageWidgetBase`/`UGV2PortraitWidgetBase` сами не реализуют старый интерфейс — это немигрированный родитель вызывает чистый public-метод мигрированного листа. Явно отложено до миграции LocationComposite. Полный `GV2.*` прогон (92/92 зелёных) и гейт убывания (20 функций / 8 payload-членов / 16 DTO, снижение с 28/12/19 после UPP-15) подтверждены после обеих правок.
  - Evidence: `Source/GV2/Public/UI/GV2ImageWidgetBase.h`, `Source/GV2/Public/UI/GV2PortraitWidgetBase.h`, `Tools/Content/validate_ui_pipeline_legacy_gate.py`.

## Проверка milestone

- [x] Ни один листовой виджет не имеет schema-specific адаптера.
- [x] `is_read_only` и `max_length` реализованы, а не объявлены.
- [x] Ни один лист не применяет текст или изображение в обход централизованных конвейеров.
- [x] Гейт убывания монотонно снижается на каждой задаче этапа (30→28→20 функций, 13→12→8 payload-членов, 20→19→16 DTO).
