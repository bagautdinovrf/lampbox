# MediaBoxVPlayer TCP API v1

Транспорт и авторизация совпадают с MediaBoxPlayer: UTF-8 JSON-объект на строку
с завершающим `\n`, не более 1 MiB в запросе, максимум 16 одновременных клиентов.
Соединение без запросов закрывается через 30 секунд. Каждая команда содержит
`protocolVersion: 1` и `token` из файла `control.token`. Необязательное поле
`id` (строка, число или null) копируется в ответ. Адрес по умолчанию:
`127.0.0.1:17656`. Публикации событий нет; менеджер запрашивает `status`.

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
        "repeat":"off","error":""
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
Неизвестные поля отклоняются; URL и автоматическая передача файлов не поддерживаются.

Ошибки команд возвращаются с актуальным полным `status`:

```json
{"protocolVersion":1,"id":2,"ok":false,"error":{"code":"unknown_window","message":"The video window does not exist."},"status":{"application":"MediaBoxVPlayer","displays":[],"windows":[],"persistenceError":""}}
```

Основные коды: `invalid_request`, `invalid_arguments`, `unknown_command`,
`unknown_window`, `unknown_screen`, `window_limit`, `queue_limit`, `invalid_path`,
`empty_queue`, `persistence_error`. Последний означает, что команда применена в
памяти, но атомарная запись `windows.json` не удалась. Ошибки авторизации и
транспорта (`unauthorized`, `unsupported_protocol`, `invalid_json`,
`request_too_large`) не содержат состояние окон.

При отключении явно выбранного монитора связанные окна останавливаются и
скрываются с пустым `actualScreen`; `play` и `load` с `autoplay:true` возвращают
`unknown_screen`. Другие окна продолжают работу. После подключения окна
показываются без autoplay. Сохранённый `screen` остаётся прежним. Поведение
восстановления, исчезнувших файлов и локальных клавиш описано в [README](README.md).
