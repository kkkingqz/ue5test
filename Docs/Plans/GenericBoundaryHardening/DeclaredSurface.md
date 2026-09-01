---
title: Declared Surface Tasks
status: active
version: 1.2
updated: 2026-09-01
depends_on:
  - README.md
  - ../../Status/GV2_remaining_review_2026-09-01.md
  - ../../UI/ScreenTemplates.md
---

# M1 — Declared Surface

> **Материализует:** `REM-03`, `REM-05`, `REM-04`, `REM-06`, `REM-07`.
> **Задачи:** GBH-01…05.
> **Результат:** всё, что можно объявить, работает; структурный отказ предсказуем до мутации.

## Результат этапа

Этап дешёвый: механизмы для всех задач уже существуют, не хватает их применения.

`HasHostForLayer` написан и реконсилятором не используется, хотя список входящих слоёв известен в `PrepareReconcile` до любой мутации. `CollectionHost` выбирается в Designer и не может создать элемент. Гейт по видам значения существует с `PCC-05` — недостаёт симметричного по видам Designer.

Большая часть задач независима, но `GBH-02` имеет две части: **A** немедленно убирает неготовый `CollectionHost` из selectable Designer surface, **B** возвращает его только после `GBH-06…08`. Это намеренно разрывает прежний цикл `GBH-02 ↔ GBH-06`.

## Задачи

- [ ] **GBH-01 — Структурная непригодность Shell предсказывается в Prepare**
  - `CommitReconcile` присоединяет экраны последовательно; отказ на втором оставляет первый в дереве, новый `ActiveScreens` не публикуется, и возникает расхождение логического и физического состояния. Остаток известен и записан комментарием в `GV2LayeredUiReconciler.cpp`, но не вынесен ни в статус, ни в проверку.
  - Done: `PrepareReconcile` до построения плана публикации проверяет для каждого incoming layer/screen все предсказуемые structural prerequisites `AttachScreenToLayer` (как минимум authored host, допустимость layer, совместимость target и любые другие ветви, способные штатно вернуть `false`) и отказывает типизированной диагностикой **до первой live mutation**; после успешного Prepare `AttachScreenToLayer` на подготовленном плане считается invariant-level infallible — это подтверждено аудитом всех его `false`-ветвей и negative tests, а не предположением по `HasHostForLayer`; тест строит ситуацию, где первый attach был бы успешен, второй structural attach — неуспешен, и доказывает, что теперь весь документ отклоняется в Prepare и Shell tree не меняется; если остаётся непредсказуемый engine-level failure после начала attach, его recovery явно делегирован `GBH-09/10`; комментарий о допустимом partial Shell tree удалён.
  - Evidence: `Source/GV2/Private/UI/GV2LayeredUiReconciler.cpp`, `Source/GV2/Public/UI/GV2GameShellWidgetBase.h`, `Source/GV2/Private/Tests/`.

- [ ] **GBH-02 — Каждый selectable вид Designer работоспособен целиком**
  - `EGV2DeclaredUiCapabilityKind::CollectionHost` объявляется через `AddCustom`, который не задаёт ни `EntryWidgetClass`, ни ключевое свойство; consumer коллекции без класса элемента создать первый элемент не может. Автор контента видит вариант, который не работает — тот же класс дефектов, что `AddObject` до `PCC-05`, но на Designer surface.
  - **Часть A — до `GBH-06`:** ввести completeness gate по всем значениям `EGV2DeclaredUiCapabilityKind`. Любой kind без доказанного end-to-end пути **не может оставаться selectable**: он Hidden/удаляется из enum surface (или editor customization делает его недоступным) и имеет явную reason-code запись. Простого «списка неготовых» при сохранённой возможности выбрать kind недостаточно. `CollectionHost` временно скрывается, если его полный contract ещё не реализован. Для `RichTextSpans` и `NestedScreen` решение принимается по тому же правилу, а не отдельным исключением.
  - **Часть B — после `GBH-06…08`:** `CollectionHost` возвращается в selectable surface только если declaration несёт `EntryWidgetClass`, `KeyPropertyName` и item contract/selected child capability, а test проходит путь **из пустой коллекции**: Designer declaration → schema/materialization → создание первого entry → child capability subset check → Commit → observable renderer state. Если такой контракт не нужен проекту, kind остаётся Hidden/удалён и это считается полным closure `REM-05`.
  - Done: gate не позволяет добавить новый Designer kind без `Supported+E2E` либо `Hidden/Inapplicable`; selectable kind всегда имеет сквозной положительный и отрицательный тест; `CollectionHost` не может быть одновременно selectable и не способным создать первый элемент пустой коллекции.
  - Зависимости: часть A — нет; часть B — `GBH-06`, `GBH-08`.
  - Evidence: `Source/GV2/Public/UI/GV2DeclaredCompositeWidgetBase.h`, `Source/GV2/Private/Tests/`.

