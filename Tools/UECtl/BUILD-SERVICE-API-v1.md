# UE Build Service API v1

**Статус:** normative / frozen for implementation  
**Клиент:** `uectl >= 0.2.2`  
**API version:** `1`  
**Transport:** HTTP внутри изолированной Docker-сети `ue-control`  
**Authentication:** отсутствует намеренно; `8765/tcp` нельзя публиковать в LAN/Internet.

Этот документ является нормативным контрактом между `uectl` и `ue-build-service`. Если общий design-doc противоречит этому файлу, для wire/API поведения действует этот файл.

## 1. Общие HTTP-заголовки

`uectl` отправляет во всех запросах:

```text
User-Agent: uectl/<client-version>
X-UECTL-API-Version: 1
X-UECTL-Client-Version: <client-version>
```

Для create-job дополнительно:

```text
X-UECTL-Request-ID: <idempotency-key>
```

Сервер обязан отклонить несовместимую major API version до выполнения операции. Рекомендуемый ответ:

```http
426 Upgrade Required
Content-Type: application/json
```

```json
{"error":"unsupported API version","code":"api_version_mismatch"}
```

## 2. Error schema

Ошибки сервера возвращаются JSON object:

```json
{
  "error": "human-readable message",
  "code": "stable_machine_code"
}
```

`error` обязателен. `code` рекомендуется и должен быть стабильным для автоматизации.

Типовые HTTP status:

```text
400 malformed/invalid request
404 project/workspace/job/plugin not found
409 conflict, включая request-id reuse с другим payload
423 workspace locked, если это нельзя представить queued job
426 incompatible API version
429 service-side admission/rate limit
500 unexpected service failure
502/503/504 transient infrastructure failure
507 insufficient storage
```

Клиент автоматически retry-ит только:

```text
408 425 429 500 502 503 504
```

`501`, `507` и прочие `5xx`, не входящие в список выше, автоматически не retry-ятся.

## 3. Status

```http
GET /v1/status
```

Минимальный ответ:

```json
{
  "service": "ue-build-service",
  "server_version": "0.1.0",
  "api_version": "1",
  "ready": true,
  "active_jobs": 0,
  "queued_jobs": 0,
  "engines": ["5.8"]
}
```

`ready=true` означает, что registry загружен, build root доступен для записи, требуемый Engine/toolchain доступен и scheduler принимает jobs.

## 4. Logical project/workspace identity

Клиент никогда не передаёт host/container filesystem path проекта. Он передаёт только:

```json
{"project":"MyGame","workspace":"main"}
```

или:

```json
{"project":"MyGame","workspace":"codex-a17f"}
```

Сервер разрешает:

```text
workspace=main        -> /projects/<project>
workspace=<other>     -> /worktrees/<project>/<workspace>
```

Имена валидируются до filesystem access. После `resolve()` итоговый путь обязан оставаться внутри соответствующего root. Symlink escape блокируется.

## 5. Project registry

Минимальный registry:

```yaml
projects:
  MyGame:
    engine: "5.8"
    project_file: "MyGame.uproject"
    editor_target: "MyGameEditor"
    game_target: "MyGame"
    client_target: "MyGameClient"   # optional
    server_target: "MyGameServer"   # optional
    default_configuration: "Development"
```

Engine version, `.uproject` и реальные target names определяются только registry/server-side. Клиент не может передать target binary name или Engine path.

## 6. Create-job endpoints

```text
POST /v1/jobs/build
POST /v1/jobs/plugin-build
POST /v1/jobs/test
POST /v1/jobs/cook
POST /v1/jobs/package
POST /v1/jobs/clean
```

Успешное создание/повторное получение job:

```http
202 Accepted
```

```json
{"job_id":"01K..."}
```

Поле называется **только `job_id`**. Legacy alias `id` в API v1 не используется.

`job_id` — opaque logical identifier, безопасный для одного URL segment. Сервер не должен генерировать `.` или `..`.

### 6.1 build

Request:

```json
{
  "project":"MyGame",
  "workspace":"codex-a17f",
  "target":"editor",
  "configuration":"Development"
}
```

`target`:

```text
editor | game | client | server
```

Сервер преобразует logical target в registry target. Если optional `client_target`/`server_target` отсутствует, возвращается validation error.

Эквивалент для Editor target:

```bash
/opt/ue/<version>/Engine/Build/BatchFiles/Linux/Build.sh \
  <registry.editor_target> \
  Linux \
  <configuration> \
  -Project=<resolved-workspace>/<registry.project_file>
```

Команда запускается argv-массивом, не через shell.

### 6.2 plugin-build

Request:

```json
{
  "project":"MyGame",
  "workspace":"codex-a17f",
  "plugin":"MyTools",
  "platform":"Linux"
}
```

