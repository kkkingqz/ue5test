# UECTL и архитектура контейнеров Unreal Engine

**Статус:** актуализированный дизайн; Build Service API v1 зафиксирован в `BUILD-SERVICE-API-v1.md`  
**Целевая платформа:** Unraid  
**Основной сценарий:** разработка проектов Unreal Engine через универсальный Agent Canvas с внешними сервисами сборки UE и Unreal Editor  
**GPU:** NVIDIA RTX 3060, назначенная контейнеру UE Editor через NVIDIA Container Toolkit  

---

## 1. Цели

Система должна позволять универсальному Agent Canvas работать с большим количеством несвязанных проектов, при этом LLM-агенты, работающие с проектами Unreal Engine, должны иметь возможность:

- редактировать исходный код Unreal Engine-проекта;
- редактировать плагины проекта, включая плагины с модулями типа `Editor`;
- читать исходники и заголовочные файлы Unreal Engine в режиме только для чтения;
- собирать цели проекта `Editor`, `Game`, `Client` и `Server`;
- отдельно собирать и упаковывать Unreal-плагины;
- запускать Unreal Automation Tests;
- выполнять cook и package проекта;
- получать полный вывод компилятора/сборки и итоговый exit code;
- собирать изолированные Git worktree, используемые параллельными автономными агентами;
- выполнять всё перечисленное без доступа Agent Canvas к Docker socket, shell хоста Unraid и без права записи в установленный Unreal Engine.

Unreal Editor должен запускаться отдельно как от Agent Canvas, так и от сервиса сборки.

---

## 2. Что не входит в первую версию

Первая реализация **не должна**:

- предоставлять произвольное выполнение команд внутри контейнера сборки UE;
- позволять LLM-агентам напрямую управлять Unraid или Docker;
- заменять UnrealBuildTool или AutomationTool;
- использовать OAuth-токены Codex, Claude или Gemini вне их штатных клиентов;
- предоставлять RPC для Live Coding или полноценного управления Editor из Agent Canvas;
- по умолчанию позволять модифицировать исходное дерево Epic Engine;
- превращать контейнер Agent Canvas в UE development image.

Поддержку изменения исходников Engine можно добавить позднее как отдельный, явно включаемый build profile.

---

# 3. Архитектура верхнего уровня

```text
                                  Unraid

  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  Agent Canvas                                                    │
  │  ┌───────────────────────────────────────┐                       │
  │  │ Codex / Claude / Gemini               │                       │
  │  │                                       │                       │
  │  │ /projects/...                         │                       │
  │  │ /worktrees/...                        │                       │
  │  │ /reference/UE/5.8/Engine/Source :ro   │                       │
  │  │                                       │                       │
  │  │ uectl build                           │                       │
  │  │ uectl plugin-build MyPlugin           │                       │
  │  └─────────────────┬─────────────────────┘                       │
  │                    │                                             │
  │                    │ private Docker network: ue-control          │
  │                    ▼                                             │
  │       ┌────────────────────────────────────┐                     │
  │       │ UE Build Service                   │                     │
  │       │                                    │                     │
  │       │ clang / lld / UBT / UHT            │                     │
  │       │ RunUAT / AutomationTool            │                     │
  │       │ /opt/ue/5.8                        │                     │
  │       │ /projects                          │                     │
  │       │ /worktrees                         │                     │
  │       │ /builds                            │                     │
  │       └────────────────────────────────────┘                     │
  │                                                                  │
  │       ┌────────────────────────────────────┐                     │
  │       │ UE Editor                          │                     │
  │       │                                    │                     │
  │       │ Selkies                            │                     │
  │       │ UnrealEditor                       │                     │
  │       │ Vulkan + RTX 3060                  │                     │
  │       │ NVENC                              │                     │
  │       │ /opt/ue/5.8                        │                     │
  │       │ /projects                          │                     │
  │       │ /ue-ddc                            │                     │
  │       └────────────────────────────────────┘                     │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

Три контейнера намеренно имеют разные обязанности:

| Контейнер | Назначение | UE установлен | GPU | Может изменять проект |
|---|---|---:|---:|---:|
| `agent-canvas` | LLM-сессии, редактирование, Git, обычные проекты | Нет | Нет | Да |
| `ue-build-service` | Компиляция UE, тесты, cook/package | Да | Нет | Только результаты сборки |
| `ue-editor` | Интерактивный Unreal Editor | Да | Да | Да |

---

# 4. Схема хранения на Unraid

Рекомендуемая логическая структура:

```text
/mnt/user/
├── UEProjects/
│   ├── MyGame/
│   └── OtherGame/
│
├── UEWorktrees/
│   ├── MyGame/
│   │   ├── codex-<id>/
│   │   └── claude-<id>/
│   └── OtherGame/
│
├── UEBuilds/
│   ├── MyGame/
│   └── OtherGame/
│
└── appdata/
    ├── agent-canvas/
    ├── ue-build-service/
    └── ue-editor/