- [ ] **GBH-03 — Судьба принадлежности UI-схем репозиторию решена**
  - `FGV2UiSchemaCache` сканирует файловую систему и не является частью pinned `GameDataRepository`; существуют две несовпадающие проекции набора пакетов. Отложено как `UPP-R5` с условием «становится обязательным, когда блоки начнут поставляться модами». Условие не наступило, но разрыв живёт только в тексте архивных сводок, а не там, где его читают при планировании.
  - Done: принято и записано одно из двух. **A — реализовать:** UI-схемы входят в closure pinned repository, package rejection policy выполняется до publication repository, а runtime materializer резолвит schema только из этого authority. **B — defer:** разрыв получает `STATUS-NNN` с наблюдаемым условием повторного открытия; `ScreenTemplates.md` и `UIDocumentAndReconciliation.md` явно описывают текущий filesystem authority; ADR-0040 получает implementation caveat либо отдельный follow-up ADR, временно сужающий применимость Decision 6/7 до момента наступления trigger — историческое решение не переписывается молча, но текущая normative chain не утверждает реализованную mod isolation. Для defer closure проверяется docs/status consistency gate; искусственный runtime red test отсутствующей feature не требуется. Выбор обоснован и не делается по умолчанию.
  - Evidence: `Docs/Status/ImplementationStatus.md`, `Docs/UI/`, при реализации — `Source/GV2/`.

- [ ] **GBH-04 — Контракты описывают фактический host API**
  - `ScreenTemplates.md` утверждает, что configured element объявляет `schema_id` и политику обязательности, тогда как `IGV2ScreenFieldHost` содержит только `GetScreenFieldId()`, а `schema_id` приходит из runtime envelope.
  - Done: контракт описывает фактический интерфейс и фактический источник `schema_id`; если политика необязательного host нужна как поведение — она либо реализована, либо описана как отсутствующая с записью в статусе; выборочно проверено, что ни одно оставшееся утверждение контракта об элементах экрана не сильнее того, что подтверждает тест.
  - Evidence: `Docs/UI/ScreenTemplates.md`, `Source/GV2/Public/UI/GV2ScreenFieldHost.h`.

- [ ] **GBH-05 — Параллельные optional-пути применения удалены**
  - `ApplyOptionalImageResource` и `ApplyOptionalPortrait` остались в публичном API, хотя `ADR-0040` определял политику подстановки заглушки как свойство схемы и ожидал удаления параллельных путей. Это не дефект времени выполнения, а поверхность, из которой вырастает второй путь презентации.
  - Done: проверены все места вызова; при отсутствии production-авторитета методы удалены вместе с тестами и упоминаниями старого пути; если какой-то вызов остаётся необходимым, он переведён на путь схемы, а не сохранён исключением; гейт запрета legacy-поверхности из `PCC` расширен на эти имена.
  - Evidence: `Source/GV2/Public/UI/GV2ImageWidgetBase.h`, `Source/GV2/Public/UI/GV2PortraitWidgetBase.h`, `Source/GV2/Private/Tests/`.

## Проверка milestone

- [ ] Ни одна **предсказуемая** причина отказа attach не доживает до Commit: она отклоняется в `PrepareReconcile` до первой live mutation, и это подтверждено аудитом всех `false`-ветвей `AttachScreenToLayer`.
- [ ] Остаточный непредсказуемый engine-level отказ attach явно делегирован `GBH-09/10` записью в задаче, а не оставлен комментарием в коде. Утверждение «частично присоединённое дерево невозможно» становится полностью истинным только после M3 и там же проверяется.
- [ ] Ни один вид Designer не является одновременно selectable и неработоспособным: часть A скрывает такие виды, и гейт не позволяет добавить новый вид в обход этого правила.
- [ ] `CollectionHost` на момент закрытия M1 либо скрыт, либо, если его контракт не нужен проекту, удалён окончательно — работоспособным он становится только в `GBH-02B` после `GBH-08`.
- [ ] Разрыв принадлежности схем либо закрыт, либо записан там, где его читают при планировании, и отражён во всей нормативной цепочке.
- [ ] Ни один контракт не описывает поверхность, которой нет.