`plugin` — logical directory/name внутри `<workspace>/Plugins/`. Произвольный path запрещён.

Эквивалент:

```bash
/opt/ue/<version>/Engine/Build/BatchFiles/RunUAT.sh \
  BuildPlugin \
  -Plugin=<resolved-workspace>/Plugins/MyTools/MyTools.uplugin \
  -Package=/builds/MyGame/plugins/MyTools/<job-id> \
  -TargetPlatforms=Linux
```

Artifact root возвращается через `artifact_paths`.

### 6.3 test

Request:

```json
{
  "project":"MyGame",
  "workspace":"codex-a17f",
  "filter":"MyGame"
}
```

Automation tests запускаются через command-line Editor/Client automation mechanism UE. Для headless Linux worker рекомендуемый шаблон:

```text
UnrealEditor-Cmd <uproject>
  -unattended
  -nop4
  -NullRHI
  -ExecCmds="Automation RunTest <validated-filter>;Quit"
  -ReportExportPath=<job-artifact-dir>/automation-report
```

Сервер **не** вставляет сырую пользовательскую строку в shell. `subprocess`/exec получает argv-массив. Дополнительно `filter` валидируется как Automation selector: control characters, `;`, кавычки и другие значения, позволяющие добавить вторую console command, запрещены.

Если `filter` отсутствует, конкретный default определяется registry/service config; сервер не должен случайно запускать весь Engine test corpus без явной policy.

Официальная документация UE 5.8 описывает CLI запуск через `-ExecCmds="Automation RunTest ...;Quit"` и `-ReportExportPath`.

### 6.4 cook

Request:

```json
{
  "project":"MyGame",
  "workspace":"codex-a17f",
  "platform":"Linux"
}
```

Worker использует `RunUAT.sh BuildCookRun` в режиме cook без package. Версия worker-а формирует окончательный argv для соответствующей версии UE; нормативный результат операции — успешно cooked project content для указанной platform без создания distributable package.

Пользователь не может передавать произвольные UAT flags.

### 6.5 package

Request:

```json
{
  "project":"MyGame",
  "workspace":"codex-a17f",
  "platform":"Linux",
  "configuration":"Development"
}
```

Worker использует `RunUAT.sh BuildCookRun` для build/cook/stage/package/archive. Archive destination всегда server-generated:

```text
/builds/<project>/packages/<job-id>/
```

Пользователь не передаёт archive path или произвольные UAT flags.

UE 5.8 documentation описывает `BuildCookRun` как AutomationTool command для build/cook/package/deploy/run pipeline.

### 6.6 clean

Request:

```json
{"project":"MyGame","workspace":"codex-a17f"}
```

`clean` **не принимает path** и не является удалённым `rm`.

V1 разрешает удалять только generated build data выбранного workspace:

```text
Binaries/
Intermediate/
Saved/Cooked/
Saved/StagedBuilds/
Plugins/*/Binaries/
Plugins/*/Intermediate/
```

V1 не удаляет:

```text
Content/
Config/
Source/
Plugins/*.uplugin и исходники plugins
Saved/Autosaves/
Saved/Config/
Saved/Logs/
.git/
```

Перед удалением каждый resolved path повторно проверяется на принадлежность workspace root. Symlink traversal запрещён.

## 7. Idempotency

Каждый create-job обязан иметь:

```text
X-UECTL-Request-ID
```

Сервис persistently сохраняет минимум:

```text
request_id
operation
canonical_payload_hash
job_id
created_at
```

Правила:

1. Новый request-id + валидный payload -> создаётся один job.
2. Повтор того же request-id + та же operation + тот же canonical payload -> возвращается **тот же `job_id`**, независимо от состояния job.
3. Тот же request-id с другой operation или другим payload -> `409 Conflict`; старый job не меняется.
4. Mapping должен переживать restart сервиса.
5. Mapping хранится не меньше retention соответствующего job.

Именно это делает безопасным client-side retry после lost HTTP response.

## 8. Job lifecycle и metadata

Допустимые состояния:

```text
created -> validated -> queued -> running -> succeeded
                                      |----> failed
                                      `----> cancelled
