---
title: UniversalUiPropertyPipeline Archive Summary
status: archived
version: 1.0
updated: 2026-08-27
---

# UniversalUiPropertyPipeline: итог выполнения

> **Материализует:** исторический итог выполненного плана; документ не является источником правил или задач.

## Цель и результат

**Цель:** заменить schema-specific цепочку `PrepareXxx` → `BuildXxx` → `FGV2XxxViewModel` → `ApplyXxx` одним универсальным property pipeline с data-driven схемами и раздельными фазами Prepare/Commit. Цель формулировалась не как устранение списка дефектов — они были устранены точечно до начала плана — а как прекращение его пополнения: за четыре раунда проверки одно семейство «значение пересекло границу и молча исчезло» дало четыре экземпляра, и частота точечных починок не падала.

**Результат:** UI-схема объявляется данными и компилируется переносимо, без UE; значение после границы существует как единое подготовленное дерево; виджет объявляет capability, каждая из которых обязана быть наблюдаемой; вся отказоспособная работа вынесена в Prepare, а Commit выполняет только подготовленные мутации; отказ ребёнка структурно не может дать успех родителя; замена экранов в слоях атомарна; `FGV2ScreenFieldAdapterRegistry`, union payload `FGV2ScreenFieldValue` и schema-specific DTO удалены, а гейт монотонного убывания доведён до нуля и превращён в постоянный запрет.

Закрыты `STATUS-003` (невыразимость «политика масштабирования не объявлена») и `STATUS-004` (предиктивный глубокий preflight). Принято решение [ADR-0040](../../ADR/0040-universal-ui-property-pipeline.md); [ADR-0038](../../ADR/0038-screen-field-value-flat-struct.md) заменён им, а не пересмотрен по своему условию: его предмет удалён целиком.

## Этапы и задачи

### M1 — Decisions and Schema Infrastructure

UI-схема стала объектом: объявляется данными, компилируется, валидируется переносимым кодом.

- `UPP-01` — ADR: универсальный UI property pipeline
- `UPP-02` — `schema_domain` и стандартные UI-kinds в компиляторе схем
- `UPP-03` — `schema_ref` и обнаружение циклов
- `UPP-04` — Переносимый валидатор значения
- `UPP-05` — Публикация UI-схем в репозитории и владение namespace
- `UPP-06` — Контракты объявляют схемы UI данными

### M2 — Prepared Values and Property Host

Несущая конструкция: дерево значений, хост свойств, consumers, Prepare/Commit и средство измерения.

- `UPP-07` — Подготовленное дерево значений
- `UPP-08` — `IGV2UiPropertyHost` и дескриптор capability
- `UPP-09` — Стандартные consumers свойств
- `UPP-10` — Prepare/Commit и наблюдаемый отказ Commit
- `UPP-11` — Harness наблюдаемости capability и гейт убывания legacy

### M3 — Proving Slice and Gate

Три элемента, покрывающие текст, ресурс и binding, плюс явное решение о продолжении.

- `UPP-12` — Text: миграция и удаление адаптера
- `UPP-13` — Image/Icon: миграция и закрытие `STATUS-003`
- `UPP-14` — Button: миграция и первый binding через generic traversal
- `UPP-15` — Гейт go/no-go

### M4 — Remaining Leaves

- `UPP-16` — Checkbox и InputField
- `UPP-17` — ProgressBar и Portrait
- `UPP-18` — RichText: листовая часть и popover
- `UPP-19` — Снятие листовой legacy-поверхности

### M5 — Collections and Composites

- `UPP-20` — Keyed collection consumer и настоящая транзакционность
- `UPP-21` — ButtonList и DropdownSelect
- `UPP-22` — RichText spans и Modal
- `UPP-23` — TabContainer и вложенный экран

### M6 — LocationScreen

- `UPP-24` — TopBar и PlayerStatus
- `UPP-25` — Scene и CommandPanel
- `UPP-26` — Приведение контента и Lua

