---
title: Measured Verification Tasks
status: active
version: 1.0
updated: 2026-08-23
depends_on:
  - README.md
  - ../../UI/UIDocumentAndReconciliation.md
---

# M2 — Measured Verification

> **Материализует:** findings CCF-AF-01, CCF-AF-03, GLS-AF-01, GLS-AF-03, UIF-AF-01, UIF-AF-04, UIF-AF-06.
> **Задачи:** SVC-05…09.
> **Результат:** проверки измеряют заявленную величину, а вакуумных тестов в наборе не остаётся.

## Результат этапа

Этап отвечает на вопрос, который проверка планов задавала каждому утверждению: упадёт ли хоть один тест, если утверждение перестанет быть истинным.

## Задачи

- [x] **SVC-05 — Матрица разрешений измеряет выделенную геометрию**
  - Все геометрические утверждения `GV2.Runtime.UI.LocationScreenViewportMatrix` читают `GetDesiredSize()`. Тест делает `Resize` и `SlatePrepass`, но `ArrangeChildren` не вызывает, а желаемый размер считается снизу вверх от содержимого и от размера окна не зависит. Поэтому сравнение ширины сцены на 1920×1080 и 2560×1080 сопоставляет два одинаковых числа, а порог «TopBar ≤ 25 % высоты» содержателен только на 720p.
  - Done: тест выполняет раскладку через `PaintWindow` и читает **выделенную** геометрию каждого блока; сравнение ширины сцены на 16:9 и 21:9 различает значения и падает, если дополнительная ширина уходит не сцене; проверка 720p опирается на фактические границы элементов, а не на число записей репитера; при этом проверяется, что все команды помещаются в выделенную область; порог верхней панели содержателен на всех шести разрешениях.
  - Evidence: `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

- [x] **SVC-06 — Снять вакуумный тест перехода**
  - Зависимости: нет.
  - `GV2.Runtime.Presentation.LocationTransitionFlow` называется «Transition Flow & Screen Instance Reuse» и перехода не выполняет: между чтением `ScreenBefore` и `ScreenAfter` нет ни одной команды. Утверждение о переиспользовании «across locations» истинно тривиально.
  - Done: вакуумный тест `LocationTransitionFlow` удалён как полностью дублирующий и уступающий `GV2.Runtime.UI.LocationScreenTransitionContract`, который выполняет реальный переход из Tavern в Market через сабмит команды `travel_city_market`, проверяет переиспользование инстанса UObject экрана (`Screen1 == Screen2`), проверяет обновление заголовка и фона сцены локации и отсутствие кнопки перехода в таверну на экране рынка.
  - Evidence: `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

- [x] **SVC-07 — Handle неактивной вкладки отклоняется**
  - Binding records создаются для всех вкладок — тест подтверждает две записи для двух вкладок. Путь `FGV2UiInteractionEmitter::Submit` → `SubmitUiInteraction` принимает только handle; `ActiveTabKey` за пределами виджета вкладок не используется. Кнопка скрытой вкладки остаётся вызываемой.
  - Done: Semantic Input принимает handle только активной вкладки и отклоняет остальные (включая случай несохранённой/пустой активной вкладки) как `StaleBindingHandle`; переключение вкладки через `UGV2TabContainerWidgetBase` синхронизирует состояние активной вкладки с рантаймом/координатором и меняет множество интерактивных handle, не меняя ревизию документа; правило и его причина зафиксированы в [Semantic Input](../../UI/SemanticInput.md); отрицательные и интеграционные случаи покрыты тестами.
  - Evidence: `Docs/UI/SemanticInput.md`, `Source/GV2/Private/UI/`, `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

- [x] **SVC-08 — Аварийные экраны: реализовать либо снять утверждение**
  - `core:screen.error`, `core:screen.loading` и `core:screen.recovery` не существуют нигде. Существует минимальная тема ядра со **строками** `core:text.screen.error.title` и подобными, и зелёный тест проверяет присутствие этих строк в каталоге — то есть более слабое свойство, чем отрисовка экранов.
  - Done: ложное утверждение о существовании экранов `core:screen.error`, `.loading`, `.recovery` снято из документации (`Docs/UI/README.md`); зафиксировано, что аварийной поверхностью отказа сессии является UE-native виджет `UGV2RecoveryScreenWidget`, который программно инициализируется при сбое bootstrap; аварийные строки темы `core:text.screen.recovery.title` и `core:text.screen.error.description` получили реального потребителя в C++ рантайме (`UGV2RuntimeSubsystem::StartSessionDirect`) через `UGV2TextPipeline::Resolve`; тест `GV2.Runtime.UI.ThemeOwnershipAndTextLengthContract` обновлен и проверяет как разрешение строк темы, так и инициализацию `UGV2RecoveryScreenWidget` с разрешенными строками.
  - Evidence: `Docs/UI/README.md`, `Source/GV2/Private/Runtime/GV2RuntimeSubsystem.cpp`, `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

- [ ] **SVC-09 — Тест отката композита**
  - `ApplyScreenFields` захватывает предыдущее значение каждого элемента и при отказе восстанавливает его — этим и обеспечивается требование «failed composite apply сохраняет предыдущие model и visuals». Откат тестом не покрыт: единственный тест отката касается браша изображения. Путь отката может отказать сам и лишь логируется.
  - Done: тест доводит композит до отказа на втором ребёнке и проверяет, что и модель, и видимое состояние вернулись к прежним; отдельно проверяется поведение при отказе самого отката; при желании применить преflight в производственных вызовах `ReconcileEntries` это делается здесь же и покрывается тем же тестом.
  - Evidence: `Source/GV2/Private/Tests/GV2RuntimeSubsystemTests.cpp`.

## Проверка milestone

- [x] Матрица различает 16:9 и 21:9 по фактической геометрии.
- [x] На 720p проверяется размещение команд, а не их количество.
- [x] Вакуумных тестов в наборе не осталось.
- [x] Handle неактивной вкладки отклоняется.
- [x] Судьба аварийных экранов решена и записана.
- [ ] Откат композита покрыт тестом.
