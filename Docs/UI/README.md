---
title: UI Documentation Index
status: normative
version: 2.0
updated: 2026-09-09
---

# UI Documentation

UI является перестраиваемой presentation projection. Lua определяет desired Screen instances, Screen Fields и gameplay-значимые доступные Commands; Unreal Blueprint Screen Templates реализуют composition, layout, animation, focus, hover и rendering.

Любой runtime-authored display text проходит единый `TextSpec → localization → validated markup → theme typography → renderer` pipeline. Widget и Lua не выбирают raw font assets, RGB или numeric font sizes.

## Reading order

1. [ScreenTemplates.md](ScreenTemplates.md)
2. [WidgetRegistry.md](WidgetRegistry.md)
3. [UIDocumentAndReconciliation.md](UIDocumentAndReconciliation.md)
4. [ImageResources.md](ImageResources.md)
5. [SemanticInput.md](SemanticInput.md)
6. [PresentationSnapshotAndEffects.md](PresentationSnapshotAndEffects.md)

Общие термины: [../Architecture/GlossaryAndNaming.md](../Architecture/GlossaryAndNaming.md). Command semantics: [../Architecture/CommandsAndEvents.md](../Architecture/CommandsAndEvents.md).

## Core invariants

- Lua не создаёт Widget и не вызывает Blueprint function по имени.
- Lua не описывает physical Widget tree: он выбирает `screen_id` и передаёт полный набор schema-validated Screen Fields.
- Concrete Screen Blueprint наследует общий abstract base; добавление screen не требует per-screen C++ branch.
- Blueprint не меняет canonical state и не отправляет gameplay event.
- Physical Widget публикует opaque `binding_handle`; Semantic Input Adapter резолвит его и пересекает Lua boundary только с current bound `command_id`.
- UI-document передаётся целиком; patch protocol отсутствует.
- Reconciliation может переиспользовать physical Screen Widget по stable `instance_key` при неизменном `screen_id`.
- Removed/stale screen/element перестаёт принимать input до завершения exit animation.
- Text использует `text_id`/arguments, assets — `resource_id`.
- Image resources используют только `fixed_aspect`, `nine_slice` или `tile`; physical rendering metadata не пересекает Lua boundary.
- Hover, pressed, focus, tooltip и cosmetic animation остаются UE-local.
- Interactive RichText использует localized semantic tag + separate span descriptor; hover открывает UE-local popover, click отправляет только opaque binding handle.
- Default visual styles reusable components разрешаются через один configured `UGV2UiTheme`; Lua theme не выбирает.
- Любой `WBP_*` с direct text primitive обязан использовать Text Pipeline native adapter; raw-`FText` runtime apply API запрещён и проверяется automation-тестом по полному `/Game/UI` inventory.
- Composite Widget обязан составлять UI из approved leaf adapters. Собственные parallel paths для runtime text, content image, repeated-child construction или Semantic Input запрещены.
- Screen publication использует generic Screen Field envelopes и становится input-ready только после atomic field apply и binding commit.

## Three Layers of UI Ownership (ADR-0030, ADR-0035)

Компоненты и ассеты UI строго распределены по трём слоям:

1. **`core` (движок)**: визуальные примитивы, базовые контейнеры раскладки и конвейеры. Не зависят от жанра и правил.
   - Примитивы: `Button`, `Text`, `Image`, `Checkbox`, `InputField`, `DropdownSelect`, `Separator`, `LoadingIndicator`, `ProgressBar`, `Icon`.
   - Контейнеры и списки: `Panel`, `ScrollArea`, `ListView`.
   - Конвейеры: Text Pipeline, Image Presentation, Keyed Collection.
2. **`textsystem` (текстовый движок)**: общие композиты и шаблоны экранов для любых текстовых RPG.
   - Композиты: `RichText`, `ButtonList`, `Modal`, `Portrait`, `Dialog`, `Inventory`.
   - Шаблоны: `LocationScreen`, `DialogueScreen`, `ErrorScreen`, `LoadingScreen`, `RecoveryScreen`.