### M7 — Screen and Document Transaction

- `UPP-27` — Экран на Prepare/Commit и закрытие `STATUS-004`
- `UPP-28` — Атомарная замена экранов в слоях
- `UPP-29` — Координатор сессии и реестр биндингов на подготовленном commit

### M8 — Teardown and Closure

- `UPP-30` — Снятие каркаса и постоянный запрет
- `UPP-31` — Контракты и статус приведены к реализации
- `UPP-32` — Сверка закрытий

## Что дал гейт наблюдаемости

Требование «capability обязана быть наблюдаемой» было добавлено в proposal при его рассмотрении именно потому, что инвариант `SchemaContract ⊆ WidgetCapabilities` сам по себе класс дефектов не закрывает: объявление capability пишется руками так же, как список `ConsumedKeys`, и ложь может просто переехать из адаптера в таблицу capability.

Это подтвердилось на практике дважды.

**Первый раз** — при сверке `UPP-32`: harness прогонялся только на 10 из 18 классов `IGV2UiPropertyHost`, и непокрытыми оставались ровно композиты. Пробел был честно записан как `STATUS-005`, а не скрыт.

**Второй раз** — при закрытии `STATUS-005`: расширение sweep на оставшиеся классы немедленно нашло **живой дефект целевого класса внутри нового pipeline**. `UGV2ModalWidgetBase` объявлял capability `key` и имел `SetKey`, но в `FGV2KeyPropertyConsumer::Commit` ветки для него не было — цепочка `Cast` доходила до безусловного `return true`. Ключ принимался, применение сообщало успех, значение не сохранялось. Дефект проверен откатом: с удалённой веткой sweep краснеет с точным диагнозом, с восстановленной — зелёный.

Вывод, который стоит сохранить: **generic-механизм не устраняет класс дефектов сам по себе — его устраняет средство измерения, приложенное ко всей поверхности.** Механизм без полного sweep воспроизводит ту же ошибку в новой форме.

## Принятые по ходу решения

- **`UPP-15` был настоящей остановкой, а не формальностью.** Пять условий гейта оценивались по правилу «тест краснеет на внесённой регрессии», и центральная проверка совместимости schema/capability подтверждена реальным откатом: проверка временно отключалась, `GV2.UI.StandardPropertyConsumers` краснел, проверка восстанавливалась.
- **Отсутствие обратной совместимости использовано как рычаг.** Виджет мигрировал вместе со своим контентом и своим адаптером в одном change set; двойной стек существовал между элементами, а не внутри одного; фаза 7 свелась к снятию каркаса вместо cutover.
- **Неизвестный тип целевого виджета в consumer — типизированный отказ, а не тихий успех.** Цепочки `Cast`, заканчивающиеся безусловным `return true`, и есть механизм, породивший дефект Modal.

## Проверка

Полный регрессионный прогон на итоговом коммите: **94/94** UE automation, **68/68** портативных CTest, `gv2-headless --self-test`, `validate_docs.py` (167 файлов) — без ошибок.

## Актуальные нормативные источники

- [ADR-0040](../../ADR/0040-universal-ui-property-pipeline.md)
- [ScreenTemplates](../../UI/ScreenTemplates.md)
- [UIDocumentAndReconciliation](../../UI/UIDocumentAndReconciliation.md)
- [WidgetRegistry](../../UI/WidgetRegistry.md)
- [Implementation Status](../../Status/ImplementationStatus.md)

## Полная история

`source_commit`: [e782f571ecd4ec7b583d5f506160974f628f1713](https://github.com/kkkingqz/ue5test/commit/e782f571ecd4ec7b583d5f506160974f628f1713)

[Полный каталог плана на source commit](https://github.com/kkkingqz/ue5test/tree/e782f571ecd4ec7b583d5f506160974f628f1713/Docs/Plans/UniversalUiPropertyPipeline) содержит исходные task-файлы, acceptance criteria и evidence.