/mnt/<fast-pool>/
├── UE/
│   ├── 5.7/
│   └── 5.8/
└── UE-DDC/
```

## 4.1 Share с UE-проектами

`UEProjects` должна размещаться на быстром SSD/NVMe pool и, где это возможно, быть настроена в Unraid как Exclusive Share.

Share содержит обычные Unreal-проекты:

```text
UEProjects/MyGame/
├── MyGame.uproject
├── Config/
├── Content/
├── Plugins/
├── Source/
├── Binaries/
├── Intermediate/
└── Saved/
```

## 4.2 Установка Engine

Установленный Unreal Engine хранится как постоянные данные на хосте и не входит в writable layers Docker:

```text
/mnt/<fast-pool>/UE/5.8
```

Он должен монтироваться в `ue-build-service` и `ue-editor` по одному и тому же пути:

```text
/opt/ue/5.8
```

Обычно Engine должен монтироваться в режиме read-only.

## 4.3 DDC

Derived Data Cache хранится отдельно от project shares:

```text
/mnt/<fast-pool>/UE-DDC
```

В Editor он монтируется как:

```text
/ue-ddc
```

Build Service также может монтировать этот каталог, если общий cache заметно ускоряет cook/package.

## 4.4 Worktree

Параллельные автономные сессии не должны выполнять сборку из одного рабочего дерева.

Каждая автономная сессия Canvas получает отдельный Git worktree в:

```text
/mnt/user/UEWorktrees/<project>/<workspace-id>
```

Примеры:

```text
/mnt/user/UEWorktrees/MyGame/codex-a17f/
/mnt/user/UEWorktrees/MyGame/claude-b944/
```

UE Build Service должен видеть то же дерево по тому же контейнерному пути, что и Agent Canvas.

---

# 5. Контракт bind mounts между контейнерами

Пути mount являются частью API-контракта между контейнерами и не должны зависеть от конкретного image.

## 5.1 Agent Canvas

```text
Host                                      Container
-----------------------------------------------------------------
/mnt/user/UEProjects                      /projects              rw
/mnt/user/UEWorktrees                     /worktrees             rw
/mnt/user/UEBuilds                        /builds                ro
/mnt/<fast-pool>/UE/5.8/Engine/Source     /reference/UE/5.8/...  ro
/appdata/agent-canvas                     application-specific   rw
```

Agent Canvas **не монтирует**:

```text
/var/run/docker.sock
/mnt/boot
/mnt/user целиком
/opt/ue с правом записи
корневую файловую систему Unraid
```

## 5.2 UE Build Service

```text
Host                               Container
------------------------------------------------------
/mnt/user/UEProjects               /projects          rw
/mnt/user/UEWorktrees              /worktrees         rw
/mnt/user/UEBuilds                 /builds            rw
/mnt/<fast-pool>/UE/5.8            /opt/ue/5.8        ro
/appdata/ue-build-service          /var/lib/uebuild   rw
```

Опционально:

```text
/mnt/<fast-pool>/UE-DDC            /ue-ddc            rw
```

## 5.3 UE Editor

```text
Host                               Container
------------------------------------------------------
/mnt/user/UEProjects               /projects          rw
/mnt/<fast-pool>/UE/5.8            /opt/ue/5.8        ro
/mnt/<fast-pool>/UE-DDC            /ue-ddc            rw
/appdata/ue-editor/home             /home/ubuntu       rw
```

Обычно Editor открывает только основное рабочее дерево проекта, а не worktree автономных агентов.

---

# 6. Модель UID/GID

Все контейнеры, которые записывают файлы проекта, должны работать с совместимыми UID/GID.

Рекомендуемая модель:

```text
PUID=<chosen unraid-compatible uid>
PGID=<chosen unraid-compatible gid>
```

Точные значения зависят от конкретной установки, но должно выполняться условие:

```text
Agent Canvas UID/GID == UE Build Service UID/GID == UE Editor UID/GID
```

Build Service не должен выполнять компиляцию от `root`.

Причина: каталоги

```text
Source/*.cpp
Intermediate/*
Binaries/*
Saved/*
```

должны оставаться доступными для записи всем предусмотренным контейнерам без последующего исправления ownership.

---

# 7. Docker-сеть

Используется отдельная внутренняя Docker-сеть:

```text
ue-control
```

Участники:

```text
agent-canvas
ue-build-service
ue-editor
```

`ue-build-service:8765` и `ue-editor-control:8766` не должны публиковаться в LAN Unraid. Selkies web UI может публиковаться отдельно согласно deployment policy.

Пример:

```yaml
networks:
  ue-control:
    internal: true
```

UE Editor не обращается к build API; он состоит в сети `ue-control`, чтобы Agent Canvas мог обращаться к узкому `ue-editor-control` API.

Agent Canvas обращается к сервисам через Docker DNS:

```text
http://ue-build-service:8765
http://ue-editor:8766
```

---

# 8. Дизайн UE Build Service

Build Service — это узкий RPC-сервис вокруг заранее поддерживаемых операций сборки Unreal.

Он **не является** удалённым универсальным shell.

## 8.1 Обязательные операции

Версия 1 должна реализовать:

```text
status
build
plugin-build
test
cook
package
clean
cancel
logs
```

Предлагаемый API:

```text
GET  /v1/status
POST /v1/jobs/build
POST /v1/jobs/plugin-build
POST /v1/jobs/test
POST /v1/jobs/cook
POST /v1/jobs/package
POST /v1/jobs/clean
GET  /v1/jobs/<job-id>
GET  /v1/jobs/<job-id>/logs?offset=<byte-offset>
POST /v1/jobs/<job-id>/cancel
```

## 8.2 Никакого endpoint для произвольных команд

Не реализовывать:

```text
POST /exec
POST /shell
POST /command
```

Каждая RPC-операция должна соответствовать заранее определённому серверному шаблону команды.

---

# 9. Идентификация проекта и проверка путей

Клиенту нельзя позволять передавать произвольный путь в файловой системе.

Плохой формат запроса:

```json
{
  "project_path": "/whatever/the/agent/wants"
}
```

Предпочтительный формат:

```json
{
  "project": "MyGame",
  "workspace": "main"
}
```

или:

```json
{
  "project": "MyGame",
  "workspace": "codex-a17f"
}
```

Сервер разрешает эти логические идентификаторы через проверенный registry:

```text
main
  -> /projects/MyGame

codex-a17f
  -> /worktrees/MyGame/codex-a17f
```

Правила валидации:

- имя проекта должно присутствовать в allowlist/registry;
- workspace ID должен соответствовать ограниченному набору допустимых символов;
- resolved path должен оставаться внутри настроенного корня project/worktree;
- выход через symlink за пределы разрешённого root должен блокироваться;
- если отдельно не настроено иное, в рабочем дереве должен существовать ровно один `.uproject`;
- версия Engine для проекта определяется серверной конфигурацией, а не произвольным параметром клиента.

---

# 10. Registry проектов

Build Service должен иметь небольшой декларативный registry.

Пример:

```yaml
projects:
  MyGame:
    engine: "5.8"
    project_file: "MyGame.uproject"
    editor_target: "MyGameEditor"
    game_target: "MyGame"
    client_target: "MyGameClient"
    server_target: "MyGameServer"
    default_configuration: "Development"

  OtherGame:
    engine: "5.7"
    project_file: "OtherGame.uproject"
    editor_target: "OtherGameEditor"
```

В дальнейшем registry можно генерировать автоматически из metadata проектов Canvas, но в версии 1 предпочтительна явная конфигурация.

---

# 11. Дизайн CLI `uectl`

`uectl` — небольшой клиент, установленный внутри Agent Canvas.

Он не содержит бинарных файлов Unreal Engine.

Обязанности:

1. определить текущий project/workspace;
2. обратиться к UE Build Service;
3. потоково передавать build logs;
4. вернуть exit code удалённой сборки как exit code собственного процесса;
5. по запросу предоставлять компактный machine-readable output.

## 11.1 Команды

```text
uectl status

uectl build
uectl build --target editor
uectl build --target game
uectl build --target server
uectl build --configuration Development

uectl plugin-build MyPlugin

uectl test
uectl test --filter <pattern>

uectl cook
uectl package
uectl clean

uectl logs <job-id>
uectl cancel <job-id>
```

## 11.2 Значения по умолчанию

Из UE worktree:

```bash
cd /worktrees/MyGame/codex-a17f
uectl build
```

команда автоматически разрешается в:

```text
project=MyGame
workspace=codex-a17f
target=editor
configuration=Development
```

Из основного дерева:

```bash
cd /projects/MyGame
uectl build
```

получаем:

```text
project=MyGame
workspace=main
```

## 11.3 Поведение exit code

`uectl` должен возвращать результат сборки как собственный process exit code.

Пример:

```text
0   сборка успешна
1   общая ошибка клиента/сервиса
2   некорректный запрос/проект
3   сервис недоступен
4   job отменён
6   ошибка сборки Unreal
```

Где это возможно, после зарезервированного диапазона client-кодов следует сохранять исходный exit code UE/AutomationTool.

Это важно, потому что LLM может выполнить:

```bash
uectl build
```

и анализировать результат через обычную shell-семантику.

---

# 12. Вывод для человека и машины

По умолчанию:

```text
$ uectl build
Project:       MyGame
Workspace:     codex-a17f
Engine:        UE 5.8
Target:        MyGameEditor
Configuration: Development
Job:           4b44de37

[1/18] Compile Foo.cpp
...

BUILD SUCCESS
Duration: 00:01:42
```

Machine-readable режим:

```bash
uectl build --json
```

Пример итогового объекта:

```json
{
  "job_id": "4b44de37",
  "state": "succeeded",
  "exit_code": 0,
  "project": "MyGame",
  "workspace": "codex-a17f",
  "target": "MyGameEditor",
  "engine": "5.8",
  "duration_ms": 102000
}
```

Поток логов может оставаться plain text даже при JSON-итоге; либо опция `--no-stream` может полностью отключать потоковый вывод.

---

# 13. Реализация сборки проекта

Для `target=editor` сервер выполняет заранее определённый эквивалент:

```bash
/opt/ue/5.8/Engine/Build/BatchFiles/Linux/Build.sh \
    MyGameEditor \
    Linux \
    Development \
    -Project=/worktrees/MyGame/codex-a17f/MyGame.uproject
```

Для основного workspace:

```text
-Project=/projects/MyGame/MyGame.uproject
```

Все пути и имена targets определяются только на стороне сервера.

---

# 14. Сборка Editor plugin

Поддерживаются два основных сценария плагинов.

## 14.1 Project plugin с Editor module

Пример:

```text
MyGame/
└── Plugins/
    └── MyTools/
        ├── MyTools.uplugin
        └── Source/
            ├── MyTools/
            └── MyToolsEditor/
```

Обычная сборка Editor target проекта:

```bash
uectl build --target editor
```

компилирует plugin как часть проекта.

Это основной рекомендуемый путь разработки.

## 14.2 Отдельная сборка пакета plugin

```bash
uectl plugin-build MyTools
```

на сервере преобразуется в эквивалент:

```bash
/opt/ue/5.8/Engine/Build/BatchFiles/RunUAT.sh \
    BuildPlugin \
    -Plugin=<validated-workspace>/Plugins/MyTools/MyTools.uplugin \
    -Package=/builds/MyGame/plugins/MyTools/<job-id> \
    -TargetPlatforms=Linux
```

Имя plugin должно валидироваться и разрешаться только внутри каталога `Plugins/` соответствующего проекта.

Произвольные пути к plugin не принимаются.

---

# 15. Engine plugins и изменение исходников Engine

## 15.1 Политика по умолчанию

Установленный Engine монтируется read-only.

Agent Canvas при необходимости получает read-only reference на исходники Engine:

```text
/reference/UE/5.8/Engine/Source
```

Это позволяет LLM читать Unreal API без возможности изменять Engine.

## 15.2 Пользовательские глобальные плагины

Глобальный пользовательский plugin предпочтительно хранить вне установки Engine:

```text
/mnt/user/UEPlugins/MyGlobalEditorPlugin
```

Его можно отдельным writable bind mount подключить к build/editor containers в выбранную точку `Engine/Plugins/...`, при этом остальная часть Engine остаётся read-only.

## 15.3 Изменение исходников Epic Engine

Изменение исходников Epic Engine или встроенных plugins явно не входит в стандартный deployment.

Будущий профиль `source-engine` может предоставлять:

```text
/opt/ue-src/5.8
```

с правом записи либо на базе отдельного Git worktree и использовать полноценную source build.

Этот режим должен быть изолирован от обычных проектов, использующих Installed Build.

---

# 16. Очередь сборки

Сервис должен иметь очередь и не запускать каждый поступивший запрос немедленно.

Рекомендуемые начальные значения для Ryzen 9 5950X:

```yaml
scheduler:
  max_global_jobs: 2
  max_jobs_per_workspace: 1
```

Правила:

- одновременно только один активный job может записывать в конкретный workspace;
- разные worktree могут собираться параллельно;
- `plugin-build` и `package` считаются build jobs;
- jobs в очереди имеют стабильные ID и собственные logs;
- отмена должна завершать всё дерево дочерних процессов.

В дальнейшем можно добавить resource profiles с отдельными лимитами CPU/RAM для разных типов job.

---

# 17. Блокировка workspace

Каждый workspace получает файловый lock, например:

```text
<workspace>/.uectl/build.lock
```

Build Service удерживает lock на протяжении всего времени выполнения job.

Это защищает от:

- одновременной сборки одного worktree двумя агентами;
- дублирования запросов из-за retry;
- пересечения `clean` и `build`.

Основным источником истины остаётся серверный scheduler; файловый lock — дополнительный уровень защиты.

---

# 18. Модель взаимодействия с Editor

Обычный интерактивный UE Editor открывает основное дерево проекта:

```text
/projects/MyGame
```

Автономные агенты обычно используют отдельные worktree:

```text
/worktrees/MyGame/<workspace-id>
```

Поэтому автономные сборки не конфликтуют с активным Editor.

Интерактивные агенты могут работать с основным деревом, но внешнюю сборку при активно работающем Editor, использующем те же `Binaries/` и `Intermediate/`, следует считать потенциально конфликтной.

Политика версии 1:

- build основного дерева разрешается только при явном запросе;
- для unattended-задач предпочтительны автономные worktree;
- Live Coding RPC не реализуется;
- Agent Canvas получает только узкий `ue-editor-control` API: `status/start/stop/restart`;
- control API не предоставляет shell, Docker API, arbitrary executable/path или произвольные Editor console commands.

---

# 19. Жизненный цикл job

```text
created
  ↓
validated
  ↓
queued
  ↓
running
  ├──→ succeeded
  ├──→ failed
  └──→ cancelled
```

Metadata job должна включать:

```yaml
id:
operation:
project:
workspace:
engine:
target:
configuration:
created_at:
started_at:
finished_at:
state:
exit_code:
log_path:
artifact_paths:
```

Logs должны храниться постоянно в течение настраиваемого retention period.

---

# 20. Работа с логами

Build output должен одновременно потоково передаваться в `uectl` и сохраняться на стороне сервера.

Рекомендуемое хранение:

```text
/var/lib/uebuild/jobs/<job-id>/
├── request.json
├── metadata.json
├── stdout.log
└── artifacts.json
```

Команда

```text
uectl logs <job-id>
```

использует byte-offset API:

```text
GET /v1/jobs/<job-id>/logs?offset=<byte-offset>
X-UECTL-Next-Offset: <next-byte-offset>
X-UECTL-Log-Size: <snapshot-eof-byte-offset>
```

Лог append-only в течение lifetime job. Активный/сохранённый job log нельзя truncate/rotate-ить по месту, иначе offset теряет смысл. Retention удаляет job/log целиком после окончания retention window. Подробный wire contract задан в `BUILD-SERVICE-API-v1.md`.

---

# 21. Аутентификация между Canvas и Build Service

В текущей доверенной deployment-модели аутентификация **не используется**. `ue-build-service:8765` и `ue-editor:8766` доступны только участникам внутренней Docker-сети `ue-control` и не публикуются в LAN/Internet.

`uectl` намеренно не отправляет bearer token. Вместо этого он отправляет protocol/version headers:

```text
User-Agent: uectl/<version>
X-UECTL-API-Version: 1
X-UECTL-Client-Version: <version>
```

Это не механизм авторизации. Изоляция обеспечивается Docker network и отсутствием `ports:` для control-plane endpoints. Если модель угроз изменится, mTLS/scoped credentials добавляются как отдельная версия deployment policy, не через project repositories.

---

# 22. Границы безопасности

## Agent Canvas может

```text
✓ читать/изменять явно подключённые project shares
✓ читать/изменять собственные worktree
✓ читать reference исходников UE
✓ использовать Internet согласно политике Canvas
✓ обращаться к ограниченному API ue-build-service
```

## Agent Canvas не может

```text
✗ обращаться к Docker socket
✗ создавать контейнеры
✗ получать shell хоста Unraid
✗ монтировать произвольные shares
✗ изменять установленный UE Engine
✗ передавать произвольные filesystem paths в Build Service
✗ выполнять произвольные команды через Build Service
```

## Build Service может

```text
✓ выполнять заранее разрешённые UE build commands
✓ записывать результаты сборки в project/worktree
✓ записывать build artifacts
✓ читать установленный UE Engine
```

## Build Service не должен

```text
✗ запускаться privileged
✗ монтировать Docker socket
✗ публиковать API в LAN
✗ иметь доступ к несвязанным Unraid shares
```

---

# 23. Контейнер UE Editor

Контейнер Editor строится на закреплённой версии Selkies EGL desktop image либо на внутреннем image, производном от неё.

Рекомендуемая конфигурация первого этапа:

```text
START_PLASMA=true
SELKIES_WAYLAND=false
```

Для упрощения диагностики сначала используется X11/DRI3.

После проверки можно перейти к минимальному single-application mode.

GPU-конфигурация:

```text
--runtime=nvidia
--shm-size=2g
NVIDIA_VISIBLE_DEVICES=<RTX3060 UUID>
NVIDIA_DRIVER_CAPABILITIES=all
```

HDMI dummy plug для Selkies не требуется, но может оставаться установленным. Позднее он позволит реализовать альтернативный путь с физическим Xorg + Sunshine/Moonlight.

---

# 24. Сетевой доступ к UE Editor

Рекомендуемые этапы:

## Первичная диагностика

```text
SELKIES_MODE=websockets
```

Публикуется только web port Selkies.

## Обычная работа в LAN

```text
SELKIES_MODE=webrtc
```

с настройками WebRTC/TURN, подходящими для Docker-сети Unraid.

Для удалённой работы через Internet предпочтителен WireGuard, а не прямое опубликование Editor в публичный Internet.

---

# 25. Пример каркаса Docker Compose

Пример ниже иллюстративный. Конкретные image tags, UID/GID, реальные пути Unraid и версия Selkies должны задаваться deployment-конфигурацией.

```yaml
services:
  agent-canvas:
    image: <agent-canvas-image>
    container_name: agent-canvas
    restart: unless-stopped
    environment:
      UECTL_ENDPOINT: http://ue-build-service:8765
      UECTL_EDITOR_ENDPOINT: http://ue-editor:8766
    volumes:
      - /mnt/user/UEProjects:/projects:rw
      - /mnt/user/UEWorktrees:/worktrees:rw
      - /mnt/user/UEBuilds:/builds:ro
      - /mnt/<fast-pool>/UE/5.8/Engine/Source:/reference/UE/5.8/Engine/Source:ro
      - /mnt/user/appdata/agent-canvas:/appdata:rw
    networks:
      - default
      - ue-control

  ue-build-service:
    image: <ue-build-service:5.8>
    container_name: ue-build-service
    restart: unless-stopped
    environment:
      UE_PROJECT_ROOT: /projects
      UE_WORKTREE_ROOT: /worktrees
      UE_BUILD_ROOT: /builds
      UE_ENGINE_ROOT_5_8: /opt/ue/5.8
      MAX_GLOBAL_JOBS: "2"
      MAX_JOBS_PER_WORKSPACE: "1"
    volumes:
      - /mnt/user/UEProjects:/projects:rw
      - /mnt/user/UEWorktrees:/worktrees:rw
      - /mnt/user/UEBuilds:/builds:rw
      - /mnt/<fast-pool>/UE/5.8:/opt/ue/5.8:ro
      - /mnt/user/appdata/ue-build-service:/var/lib/uebuild:rw
    expose:
      - "8765"
    networks:
      - ue-control

  ue-editor:
    image: <ue-editor:5.8-selkies>
    container_name: ue-editor
    restart: unless-stopped
    runtime: nvidia
    shm_size: 2gb
    environment:
      NVIDIA_VISIBLE_DEVICES: ${UE_GPU_UUID}
      NVIDIA_DRIVER_CAPABILITIES: all
      START_PLASMA: "true"
      SELKIES_WAYLAND: "false"
      SELKIES_MODE: websockets
      UE_EDITOR_CONTROL_HOST: 0.0.0.0
      UE_EDITOR_CONTROL_PORT: "8766"
    volumes:
      - /mnt/user/UEProjects:/projects:rw
      - /mnt/<fast-pool>/UE/5.8:/opt/ue/5.8:ro
      - /mnt/<fast-pool>/UE-DDC:/ue-ddc:rw
      - /mnt/user/appdata/ue-editor/home:/home/ubuntu:rw
    ports:
      - "8080:8080"
    expose:
      - "8766"
    networks:
      - default
      - ue-control

networks:
  ue-control:
    internal: true
```

В реальном deployment на Unraid эти services могут быть представлены нативными Docker templates вместо Compose; контракт mount/network должен остаться тем же.

---

# 26. Image для Build Service

Build image должен содержать только build/runtime prerequisites и реализацию `ue-build-service`.

Концептуально:

```dockerfile
FROM ubuntu:22.04

# Зависимости сборки UE
# clang/lld, соответствующие выбранной версии UE
# git/git-lfs при необходимости
# runtime libraries для UBT/UHT/AutomationTool

COPY ue-build-service /usr/local/lib/ue-build-service
COPY project-registry.yaml /etc/uebuild/projects.yaml

USER ue
ENTRYPOINT ["/usr/local/lib/ue-build-service/server"]
```

Сам Engine монтируется во время запуска.

Не следует помещать установку Engine размером 50–100+ GB внутрь image через `COPY`.

---

# 27. Установка `uectl` в Agent Canvas

`uectl` должен быть небольшим статически распространяемым бинарным файлом либо минимальным script/package.

Желательные свойства:

- отсутствие зависимостей Unreal;
- отсутствие зависимости от Docker CLI/API;
- отсутствие необходимости в root;
- возможность просто скопировать в `/usr/local/bin/uectl`;
- удобные shell exit codes;
- поддержка JSON output;
- эффективный streaming logs;
- конфигурация через environment variables.

Основные environment variables:

```text
UECTL_ENDPOINT=http://ue-build-service:8765
UECTL_EDITOR_ENDPOINT=http://ue-editor:8766
UECTL_PROJECT_ROOT=/projects
UECTL_WORKTREE_ROOT=/worktrees
```

Аутентификационного token в текущей isolated-network модели нет. Timeout/retry параметры могут задаваться отдельными `UECTL_*` variables, как описано в README пакета.

---

# 28. Metadata проекта для агентов

Каждый UE-проект должен содержать краткие инструкции, пригодные для разных LLM-агентов.

Пример фрагмента `AGENTS.md`:

```text
Этот repository является проектом Unreal Engine 5.8.

Сборка Editor target:
    uectl build

Сборка/упаковка project plugin:
    uectl plugin-build <PluginName>

Запуск тестов:
    uectl test

Упаковка проекта:
    uectl package

Reference исходников Unreal Engine:
    /reference/UE/5.8/Engine/Source

Не использовать Docker напрямую.
Не запускать UnrealBuildTool или Build.sh напрямую.
Не изменять /reference/UE.
```

При необходимости тот же command contract можно продублировать в Claude/Gemini-specific instruction files.

---

# 29. Типичный workflow автономного агента

```text
Agent Canvas
   │
   ├─ создаёт/выбирает Git worktree
   │
   ├─ LLM изменяет Source/ или Plugins/
   │
   ├─ uectl build
   │      │
   │      └─ UE Build Service
   │             ├─ UHT
   │             ├─ UBT
   │             └─ clang/lld
   │
   ├─ получает ошибку компилятора
   │
   ├─ LLM исправляет код
   │
   ├─ uectl build
   │
   ├─ uectl test
   │
   └─ commit / показывает diff для review
```

На протяжении этого процесса Editor может продолжать работать с основным worktree.

---

# 30. Типичный workflow Editor plugin

Агент изменяет:

```text
Plugins/MyTools/Source/MyToolsEditor/...
```

затем:

```text
uectl build
```

выполняется:

```text
MyGameEditor target
```

и в его составе компилируется:

```text
MyToolsEditor module
```

Для проверки готовности к распространению:

```text
uectl plugin-build MyTools

        ↓

RunUAT BuildPlugin

        ↓

/builds/MyGame/plugins/MyTools/<job-id>/
```

Таким образом Editor plugins поддерживаются без необходимости физически хранить plugin в дереве Engine.

---

# 31. Обработка ошибок

Build Service должен различать как минимум:

- ошибку валидации запроса;
- отсутствующий project/worktree;
- неизвестную версию Engine;
- отказ scheduler/queue;
- отменённый job;
- ошибку UHT;
- ошибку UBT/compiler/linker;
- ошибку AutomationTool;
- инфраструктурную ошибку сервиса.

`uectl` должен показывать полезный хвост ошибки, сохраняя полный log на сервере.

Пример:

```text
BUILD FAILED
Job: 91fa6d
Exit code: 6

Source/MyGame/Foo.cpp:127:18: error: no member named 'Bar' in 'AFoo'

Full log:
    uectl logs 91fa6d
```

---

# 32. Идемпотентность и retry

Каждый create-job содержит:

```text
X-UECTL-Request-ID: <idempotency-key>
```

Build Service persistently сохраняет `request_id`, operation, hash canonical payload, `job_id` и время создания. Повтор того же request-id с тем же operation/payload возвращает тот же `job_id` даже после restart сервиса. Тот же request-id с другим payload/operation возвращает `409 Conflict` и не изменяет старый job. Mapping хранится не меньше retention соответствующего job.

`uectl` автоматически retry-ит transport failures и HTTP `408`, `425`, `429`, `500`, `502`, `503`, `504` с тем же request-id. `501`, `507` и прочие non-transient ответы автоматически не повторяются.

POST `/v1/jobs/<job-id>/cancel` обязан быть идемпотентным, поскольку клиент также может повторять его после transient transport failure.

Полный normative contract находится в `BUILD-SERVICE-API-v1.md`.

---

# 33. Наблюдаемость

Минимальный operational endpoint:

```text
GET /v1/status
```

должен возвращать, например:

```json
{
  "service": "ue-build-service",
  "ready": true,
  "active_jobs": 1,
  "queued_jobs": 0,
  "engines": ["5.8"]
}
```

Расширенный admin-only endpoint может дополнительно показывать:

- PID активных jobs;
- использование CPU/RAM;
- глубину очереди;
- доступные установки Engine;
- результаты проверки project registry;
- свободное место в build/worktree storage.

LLM-агентам этот admin endpoint не нужен.

---

# 34. Health checks контейнеров

## UE Build Service

Health check должен проверять:

```text
HTTP service отвечает
configured Engine root существует
Build.sh существует
RunUAT.sh существует
project registry успешно разбирается
build root доступен для записи
```

## UE Editor

Health check может проверять:

```text
Selkies web service отвечает
nvidia-smi выполняется успешно
UnrealEditor binary существует
```

Не следует требовать, чтобы процесс Unreal Editor был постоянно запущен, если контейнер предоставляет desktop, из которого Editor запускается вручную.

---

# 35. Версионирование

`uectl` и server API должны иметь независимое semantic versioning.

Пример:

```text
uectl 1.2.0
UE Build Service 1.4.1
API /v1
```

`uectl version` при доступном сервере показывает версии клиента и сервера.

Все control-plane запросы несут `X-UECTL-API-Version: 1` и `X-UECTL-Client-Version`. Сервис должен явно отклонять несовместимую API version (рекомендуется HTTP `426`), а не молча неверно интерпретировать запрос.

---

# 36. Поддержка нескольких версий Engine

Архитектура не должна предполагать, что все проекты используют одну версию UE.

Есть два разумных варианта реализации.

## A. Отдельный service image для каждой major/minor версии UE

```text
ue-build-service-5.7
ue-build-service-5.8
```

Преимущества:

- наиболее простая изоляция зависимостей;
- точный compiler/toolchain для конкретной версии Engine;
- меньший риск конфликтов между версиями.

## B. Один сервис с несколькими смонтированными Engines

```text
/opt/ue/5.7
/opt/ue/5.8
```

Преимущества:

- одна очередь/API;
- более простая конфигурация Canvas.

Для первой реализации достаточно одного UE 5.8 worker. Внешний контракт Canvas при этом уже предполагает **один logical `UECTL_ENDPOINT`**. Если разные UE версии потребуют отдельных toolchains/images, перед version-specific workers добавляется тонкий router; project registry выбирает engine, а `uectl` не выбирает Engine path напрямую.

---

# 37. Рекомендуемые этапы реализации

## Этап 1 — минимальный безопасный build loop

Реализовать:

```text
uectl status
uectl build
uectl logs
```

Поддержать одну версию UE и один проект.

Проверить:

- одна и та же share видна Canvas и builder;
- Editor target проекта успешно собирается;
- ошибки возвращаются LLM;
- Docker socket отсутствует в Canvas.

## Этап 2 — plugins и worktrees

Добавить:

```text
plugin-build
workspace IDs
path validation
build queue
cancel
```

## Этап 3 — тесты и packaging

Добавить:

```text
test
cook
package
artifact tracking
```

## Этап 4 — эксплуатационное усиление

Добавить:

```text
request idempotency
job retention/rotation
metrics
admin status
resource limits
health checks
```

## Этап 5 — опциональная интеграция с Editor

Возможные дальнейшие расширения:

```text
Live Coding request channel
Editor status
reload notification
PIE/test automation
```

Эти возможности должны оставаться отдельно от базового build API и не должны предоставлять Agent Canvas неограниченное управление Editor.

---

# 38. Ключевые архитектурные решения

1. **Agent Canvas остаётся универсальным.** UE — внешняя capability, а не часть image Canvas.
2. **В Agent Canvas нет Docker socket.** Запросы сборки идут через узкий RPC API.
3. **UE Engine хранится как persistent host storage.** Он монтируется в build/editor containers и не встраивается в их images.
4. **Engine по умолчанию read-only.** Разработка проекта и project plugins не требует права записи в Engine.
5. **Editor plugins — полноценный поддерживаемый сценарий.** Project plugins с модулями `Editor` собираются через Editor target проекта; отдельная проверка/упаковка выполняется через `BuildPlugin`.
6. **Параллельные автономные агенты используют Git worktrees.** Каждый worktree имеет независимые `Intermediate/` и `Binaries/`.
7. **Canvas и Build Service видят project/worktree по одинаковым контейнерным путям.** Это снижает проблемы Unreal build metadata, зависящей от путей.
8. **Build Service принимает логические project/workspace IDs, а не произвольные host paths.**
9. **Build operations заранее определены.** Универсального remote shell endpoint нет.
10. **UE Editor остаётся отдельным GPU-enabled контейнером Selkies и управляется только узким `ue-editor-control` API.**
11. **Build artifacts монтируются в Agent Canvas как `/builds:ro`.**
12. **Wire contract Build Service API v1 зафиксирован отдельно в `BUILD-SERVICE-API-v1.md`.**

---

# 38.1 Нормативный Build Service API v1

Начиная с `uectl 0.2.2`, wire/API поведение не следует выводить из примеров этого документа. Нормативным источником является `BUILD-SERVICE-API-v1.md`. Он фиксирует:

- headers/version negotiation;
- error schema и retryable HTTP statuses;
- create payloads для `build`, `plugin-build`, `test`, `cook`, `package`, `clean`;
- persistent idempotency и conflict semantics;
- job identity/lifecycle и terminal-state barrier;
- byte-offset log protocol;
- идемпотентный cancel;
- safe `clean`;
- `client_target`;
- `/builds:ro` для Agent Canvas.

Для Automation Tests используется штатный UE command-line mechanism `Automation RunTest ...;Quit`; для cook/package — AutomationTool `BuildCookRun`. Пользовательские строки не превращаются в shell command и не дают arbitrary UAT/UBT flags.

---

# 39. Итоговая целевая топология

```text
                         ┌────────────────────┐
                         │   Agent Canvas     │
                         │                    │
                         │ Codex              │
                         │ Claude             │
                         │ Gemini             │
                         │                    │
                         │ uectl              │
                         └─────────┬──────────┘
                                   │
                            restricted RPC
                                   │
                         ┌─────────▼──────────┐
                         │ UE Build Service   │
                         │                    │
                         │ UBT / UHT          │
                         │ clang / lld        │
                         │ RunUAT             │
                         └──────┬─────────────┘
                                │
                       shared filesystem
                                │
               ┌────────────────┴────────────────┐
               │                                 │
        UE project/worktrees               build artifacts
               │
               │
       ┌───────▼────────┐
       │ UE Editor      │
       │                │
       │ Selkies        │
       │ Vulkan         │
       │ RTX 3060       │
       └────────────────┘
```

Такая схема сохраняет универсальный Agent Canvas независимым от Unreal Engine, но при этом предоставляет каждому подключённому LLM безопасный, детерминированный и пригодный для автоматизации workflow сборки, тестирования и разработки Unreal-плагинов.