3. **`rh` (игра)**: специфичные композиты и экраны конкретной игры (Red Hood).

Критерий трёх вопросов применяется до реализации любого композита:
1. Нужен ли в любой игре вообще? → `core`.
2. Специфичен ли для текстовых RPG, но не для конкретного лора? → `textsystem`.
3. Принадлежит ли конкретно Red Hood? → `rh`.

Переиспользуемость внутри одного слоя не является основанием поднимать композит на уровень выше.

### Каталоги ассетов слоев и разрешение темы

Ассеты строго структурированы по каталогам своих слоев:
- `Content/UI/` — инфраструктура `core`: Game Shell (`Content/UI/Shell/`), реестр (`Content/UI/Registry/DA_ScreenRegistry`), `WBP_ScreenBase` (`Content/UI/Screens/`), базовые примитивы (`Content/UI/Widgets/`: `WBP_Text`, `WBP_Image`, `WBP_Button`, `WBP_Checkbox`, `WBP_InputField`, `WBP_DropdownSelect`, `WBP_ProgressBar`, `WBP_Separator`, `WBP_LoadingIndicator`) и тестовый экран `core:screen.test` (`WBP_Testscreen`, namespace `core`).
- `Content/TextSystem/UI/` — тема и стили (`Content/TextSystem/UI/Styles/`: `DA_UITheme_Default`, `BP_UIStyle_*`) и композиты (`Content/TextSystem/UI/Widgets/`: `WBP_RichText`, `WBP_RichTextPopover`, `WBP_ButtonList`, `WBP_Modal`, `WBP_Portrait`).
- `Content/RH/UI/` — специфичные стили и экраны `rh`; открывается по факту появления первого ассета этого слоя.

Правило проверяется реестром экранов (`UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace`, `Validate`): экран нижнего слоя не может ссылаться на ассет из верхнего слоя.

Активная тема выбирается конфигом `Config/DefaultGame.ini` (`UGV2UiThemeSettings.ThemeAsset`), но резолюция принадлежит построению session content snapshot (`ADR-0043` D1) и происходит один раз за сессию, не заново на каждый Apply — целевое правило, закрывающее `PAH-R1`. Сессия, чья настроенная тема не разрешается, **не стартует** (`ThemeNotReady`) и показывает экран восстановления: подмена настроенной темы минимальной запрещена, потому что она превращает ошибку конфигурации в тихо работающую подменную презентацию.

Программно собранная в C++ минимальная тема ядра (`UGV2UiTheme::GetCoreMinimalTheme()`, не отдельный ассет) допускается ровно в двух structurally различных ролях:

1. **Нативный экран восстановления `UGV2RecoveryScreenWidget`** — он показывается именно тогда, когда у сессии нет snapshot вовсе (bootstrap ещё не дошёл до `Ready` либо сессия уже `Failed`), и берёт из минимальной темы базовую типографику и аварийные строки каталога (`core:text.screen.recovery.title`, `core:text.screen.error.*`). Это bootstrap/failure-time fallback без snapshot, а не повторное разрешение snapshot-owned Theme.
2. **Построение самого snapshot** — candidate builder один раз резолвит минимальную тему и пришпиливает её в `FGV2ResolvedUiTheme::FallbackTheme` как текстовый fallback сессии: `text_id`, которого нет ни в `TextCatalog`, ни в `FallbackTextCatalog` настроенной темы, разрешается из неё. Это не второй authority и не runtime pull — значение разрешается там же, где и остальные поля snapshot, и downstream видит его исключительно как обычное prepared value через `FGV2PresentationPrepareContext`; ни один Commit-facing путь к `GetCoreMinimalTheme()` не обращается.

Обе роли — и только они — допускаются production call-site inventory (`Tools/Testing/validate_central_style_runtime_boundary.py`).
