# Linux: пользовательский сервис systemd

MediaBoxPlayer работает без окна и остаётся в основном процессе: systemd следит за ним и перезапускает при ошибке. Для доступа к PipeWire/PulseAudio сервис запускается от пользователя, которому принадлежит аудиосессия. Системный сервис от root обычно не имеет доступа к этому аудиосерверу.

Сначала установите плеер с зависимостями в `/usr/local` или измените `ExecStart` в unit-файле на абсолютный путь к установленному исполняемому файлу. Затем, из корня репозитория:

```sh
mkdir -p ~/.config/systemd/user
cp MediaBoxPlayer/deploy/linux/mediaboxplayer.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now mediaboxplayer.service
systemctl --user status mediaboxplayer.service
journalctl --user -u mediaboxplayer.service -f
```

Данные и токен управления сохраняются в `~/.local/share/MediaBoxPlayer`; unit задаёт `UMask=0077`. По умолчанию управление доступно только через loopback. Путь к данным, адрес и порт можно заменить через `systemctl --user edit mediaboxplayer.service`:

```ini
[Service]
ExecStart=
ExecStart=/usr/local/bin/MediaBoxPlayer --data-dir %h/.local/share/MediaBoxPlayer --listen 127.0.0.1 --port 17655
```

После изменения выполните `systemctl --user daemon-reload` и `systemctl --user restart mediaboxplayer.service`. Если путь содержит пробелы, заключите соответствующий аргумент `ExecStart` в двойные кавычки.

Для старта без входа в систему администратор может включить `loginctl enable-linger ИМЯ_ПОЛЬЗОВАТЕЛЯ`. Аудиосервер пользователя также должен запускаться без графического входа; настройка зависит от дистрибутива. На машине без доступного аудиоустройства API остаётся доступен, а ошибка воспроизведения возвращается клиенту.

Остановка: `systemctl --user stop mediaboxplayer.service`. SIGTERM и SIGINT переводятся в завершение Qt через self-pipe; обработчик сигнала не вызывает Qt и не выполняет запись состояния. Очистка выполняется в обычном потоке приложения.

Удаление сервиса сохраняет каталог данных и токен. Очередь хранится в памяти;
после нового запуска Manager должен загрузить её заново:

```sh
systemctl --user disable --now mediaboxplayer.service
rm ~/.config/systemd/user/mediaboxplayer.service
systemctl --user daemon-reload
```
