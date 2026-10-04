# Установщик MediaBoxManager

Установщик собирается **Inno Setup 6** из готового Qt deploy-каталога. Основной сценарий сборки описан в [agent_build/README.md](../agent_build/README.md).

Из корня репозитория:

```powershell
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' `
    '/DAppVersion=1.2.8' `
    .\Installer\installer.iss
```

Параметры необязательны: значения по умолчанию — версия `1.2.8`, пакет `../agent_build/deploy/Release`, выход `Installer/bin`. Относительные пути разрешаются от каталога `installer.iss`. Результат — `MediaBoxManager-<версия>-Setup.exe`.

В пакете обязательны `bin/MediaBoxManager.exe`, `bin/MediaBoxPlayer.exe`, `bin/MediaBoxVPlayer.exe` и локальные библиотеки MSVC: `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`. Дерево deploy устанавливается без изменения структуры: `bin`, `plugins`, `translations`, Qt DLL, включая MultimediaWidgets и мультимедийный backend, `bin/qt.conf` и остальные локальные библиотеки среды MSVC. В пакет включаются только три перечисленных EXE и конфигурация Qt; остальные EXE и `.conf`, `control.token`, `windows.json`, `project.json` с его журналом и блокировкой, журналы работы и отладочные символы исключены. Это также предотвращает включение файлов, оставшихся в deploy от предыдущих сборок.

Установка выполняется с правами администратора в `%ProgramFiles%\MediaBox` (64-разрядный каталог Program Files), без страницы выбора каталога. Прежний каталог установки не используется. Общие ярлыки Manager и MediaBoxVPlayer в меню «Пуск» и на рабочем столе создаются всегда, без страницы выбора ярлыков. В названия входит версия: например, `MediaBoxManager 1.2.8` и `MediaBoxVPlayer 1.2.8`. На завершающей странице есть отмеченная галочка **«Запустить MediaBoxManager»**; приложение запускается от имени пользователя, открывшего установщик. В тихом режиме программа не запускается. Установщик не запускает MediaBoxVPlayer напрямую и не добавляет его в автозагрузку ОС. Запущенный Manager автоматически поднимает недоступные локальные аудио- и видеоплееры в режиме `--managed`.

Настройки Manager всегда хранятся в `%ProgramData%\MediaBox\MediaBoxManager\MediaBoxManager.conf`, журналы — в том же каталоге. Путь не зависит от установки в Program Files или запуска EXE из другой папки. Установщик предоставляет обычным пользователям изменение каталога Manager и общего каталога `media`. В корне `%ProgramData%\MediaBox` разрешены чтение и создание файлов/каталогов без наследования этих прав на плееры. Каталоги плееров установщик не создаёт: [скрипт службы](../MediaBoxPlayer/deploy/windows/README.md) создаёт `%ProgramData%\MediaBox\MediaBoxPlayer` с закрытым ACL. MediaBoxVPlayer создаёт `%ProgramData%\MediaBox\MediaBoxVPlayer` от имени пользователя графической сессии при первом запуске; отдельный `control.token` получает закрытые права. Видеоплеер сохраняет настройки окон и загруженные очереди в `windows.json`; именованные плейлисты хранятся в конфиге Manager. Параметр `--data-dir` переопределяет каталог данных видеоплеера.

При автоматическом запуске из Manager плееры получают отдельные каталоги `managed-player/<адрес>-<порт>` и `managed-vplayer/<адрес>-<порт>` внутри каталога настроек Manager. У видеоплеера там сохраняются `windows.json` и отдельные SQLite-базы окон в `runtimes`. Окна восстанавливаются; воспроизведение возобновляется только для сохранённого режима расписания. Аудиоплеер восстанавливает принятый выпуск и режим из `runtime.sqlite`. Эти файлы сохраняются после закрытия Manager и не включаются в установочный пакет. Также исключены `schedule-project.json` с блокировкой, `active.json`, каталоги `snapshots` и `publications`, файлы SQLite WAL/SHM.

При первом запуске Manager создаёт конфигурацию по умолчанию в своём каталоге настроек.

Деинсталлятор удаляет только зарегистрированные при установке файлы и ярлыки. Каталог `%PROGRAMDATA%\MediaBox`, настройки, `project.json`, журналы и другие созданные пользователем файлы сохраняются. MediaBoxPlayer входит в пакет как отдельное приложение; регистрация службы выполняется [отдельным скриптом](../MediaBoxPlayer/deploy/windows/README.md). MediaBoxVPlayer запускается только в пользовательской графической сессии; он не устанавливается как служба аудиоплеера. Подключение Manager к видеоплееру использует порт `17656` и его собственный токен.

Установочная эмблема и изображения мастера лежат в `assets`. Знак лампочки также используется в значке приложения `src/icons/app.ico` для Windows EXE и Qt. Экспорт из `assets/emblem.png` выполняется командой `python Installer/prepare-artwork.py` из корня репозитория (нужен Pillow). Лицензия подключается из `eula.rtf`.

Описание заключительной галочки: [Inno Setup: секция Run](https://jrsoftware.org/ishelp/topic_runsection.htm). Права установки: [PrivilegesRequired](https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm).