```

```http
GET /v1/jobs/<job-id>
```

Минимальная metadata:

```json
{
  "job_id":"01K...",
  "operation":"build",
  "project":"MyGame",
  "workspace":"codex-a17f",
  "engine":"5.8",
  "state":"running",
  "created_at":"...",
  "started_at":"...",
  "finished_at":null,
  "exit_code":null,
  "artifact_paths":[]
}
```

Для terminal job `finished_at` и `exit_code` фиксированы. `artifact_paths` содержит container-visible пути, обычно под `/builds`.

Клиент проверяет identity fields (`job_id`, `project`, `workspace`, `operation`) и считает их несовпадение protocol corruption.

### Terminal-state barrier

Сервер не может публиковать `succeeded|failed|cancelled`, пока не выполнены все пункты:

1. основной процесс завершён;
2. всё дочернее process tree завершено/убито согласно состоянию;
3. stdout/stderr полностью drain-нуты;
4. log flush завершён;
5. artifacts завершены и metadata по ним зафиксирована;
6. `exit_code` и timestamps сохранены;
7. только после этого metadata atomically переводится в terminal state.

После terminal state log и artifact metadata immutable до удаления job retention policy.

## 9. Logs — byte-offset protocol

```http
GET /v1/jobs/<job-id>/logs?offset=<non-negative-byte-offset>
```

Ответ:

```http
200 OK
Content-Type: text/plain; charset=utf-8
X-UECTL-Next-Offset: <absolute-byte-offset>
X-UECTL-Log-Size: <snapshot-eof-byte-offset>
```

Body содержит bytes начиная с requested offset. `X-UECTL-Next-Offset` обязан равняться:

```text
requested_offset + Content-Length(body)
```

`X-UECTL-Log-Size` — размер лог-файла (EOF byte offset), зафиксированный сервером в начале обработки конкретного запроса. Он не может быть меньше `X-UECTL-Next-Offset`. Это позволяет `uectl logs` без `-f` дочитать именно snapshot и завершиться, даже если активный job продолжает писать лог.

Если offset равен текущему EOF:

```text
body = empty
X-UECTL-Next-Offset = offset
X-UECTL-Log-Size = offset
```

Если offset больше текущего EOF:

```http
416 Range Not Satisfiable
```

Log append-only. Он не truncate/rotate-ится, пока job доступен через API, иначе byte offsets теряют смысл. Retention удаляет job/log целиком после окончания retention window.

Сервис может отдавать лог chunks ограниченного размера. `uectl 0.2.2` продолжает читать chunks и после terminal metadata до пустого chunk/EOF. Обычный `uectl logs` запоминает первый `X-UECTL-Log-Size` и не начинает follow растущего active log; `uectl logs -f` продолжает опрос после snapshot.

## 10. Cancel

```http
POST /v1/jobs/<job-id>/cancel
Content-Type: application/json

{}
```

Cancel **идемпотентен**:

- queued -> исключить из queue -> cancelled;
- running -> завершить всё process tree -> cancelled;
- already cancelled -> вернуть cancelled;
- succeeded/failed -> вернуть текущее terminal state, не менять результат.

Response:

```json
{"job_id":"01K...","state":"cancelled"}
```

Идемпотентность обязательна, потому что `uectl` может retry POST `/cancel` при transient transport errors.

## 11. Scheduler / workspace exclusivity

Начальная policy:

```yaml
max_global_jobs: 2
max_jobs_per_workspace: 1
```

Один writable workspace не исполняет одновременно две операции, затрагивающие generated state. Scheduler является source of truth; filesystem lock внутри workspace — дополнительная защита.

Cancel должен завершать всё process tree job, а не только parent PID.

## 12. Artifacts и mounts

Build service:

```text
/projects   rw
/worktrees  rw
/builds     rw
/opt/ue/... ro
```

Agent Canvas:

```text
/projects   rw
/worktrees  rw
/builds     ro
/reference/UE/... ro
```

Таким образом `artifact_paths`, возвращаемые server metadata, доступны агенту для чтения без права изменять build artifacts.

## 13. Несколько версий Engine

API v1 engine-aware, но клиент имеет один logical endpoint.

Начальная реализация может обслуживать только UE 5.8. Если версии требуют разных toolchains, стабильная внешняя схема остаётся:

```text
uectl -> one logical endpoint/router -> version-specific worker 5.7/5.8/...
```

Project registry выбирает engine; клиент не выбирает Engine path.

## 14. Не существует в API v1

Запрещены endpoints и параметры, позволяющие произвольное выполнение:

```text
/exec
/shell
/command
arbitrary filesystem paths
arbitrary executable names
arbitrary UBT/UAT flags
arbitrary environment injection
```

## 15. Reference notes

Для Automation Test CLI и BuildCookRun semantics использована официальная документация Epic Games для Unreal Engine 5.8:

- Run Automation Tests in Unreal Engine
- Build Operations: Cooking, Packaging, Deploying, and Running Projects in Unreal Engine
- Unreal Automation Tool Overview

Официальные ссылки:

- https://dev.epicgames.com/documentation/unreal-engine/run-automation-tests-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/unreal-automation-tool-overview-for-unreal-engine
