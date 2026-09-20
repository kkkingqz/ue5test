# uectl в составе GV2

Внешний пакет, распакованный сюда как есть. **Файлы пакета не правятся:** любая
правка ломает сверку с апстримом и превращает вендоринг в форк.

- Версия: `0.2.2`, из `uectl-package-0.2.2.zip`.
- Дата распаковки: 2026-09-20.
- Назначение в GV2: клиент `ue-build-service` и `ue-editor-control` для контейнерного
  контура — см. [ContainerizedExecutionEnvironmentProposal](../../Docs/Proposals/ContainerizedExecutionEnvironmentProposal.md).

Требования GV2 к инфраструктуре живут рядом, в [`GV2-REQUIREMENTS.md`](GV2-REQUIREMENTS.md):
это документ GV2, а не апстрима, поэтому он правится свободно.

## Чем это не является

Пакет **не участвует** в сборке и проверках GV2: он не зарегистрирован в `CMakeLists.txt`,
не входит в портативный CTest и не сканируется гейтами. Его собственные 63 regression-теста
требуют `pytest`, которого в окружении GV2 нет, и запускаются отдельно по инструкции в
[`README.md`](README.md).

Проверено при распаковке без pytest: синтаксис всех `.py` разбирается, `uectl version`
возвращает `0.2.2`, `--help` перечисляет команды. Это smoke, а не прогон его тестов.

## Дубликаты внутри пакета

`uectl` и `uectl.py` побайтно идентичны, как и `ue-editor-control` с `ue_editor_control.py`:
в пакете есть и исполняемая, и импортируемая форма. Дубликаты сохранены намеренно — пакет
вендорится целиком, а не выборочно.

## Что относится к GV2, а что к апстриму

| Вопрос | Где решается |
|---|---|
| Контракт `BUILD-SERVICE-API-v1.md`, поведение CLI, daemon | Апстрим пакета |
| Что GV2 требует от контура исполнения | [`GV2-REQUIREMENTS.md`](GV2-REQUIREMENTS.md) — нумерованные требования, включая требования к `ue-build-service` |
| Какая проверка в каком контуре исполняется | [Build and Tooling § Где какая проверка исполняется](../../Docs/Architecture/BuildAndTooling.md) |
| Решения по стыкам (полный лог, шаблон приёмки, Editor API, отказ от worktree) | [ContainerizedExecutionEnvironmentProposal](../../Docs/Proposals/ContainerizedExecutionEnvironmentProposal.md) |
| Переносимость приёмочного evidence | План [AcceptanceEvidencePortability](../../Docs/Plans/Archive/AcceptanceEvidencePortability.md) |
