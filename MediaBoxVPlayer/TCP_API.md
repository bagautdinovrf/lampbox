# MediaBoxVPlayer TCP API v1

Транспорт и авторизация совпадают с MediaBoxPlayer: UTF-8 JSON-объект на строку
с завершающим `\n`, не более 1 MiB в запросе, максимум 16 одновременных клиентов.
Соединение без запросов закрывается через 30 секунд. Каждая команда содержит
`protocolVersion: 1` и `token`. При самостоятельном запуске токен берётся из
файла `control.token`; при локальном запуске из Manager с `--managed` он
передаётся процессу через `MEDIABOXVPLAYER_CONTROL_TOKEN`. Необязательное поле
`id` (строка, число или null) копируется в ответ. Адрес по умолчанию:
`127.0.0.1:17656`. Публикации событий нет; менеджер запрашивает `status`.

Manager автоматически запускает локальный процесс при недоступном
TCP-подключении. Это действие ОС; отдельной команды запуска в протоколе нет.
Ошибки аутентификации и версии протокола не вызывают запуск нового плеера.
Закрытие Manager и разрыв соединения не завершают видеоплеер. Подробности
режима, токенов и восстановления окон — в [README](README.md#автоматический-запуск-из-manager).

```json
{"protocolVersion":1,"token":"<64 hex characters>","id":1,"command":"status"}
```

Успешный ответ всегда содержит **всё** состояние видеоплеера:

```json
{
  "protocolVersion": 1,
  "id": 1,
  "ok": true,
  "status": {
    "application": "MediaBoxVPlayer",
    "persistenceError": "",
    "displays": [{"id":"screen-<hardware hash>","name":"HDMI-1","index":0}],
    "windows": [{
      "id":"hall","name":"Зал","screen":"","actualScreen":"screen-<hardware hash>",
      "fullscreen":true,"restoreError":"",
      "playback": {
        "state":"stopped","playbackRequested":false,
        "queue":[],"currentIndex":-1,"currentTrack":"",
        "positionMs":0,"durationMs":0,"volumePercent":100,"muted":false,
        "repeat":"off","order":"sequential","error":"",
        "playbackMode":"manual","channelName":"",
        "scheduleAvailable":false,"scheduleError":"",
        "scheduleId":"","publicationId":"","revision":0,"playbackId":"",
        "supportedCapabilities":["schedule.current.v1","calendar.v1","rotation.strict.v1","events.fixed.v1","media.video.v1"]
      }
    }]
  }
}
```

`displays.id` — идентификатор для `screen`; `index` — только текущий порядок
перечисления, не постоянный ID. `actualScreen` — текущий физический монитор
видимого окна, либо пустая строка у скрытого окна. `fullscreen` отражает
запрошенный режим, в том числе пока окно скрыто. `playback.state` подтверждается
декодером (`stopped`, `loading`, `playing`, `paused`, `error`), а
`playbackRequested` отражает намерение продолжать воспроизведение.
`playbackMode` различает `manual` и `schedule`; `channelName` содержит название
текущего канала. `scheduleAvailable` сообщает о загруженном снимке расписания,
а `scheduleError` — о проблеме его исполнения. Эти поля относятся к одному окну.

Команды окна:

| `command` | Обязательные дополнительные поля | Действие |
| --- | --- | --- |
| `status` | — | Состояние сервиса, мониторов и всех окон |
| `configureWindow` | `windowId`, `name`, `screen`, `fullscreen` | Создать окно или изменить его параметры, сохраняя текущую очередь и воспроизведение |
| `removeWindow` | `windowId` | Остановить, удалить окно и сохранённую очередь |
| `fullscreen` | `windowId`, `value` (boolean) | Переключить режим конкретного окна |

`windowId` соответствует `[A-Za-z0-9_-]{1,64}`. `name` — непустая строка до
128 символов (пробелы по краям удаляются). `screen` — строка до 256 символов,
пустая строка выбирает основной монитор; другое значение должно присутствовать
в текущем `displays`. `fullscreen` обязателен и имеет boolean-тип. Все четыре
поля настройки обязательны и при обновлении существующего окна. Лимит —
16 окон. Для независимого воспроизведения два окна могут использовать один монитор.

```json
{"protocolVersion":1,"token":"<token>","command":"configureWindow","windowId":"hall","name":"Зал","screen":"","fullscreen":true}
{"protocolVersion":1,"token":"<token>","command":"load","windowId":"hall","paths":["/srv/media/intro.mp4","/srv/media/main.mp4"],"autoplay":true}
{"protocolVersion":1,"token":"<token>","command":"volume","windowId":"hall","value":70}
```

Все команды воспроизведения адресуют **одно** окно через обязательный `windowId`.
Поля и поведение совпадают с [MediaBoxPlayer API](../MediaBoxPlayer/TCP_API.md):

| `command` | Дополнительные поля | Действие |
| --- | --- | --- |
| `load` | `paths`; необязательные `startIndex` (по умолчанию 0), `autoplay` (false) | Атомарная замена очереди |
| `enqueue` | `paths` | Добавить файлы в конец очереди |
| `loadPublication` | `activePath`, `contentRoot`, необязательный `autoplay` (false) | Прочитать текущий выпуск расписания из файлов для этого окна |
| `schedule` | — | Включить последний принятый выпуск окна |
| `playChannel` | `name`, `paths`, `volume` | Немедленно играть весь канал в ручном режиме с повтором `all` и громкостью 0..100 |
| `play`, `pause`, `stop` | — | Управление воспроизведением; stop возвращает позицию в начало |
| `next`, `previous` | — | Перейти к соседнему элементу, сохраняя намерение воспроизведения |
| `seek` | `positionMs` | Перемотка в миллисекундах, целое число >= 0 в пределах известной длительности |
| `volume` | `value` | Целая громкость 0..100 |
| `mute` | `value` | Boolean |
| `repeat` | `mode` | `off`, `all` или `one` |
| `clear` | — | Остановить и очистить очередь |

Пути должны быть абсолютными и локальными на машине видеоплеера, максимум
4096 символов каждый. Отсутствующий или нечитаемый файл отклоняет весь `load`
или `enqueue`, без частичной замены. Очередь содержит до 1000 файлов на окно.
Суммарный размер всех массивов путей в compact UTF-8 JSON ограничен 8 MiB,
чтобы полный ответ оставался в пределах 32 MiB клиента менеджера.
Расписание не входит в TCP-запрос: передаются только пути. В режиме расписания очередь содержит текущий выбранный трек.
Неизвестные поля отклоняются; URL и автоматическая передача файлов не поддерживаются.

Оба плеера используют единственный актуальный формат `mediabox.schedule`, `schemaVersion: 1`. Для видео разрешены assets с `mediaType: "video"` и `"audio"`; видео требует capability `media.video.v1`. В `playback.supportedCapabilities` каждого окна указаны `schedule.current.v1`, календарные возможности и `media.video.v1`. Manager проверяет поддержку перед обновлением.

```json
{"protocolVersion":1,"id":"video-release","token":"<CONTROL_TOKEN>","command":"loadPublication","windowId":"hall","activePath":"C:/ProgramData/MediaBox/video-schedule/active.json","contentRoot":"C:/ProgramData/MediaBox/media","autoplay":true}
```

Указатель ограничен 64 KiB, снимок — 64 MiB. Плеер сам читает `active.json` и snapshot, проверяет контрольную сумму и принимает выпуск. При ошибке прежний выпуск сохраняется. Пути должны быть доступны машине и учётной записи видеоплеера; для удалённого подключения нужен общий каталог или подготовленная файловая копия.

`windows.json` версии 3 хранит конфигурацию окон, ручные очереди и ссылки на публикации. У каждого окна отдельный постоянный runtime в каталоге `runtimes`; курсоры и события разных окон независимы. Неподдерживаемая версия файла вызывает ошибку без изменения исходных данных.

После перезапуска сохранённый режим расписания продолжает программу по текущему времени. Ручные очереди восстанавливаются остановленными; `pause`, `stop`, `clear`, `load`, `enqueue` и `playChannel` сохраняют ручной режим. Для возврата к расписанию отправьте `schedule`. Текущая позиция через перезапуск не сохраняется. Полный сохраняемый документ проверяется по тому же ограничению 64 MiB, которое применяется при чтении.

Ошибки команд возвращаются с актуальным полным `status`:

```json
{"protocolVersion":1,"id":2,"ok":false,"error":{"code":"unknown_window","message":"The video window does not exist."},"status":{"application":"MediaBoxVPlayer","displays":[],"windows":[],"persistenceError":""}}
```

Основные коды: `invalid_request`, `invalid_arguments`, `unknown_command`,
`unknown_window`, `unknown_screen`, `window_limit`, `queue_limit`, `invalid_path`,
`empty_queue`, `invalid_publication`, `runtime_error`, `schedule_unavailable`, `persistence_error`. Последний означает, что команда применена в
памяти, но атомарная запись `windows.json` не удалась. Ошибки авторизации и
транспорта (`unauthorized`, `unsupported_protocol`, `invalid_json`,
`request_too_large`) не содержат состояние окон.

При отключении явно выбранного монитора связанные окна останавливаются и
скрываются с пустым `actualScreen`; `play`, `schedule`, `playChannel`, а также `load` и `loadPublication` с `autoplay:true` возвращают
`unknown_screen`, как и `schedule` и `playChannel`. Другие окна продолжают работу.
После подключения окна показываются; активное расписание пересчитывается по
текущему времени, ручной режим остаётся остановленным. Сохранённый `screen`
остаётся прежним. Поведение
восстановления, исчезнувших файлов и локальных клавиш описано в [README](README.md).
